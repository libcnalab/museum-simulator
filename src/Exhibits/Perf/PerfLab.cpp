// Performance laboratory: stress levels for the things that cost time in a CNA game, each with live numbers.
// The stress only runs while the visitor stands in the room, starts at a level that is comfortable on a
// software renderer, and backs off by itself when the museum's frame time stays above 100 ms.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CNA/Diagnostics/Diagnostics.hpp"
#include "CNA/GraphicsCapability.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBufferBinding.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Rendering/CompiledInstancing.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Dashboard, Objects, Sprites, State, Buffers, Targets, Overdraw, Particles };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"objects", Mode::Objects}, {"sprites", Mode::Sprites}, {"state", Mode::State}, {"buffers", Mode::Buffers},
                                                         {"targets", Mode::Targets}, {"overdraw", Mode::Overdraw}, {"particles", Mode::Particles}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Dashboard;
}

using Clock = std::chrono::steady_clock;

double MillisSince(Clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

std::string Fmt(double v, const char* f = "%.2f")
{
    char buf[48];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

struct Counts {
    int values[5];
};

class PerfLab final : public ScreenExhibit {
public:
    PerfLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))) {}

    ~PerfLab() override { RestoreGlobals(); }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        if (mode_ != Mode::Dashboard) {
            p.AddSlider("Stress level (0 low .. 4 extreme)", 0.0f, 4.0f, static_cast<float>(level_), [this](float v) { level_ = static_cast<int>(v); backedOff_ = false; }, "%.0f", 1.0f);
            p.AddReadout("Amount", [this] { return Amount(); });
        }
        switch (mode_) {
            case Mode::Dashboard:
                p.AddSlider("Extra delay per Update (ms)", 0.0f, 60.0f, delay_, [this](float v) { delay_ = v; S().mutableStats->injectedDelayMs = v; }, "%.0f", 1.0f);
                p.AddButton("Hitch: 40 ms for 30 frames", [this] { S().mutableStats->hitchFrames = 30; });
                p.AddReadout("Frame time (avg / worst)", [this] { return Fmt(avg_) + " / " + Fmt(worst_) + " ms"; });
                p.AddReadout("1% low frame rate", [this] { return Fmt(low1_, "%.0f") + " fps"; });
                break;
            case Mode::Objects:
                p.AddChoice("Method", {"One draw call per cube", "DrawInstancedPrimitives"}, 0, [this](int v) { instanced_ = v == 1; });
                p.AddReadout("Instancing", [this] { if (!instanceRefusal_.empty()) return std::string("reported yes, draw refused (see log)"); if (instancingFx_) return std::string("supported (compiled XNA effect)"); return std::string(S().device->SupportsCapability(CNA::GraphicsCapability::Instancing) ? "supported" : "NOT supported: falls back to one call each"); });
                p.AddReadout("CPU time to submit", [this] { return Fmt(cpuMs_) + " ms"; });
                break;
            case Mode::Sprites:
                p.AddChoice("Sort mode", {"Deferred", "Texture", "BackToFront"}, 0, [this](int v) { sort_ = v; });
                p.AddChoice("Textures used", {"1", "2", "8"}, 0, [this](int v) { textureChoice_ = v; });
                p.AddReadout("CPU time to batch", [this] { return Fmt(cpuMs_) + " ms"; });
                break;
            case Mode::State:
                p.AddToggle("Sort draws by texture", false, [this](bool v) { sortByState_ = v; });
                p.AddReadout("CPU time to submit", [this] { return Fmt(cpuMs_) + " ms"; });
                break;
            case Mode::Buffers:
                p.AddChoice("Upload method", {"DynamicVertexBuffer (Discard)", "DrawUserPrimitives", "Static buffer, no uploads"}, 0, [this](int v) { upload_ = v; });
                p.AddReadout("Uploaded per frame", [this] { return Fmt(static_cast<double>(UploadBytes()) / 1048576.0) + " MB"; });
                p.AddReadout("CPU time", [this] { return Fmt(cpuMs_) + " ms"; });
                break;
            case Mode::Targets:
                p.AddReadout("Render-target switches", [this] { return std::to_string(Metric("Graphics/RenderTargetChanges")); });
                p.AddReadout("CPU time", [this] { return Fmt(cpuMs_) + " ms"; });
                break;
            case Mode::Overdraw:
                p.AddReadout("Pixels shaded per frame", [this] { return Fmt(static_cast<double>(Count()) * 960.0 * 540.0 / 1.0e6) + " million"; });
                p.AddReadout("CPU time", [this] { return Fmt(cpuMs_) + " ms"; });
                break;
            case Mode::Particles:
                p.AddReadout("Update time", [this] { return Fmt(updateMs_) + " ms"; });
                p.AddReadout("Draw submit time", [this] { return Fmt(cpuMs_) + " ms"; });
                break;
        }
        p.AddReadout("Whole museum frame", [this] { return Fmt(static_cast<double>(S().stats->frameMilliseconds)) + " ms  (" + Fmt(static_cast<double>(S().stats->fps), "%.0f") + " fps)"; });
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
        GraphicsDevice& d = *S().device;
        fx_ = std::make_unique<BasicEffect>(d);
        fx_->EnableDefaultLighting();
        fx_->setTextureEnabledProperty(true);
        fx_->setTextureProperty(S().textures->White());
        flat_ = std::make_unique<BasicEffect>(d);
        flat_->setVertexColorEnabledProperty(true);
        flat_->setLightingEnabledProperty(false);
        flat_->setTextureEnabledProperty(false);
        if (mode_ == Mode::Objects) {
            instancingFx_ = CompiledInstancing::Load(d, S().contentRoot);
        }
        if (mode_ == Mode::Targets) {
            for (int i = 0; i < 32; ++i) targets_.push_back(std::make_unique<RenderTarget2D>(d, 128, 128, false, SurfaceFormat::Color, DepthFormat::None));
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        inRoom_ = ctx.playerInRoom;
        // Statistics.
        sampleTimer_ -= ctx.dt;
        if (sampleTimer_ <= 0.0f) {
            sampleTimer_ = 0.4f;
            try {
                const auto snap = CNA::Diagnostics::GetProvider().CaptureSnapshot();
                metrics_.clear();
                for (const auto& m : snap.metrics) metrics_[m.name] = m.value;
            } catch (const std::exception&) {}
        }
        // Frame-time summary from the loop's history.
        {
            const FrameStats& st = *S().stats;
            std::vector<float> h(st.frameHistory, st.frameHistory + 120);
            std::vector<float> nz;
            for (float v : h) if (v > 0.0f) nz.push_back(v);
            if (!nz.empty()) {
                double sum = 0;
                for (float v : nz) sum += static_cast<double>(v);
                avg_ = sum / static_cast<double>(nz.size());
                worst_ = static_cast<double>(*std::max_element(nz.begin(), nz.end()));
                std::sort(nz.begin(), nz.end());
                const float p99 = nz[std::min(nz.size() - 1, static_cast<std::size_t>(static_cast<double>(nz.size()) * 0.99))];
                low1_ = p99 > 0.0f ? 1000.0 / static_cast<double>(p99) : 0.0;
            }
        }
        // Back off when the museum is struggling.
        if (mode_ != Mode::Dashboard && inRoom_ && level_ > 0) {
            slow_ = S().stats->frameMilliseconds > 100.0f ? slow_ + 1 : 0;
            if (slow_ >= 20) {
                --level_;
                slow_ = 0;
                backedOff_ = true;
                Museum::Log("perf", "%s: frame time stayed above 100 ms, level lowered to %d", Title().c_str(), level_);
            }
        }
        if (mode_ == Mode::Particles && inRoom_) UpdateParticles(ctx.dt);
        if (mode_ == Mode::Dashboard) S().mutableStats->injectedDelayMs = delay_;
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (mode_ == Mode::Targets && inRoom_) RenderTargets(ctx);
        ScreenExhibit::RenderOffscreen(ctx);
    }

    void DrawExtra(const DrawContext& ctx) override
    {
        if (ctx.offscreen || !inRoom_) return;
        switch (mode_) {
            case Mode::Objects: DrawObjects(ctx); break;
            case Mode::State: DrawState(ctx); break;
            case Mode::Buffers: DrawBuffers(ctx); break;
            case Mode::Particles: DrawParticles(ctx); break;
            default: break;
        }
    }

    void OnLeave() override { RestoreGlobals(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        Texture2D& white = *S().whiteTexture;
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        if (mode_ == Mode::Sprites && inRoom_) DrawSprites(sb);
        if (mode_ == Mode::Overdraw && inRoom_) DrawOverdraw(sb);
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        rect(0, 0, 960, mode_ == Mode::Dashboard ? 540 : 70, Color(14, 18, 30, mode_ == Mode::Dashboard ? 255 : 200));
        text(Title(), 24, 12, Kit::kGold, 1.0f);
        if (mode_ != Mode::Dashboard) {
            std::string line = "level " + std::to_string(level_) + ": " + Amount() + "    museum frame " + Fmt(static_cast<double>(S().stats->frameMilliseconds)) + " ms";
            if (backedOff_) line += "   (backed off: the frame time was too high)";
            text(line, 24, 44, Kit::kText, 0.6f);
            if (!inRoom_) text("Stand in the room: the stress runs only while you are here.", 24, 90, Kit::kDim, 0.7f);
        }
        switch (mode_) {
            case Mode::Dashboard: DrawDashboard(sb, rect, text); break;
            case Mode::Targets: DrawTargetsScreen(sb); break;
            default: break;
        }
        if (mode_ != Mode::Dashboard) DrawCounters(rect, text);
        sb.End();
    }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Frame-time dashboard", "Objects: draw calls and instancing", "Sprites: SpriteBatch volume", "State changes and batching", "Buffer uploads", "Render-target switches", "Overdraw", "CPU particles"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Dashboard: return {"The graph is the last 120 Update times. Add a delay or trigger a hitch to see how a frame-time spike looks; the", "1% low is what players feel as stutter. Engine counters (CNA diagnostics) show what the whole museum submitted."};
            case Mode::Objects: return {"Each cube is a few triangles, so the cost is calls, not triangles. Switch to DrawInstancedPrimitives and the same", "cubes become one call. The CPU time is measured around the submission only; levels run 10 to 20,000 cubes."};
            case Mode::Sprites: return {"SpriteBatch collects sprites and draws them in batches split by texture and state. The sort mode decides the", "order (Texture groups by texture, BackToFront by depth); 8 textures break the batch most. Levels: 100 to 60,000."};
            case Mode::State: return {"Changing the texture between draws costs more than the draws. Sorting the same quads by texture first gives", "the same picture with far fewer texture binding changes: watch that counter drop. Levels: 50 to 10,000 draws."};
            case Mode::Buffers: return {"Moving vertices to the GPU each frame costs bandwidth. DynamicVertexBuffer with Discard avoids stalls,", "DrawUserPrimitives copies on every call, a static buffer uploads nothing. The surface here is rewritten every frame."};
            case Mode::Targets: return {"Each render-target switch ends a pass. Rendering many small targets per frame is how mirrors, shadow maps and", "post-processing work, and the switch counter shows the price. Levels: 1 to 32 targets of 128 x 128."};
            case Mode::Overdraw: return {"Overdraw is drawing the same pixels again and again: translucent layers, particles, UI. Every layer adds a full", "screen's worth of shading. Levels: 1 to 32 layers on the 960 x 540 screen; a software renderer feels this first."};
            default: return {"A CPU particle system updates every particle each frame and uploads a vertex per particle (drawn as one-pixel", "PointListEXT points here). Update time and upload time scale with the count: levels 1,000 to 250,000."};
        }
    }

    [[nodiscard]] int Count() const
    {
        static const int counts[][5] = {{10, 100, 1000, 5000, 20000}, {100, 1000, 5000, 20000, 60000}, {50, 200, 1000, 4000, 10000}, {1000, 10000, 50000, 200000, 600000},
                                        {1, 4, 8, 16, 32}, {1, 4, 8, 16, 32}, {1000, 10000, 50000, 100000, 250000}};
        const int row = mode_ == Mode::Objects ? 0 : (mode_ == Mode::Sprites ? 1 : (mode_ == Mode::State ? 2 : (mode_ == Mode::Buffers ? 3 : (mode_ == Mode::Targets ? 4 : (mode_ == Mode::Overdraw ? 5 : 6)))));
        int c = counts[row][std::clamp(level_, 0, 4)];
        // A call per cube beyond 5,000 would stall even good machines: cap the naive path.
        if (mode_ == Mode::Objects && !instanced_) c = std::min(c, 5000);
        return c;
    }
    [[nodiscard]] std::string Amount() const
    {
        static const char* unit[] = {"", "cubes", "sprites", "draws", "vertices", "render targets", "full-screen layers", "particles"};
        return std::to_string(Count()) + " " + unit[static_cast<int>(mode_)];
    }
    [[nodiscard]] long long Metric(const char* name) const
    {
        const auto it = metrics_.find(name);
        return it == metrics_.end() ? 0 : it->second;
    }
    [[nodiscard]] std::size_t UploadBytes() const { return upload_ == 2 ? 0 : static_cast<std::size_t>(Count()) * sizeof(VertexPositionColor); }

    void RestoreGlobals()
    {
        if (mode_ == Mode::Dashboard && S().mutableStats != nullptr) {
            S().mutableStats->injectedDelayMs = 0.0f;
            S().mutableStats->hitchFrames = 0;
        }
    }

    // --- common drawing ---------------------------------------------------------------------------
    template <typename Rect, typename Text>
    void DrawDashboard(SpriteBatch&, Rect& rect, Text& text)
    {
        const FrameStats& st = *S().stats;
        rect(24, 70, 912, 190, Kit::Rgba(18, 24, 38));
        const float budget = 16.67f, scale = 190.0f / 50.0f;
        rect(24, 70.0f + 190.0f - budget * scale, 912, 2, Kit::Rgba(110, 230, 150));
        text("16.7 ms (60 fps)", 30, 70.0f + 190.0f - budget * scale - 24, Kit::Rgba(110, 230, 150), 0.55f);
        for (int i = 0; i < 120; ++i) {
            const float v = std::min(50.0f, st.frameHistory[i]);
            const float h = v * scale;
            rect(24.0f + static_cast<float>(i) * 7.6f, 70.0f + 190.0f - h, 6.6f, h, v > 33.0f ? Kit::Rgba(235, 120, 110) : (v > 18.0f ? Kit::Rgba(255, 200, 90) : Kit::Rgba(110, 190, 255)));
        }
        text("frame time of the last 120 updates (ms), 50 ms at the top", 24, 266, Kit::kDim, 0.55f);
        text("average " + Fmt(avg_) + " ms     worst " + Fmt(worst_) + " ms     1% low " + Fmt(low1_, "%.0f") + " fps     running slowly: " + (st.runningSlowly ? "yes" : "no"), 24, 296, Kit::kText, 0.7f);
        DrawCounters(rect, text, 330.0f);
    }
    template <typename Rect, typename Text>
    void DrawCounters(Rect&, Text& text, float y = 0.0f)
    {
        const bool dash = y > 0.0f;
        if (!dash) y = 420.0f;
        const std::pair<const char*, const char*> names[] = {{"Graphics/DrawCalls", "draw calls"}, {"Graphics/EffectChanges", "effect changes"}, {"Graphics/TextureBindingChanges", "texture binds"},
                                                             {"Graphics/RenderTargetChanges", "target switches"}, {"Graphics/SpriteSubmissions", "sprites"}, {"Graphics/SubmittedPrimitives", "primitives"}};
        text("Engine counters, whole museum, last frame:", 24, y, Kit::kDim, 0.6f);
        float x = 24.0f;
        for (const auto& [metric, label] : names) {
            text(std::string(label) + " " + std::to_string(Metric(metric)), x, y + 30.0f, Kit::kText, 0.6f);
            x += 150.0f;
            if (x > 800.0f) {
                x = 24.0f;
                y += 28.0f;
            }
        }
        if (metrics_.empty()) text("(no counters: CNA was built without CNA_DIAGNOSTICS)", 24, y + 30.0f, Kit::Rgba(255, 190, 150), 0.6f);
    }

    // --- objects -----------------------------------------------------------------------------------
    void DrawObjects(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        const LitMesh& cube = S().shapes->Cube();
        const int count = Count();
        const int side = std::max(1, static_cast<int>(std::ceil(std::cbrt(static_cast<double>(count)))));
        const float spacing = 3.0f / static_cast<float>(side), size = spacing * 0.6f;
        const Vector3 origin = R().centre + Vector3(0, 3.2f, 0);
        const Matrix spin = Matrix::CreateRotationY(time_ * 0.3f);
        RenderStates::SetOpaque3D(device);
        device.setRasterizerStateProperty(RasterizerState::CullClockwise);
        fx_->setViewProperty(ctx.view.view);
        fx_->setProjectionProperty(ctx.view.projection);
        fx_->setDiffuseColorProperty(Vector3(0.45f, 0.75f, 1.0f));
        const auto t0 = Clock::now();
        const auto place = [&](int k) {
            const int x = k % side, y = (k / side) % side, z = k / (side * side);
            const Vector3 p((static_cast<float>(x) - static_cast<float>(side) * 0.5f) * spacing, (static_cast<float>(y) - static_cast<float>(side) * 0.5f) * spacing, (static_cast<float>(z) - static_cast<float>(side) * 0.5f) * spacing);
            return Matrix::CreateScale(size) * Matrix::CreateTranslation(p);
        };
        bool drawnInstanced = false;
        if (instanced_ && instanceRefusal_.empty() && device.SupportsCapability(CNA::GraphicsCapability::Instancing)) {
            if (instCount_ != count || !instV_) {
                std::vector<Matrix> m(static_cast<std::size_t>(count));
                for (int k = 0; k < count; ++k) m[static_cast<std::size_t>(k)] = place(k);
                const VertexDeclaration decl(64, {VertexElement(0, VertexElementFormat::Vector4, VertexElementUsage::BlendWeight, 0), VertexElement(16, VertexElementFormat::Vector4, VertexElementUsage::BlendWeight, 1),
                                                  VertexElement(32, VertexElementFormat::Vector4, VertexElementUsage::BlendWeight, 2), VertexElement(48, VertexElementFormat::Vector4, VertexElementUsage::BlendWeight, 3)});
                instV_ = std::make_unique<VertexBuffer>(device, decl, count, BufferUsage::WriteOnly);
                instV_->SetDataRaw(m.data(), count, 64);
                instCount_ = count;
            }
            const Matrix global = spin * Matrix::CreateTranslation(origin);
            if (instancingFx_) {
                CompiledInstancing::Apply(*instancingFx_, global * ctx.view.view * ctx.view.projection,
                                          Vector4(0.45f, 0.75f, 1.0f, 1.0f));
            } else {
                fx_->setWorldProperty(global);
                EffectBank::Apply(*fx_);
            }
            const std::vector<VertexBufferBinding> bindings = {VertexBufferBinding(cube.Vertices(), 0, 0), VertexBufferBinding(instV_.get(), 0, 1)};
            device.setIndicesProperty(cube.Indices());
            device.SetVertexBuffers(bindings);
            try {
                device.DrawInstancedPrimitives(PrimitiveType::TriangleList, 0, 0, cube.VertexCount(), 0, cube.TriangleCount(), count);
                drawnInstanced = true;
            } catch (const std::exception& e) {
                // FNA3D reports Instancing, but its stock effects declare no per-instance stream and refuse the draw.
                instanceRefusal_ = e.what();
                Museum::Log("perf", "DrawInstancedPrimitives refused: %s", e.what());
            }
            device.SetVertexBuffer(cube.Vertices());
        }
        if (!drawnInstanced) {
            for (int k = 0; k < count; ++k) {
                fx_->setWorldProperty(place(k) * spin * Matrix::CreateTranslation(origin));
                EffectBank::Apply(*fx_);
                cube.Draw(device);
            }
        }
        cpuMs_ = cpuMs_ * 0.9 + MillisSince(t0) * 0.1;
    }

    // --- sprites -----------------------------------------------------------------------------------
    void DrawSprites(SpriteBatch& sb)
    {
        static const char* names[] = {"checker", "brick", "marble", "metal", "wood", "stone", "grass", "carpet"};
        const int textures = textureChoice_ == 0 ? 1 : (textureChoice_ == 1 ? 2 : 8);
        static const SpriteSortMode modes[] = {SpriteSortMode::Deferred, SpriteSortMode::Texture, SpriteSortMode::BackToFront};
        const int count = Count();
        const auto t0 = Clock::now();
        sb.Begin(modes[sort_], &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        std::uint32_t seed = 7;
        for (int i = 0; i < count; ++i) {
            seed = seed * 1664525u + 1013904223u;
            const float fx = static_cast<float>((seed >> 8) & 0xFFFF) / 65536.0f;
            seed = seed * 1664525u + 1013904223u;
            const float fy = static_cast<float>((seed >> 8) & 0xFFFF) / 65536.0f;
            const float x = fx * 940.0f + std::sin(time_ + static_cast<float>(i) * 0.01f) * 6.0f, y = fy * 520.0f;
            Texture2D* tex = S().textures->Get(names[i % textures]);
            sb.Draw(*tex, Rectangle(static_cast<int>(x), static_cast<int>(y), 14, 14), std::nullopt, Color(255, 255, 255, 200), 0.0f, Vector2(0, 0), SpriteEffects::None, static_cast<float>(i % 100) / 100.0f);
        }
        sb.End();
        cpuMs_ = cpuMs_ * 0.9 + MillisSince(t0) * 0.1;
    }

    // --- state changes -----------------------------------------------------------------------------
    void DrawState(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        static const char* names[] = {"checker", "brick", "marble", "metal"};
        const LitMesh& quad = S().shapes->Panel();
        const int count = Count();
        const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count) * 1.6))));
        const int rows = (count + cols - 1) / cols;
        const float cell = std::min(5.0f / static_cast<float>(cols), 3.0f / static_cast<float>(rows));
        const Vector3 origin = R().centre + Vector3(0, 2.0f, 0);
        RenderStates::SetOpaque3D(device);
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        fx_->setViewProperty(ctx.view.view);
        fx_->setProjectionProperty(ctx.view.projection);
        fx_->setDiffuseColorProperty(Vector3(1, 1, 1));
        std::vector<int> order(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) order[static_cast<std::size_t>(i)] = i;
        if (sortByState_) std::stable_sort(order.begin(), order.end(), [](int a, int b) { return a % 4 < b % 4; });
        const auto t0 = Clock::now();
        for (const int i : order) {
            const int x = i % cols, y = i / cols;
            fx_->setTextureProperty(S().textures->Get(names[i % 4]));
            fx_->setWorldProperty(Matrix::CreateScale(cell * 0.9f) * Matrix::CreateTranslation(origin + Off((static_cast<float>(x) - static_cast<float>(cols) * 0.5f) * cell, (static_cast<float>(rows) * 0.5f - static_cast<float>(y)) * cell, 0.6f)));
            EffectBank::Apply(*fx_);
            quad.Draw(device);
        }
        cpuMs_ = cpuMs_ * 0.9 + MillisSince(t0) * 0.1;
    }

    // --- buffers -----------------------------------------------------------------------------------
    void DrawBuffers(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        const int count = std::max(6, Count() / 6 * 6);
        const int quads = count / 6;
        const int cols = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(quads))));
        if (static_cast<int>(verts_.size()) != count) {
            verts_.assign(static_cast<std::size_t>(count), VertexPositionColor());
            staticBuilt_ = false;
            dyn_.reset();
        }
        const auto t0 = Clock::now();
        const float cell = 4.0f / static_cast<float>(cols);
        const auto fill = [&](float t) {
            for (int q = 0; q < quads; ++q) {
                const int cx = q % cols, cy = q / cols;
                const float x = (static_cast<float>(cx) - static_cast<float>(cols) * 0.5f) * cell, y = (static_cast<float>(cy) - static_cast<float>(cols) * 0.5f) * cell * 0.6f;
                const float z = std::sin(t * 2.0f + static_cast<float>(cx) * 0.4f + static_cast<float>(cy) * 0.3f) * 0.3f;
                const Color c(static_cast<int>(120 + 100 * std::sin(static_cast<float>(cx) * 0.2f)), static_cast<int>(140 + 100 * std::sin(static_cast<float>(cy) * 0.2f)), 230, 255);
                const Vector3 a(x, y, z), b(x + cell, y, z), cc(x + cell, y + cell * 0.6f, z), d(x, y + cell * 0.6f, z);
                VertexPositionColor* v = &verts_[static_cast<std::size_t>(q) * 6];
                v[0] = VertexPositionColor(a, c);
                v[1] = VertexPositionColor(b, c);
                v[2] = VertexPositionColor(cc, c);
                v[3] = VertexPositionColor(a, c);
                v[4] = VertexPositionColor(cc, c);
                v[5] = VertexPositionColor(d, c);
            }
        };
        if (upload_ != 2 || !staticBuilt_) fill(upload_ == 2 ? 0.0f : time_);
        const Vector3 origin = R().centre + Vector3(0, 3.0f, 0);
        RenderStates::SetOpaque3D(device);
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        flat_->setViewProperty(ctx.view.view);
        flat_->setProjectionProperty(ctx.view.projection);
        flat_->setWorldProperty(Matrix::CreateTranslation(origin));
        EffectBank::Apply(*flat_);
        if (upload_ == 1) {
            device.DrawUserPrimitives(PrimitiveType::TriangleList, verts_.data(), 0, count / 3);
        } else {
            if (!dyn_) dyn_ = std::make_unique<DynamicVertexBuffer>(device, VertexPositionColor::getVertexDeclarationStatic(), count, BufferUsage::WriteOnly);
            if (upload_ == 0 || !staticBuilt_) dyn_->SetData(verts_.data(), 0, count, SetDataOptions::Discard);
            staticBuilt_ = true;
            device.SetVertexBuffer(dyn_.get());
            device.DrawPrimitives(PrimitiveType::TriangleList, 0, count / 3);
        }
        cpuMs_ = cpuMs_ * 0.9 + MillisSince(t0) * 0.1;
    }

    // --- render targets ----------------------------------------------------------------------------
    void RenderTargets(const DrawContext& ctx)
    {
        if (ctx.offscreen) return;
        GraphicsDevice& d = ctx.device;
        const int n = Count();
        const auto t0 = Clock::now();
        SpriteBatch& sb = *S().sprites;
        for (int i = 0; i < n && i < static_cast<int>(targets_.size()); ++i) {
            d.SetRenderTarget(targets_[static_cast<std::size_t>(i)].get());
            d.Clear(Color(30 + (i * 37) % 200, 60 + (i * 91) % 150, 120 + (i * 53) % 120, 255));
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearClamp, nullptr, nullptr);
            const float a = time_ * 2.0f + static_cast<float>(i);
            sb.Draw(*S().whiteTexture, Rectangle(static_cast<int>(64.0f + std::cos(a) * 40.0f) - 12, static_cast<int>(64.0f + std::sin(a) * 40.0f) - 12, 24, 24), Color::White);
            sb.End();
        }
        d.SetRenderTarget(nullptr);
        cpuMs_ = cpuMs_ * 0.9 + MillisSince(t0) * 0.1;
    }
    void DrawTargetsScreen(SpriteBatch& sb)
    {
        const int n = Count();
        const int cols = n <= 4 ? 4 : (n <= 8 ? 4 : 8);
        const int w = 912 / cols - 4;
        for (int i = 0; i < n && i < static_cast<int>(targets_.size()); ++i)
            sb.Draw(*targets_[static_cast<std::size_t>(i)], Rectangle(24 + (i % cols) * (w + 4), 100 + (i / cols) * (w + 4), w, w), Color::White);
    }

    // --- overdraw ----------------------------------------------------------------------------------
    void DrawOverdraw(SpriteBatch& sb)
    {
        const auto t0 = Clock::now();
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        const int layers = Count();
        for (int i = 0; i < layers; ++i) {
            const float a = static_cast<float>(i) / static_cast<float>(layers);
            sb.Draw(*S().whiteTexture, Rectangle(0, 0, 960, 540), Color(static_cast<int>(60 + 150 * a), static_cast<int>(80 + 100 * std::sin(a * 6.0f + time_)), static_cast<int>(200 - 120 * a), 40));
        }
        sb.End();
        cpuMs_ = cpuMs_ * 0.9 + MillisSince(t0) * 0.1;
    }

    // --- particles ---------------------------------------------------------------------------------
    struct P {
        Vector3 pos, vel;
        float life;
    };
    void UpdateParticles(float dt)
    {
        const int count = Count();
        const auto t0 = Clock::now();
        std::uint32_t seed = seed_;
        const auto rnd = [&seed] {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>(seed >> 8) / 16777216.0f;
        };
        if (static_cast<int>(parts_.size()) > count) parts_.resize(static_cast<std::size_t>(count));
        while (static_cast<int>(parts_.size()) < count) {
            const float a = rnd() * MathHelper::TwoPi, r = rnd() * 0.3f;
            parts_.push_back({Vector3(std::cos(a) * r, 0.0f, std::sin(a) * r), Vector3((rnd() - 0.5f) * 1.2f, 2.5f + rnd() * 3.0f, (rnd() - 0.5f) * 1.2f), rnd() * 3.0f});
        }
        for (P& p : parts_) {
            p.vel.Y -= 4.5f * dt;
            p.pos += p.vel * dt;
            p.life -= dt;
            if (p.pos.Y < 0.0f || p.life < 0.0f) {
                const float a = rnd() * MathHelper::TwoPi, r = rnd() * 0.3f;
                p.pos = Vector3(std::cos(a) * r, 0.0f, std::sin(a) * r);
                p.vel = Vector3((rnd() - 0.5f) * 1.2f, 2.5f + rnd() * 3.0f, (rnd() - 0.5f) * 1.2f);
                p.life = 2.0f + rnd();
            }
        }
        seed_ = seed;
        updateMs_ = updateMs_ * 0.9 + MillisSince(t0) * 0.1;
    }
    void DrawParticles(const DrawContext& ctx)
    {
        if (parts_.empty()) return;
        GraphicsDevice& device = ctx.device;
        const auto t0 = Clock::now();
        const int count = static_cast<int>(parts_.size());
        if (static_cast<int>(pv_.size()) != count) pv_.assign(static_cast<std::size_t>(count), VertexPositionColor());
        for (int i = 0; i < count; ++i) {
            const P& p = parts_[static_cast<std::size_t>(i)];
            const int h = std::clamp(static_cast<int>(p.pos.Y * 60.0f), 0, 255);
            pv_[static_cast<std::size_t>(i)] = VertexPositionColor(p.pos, Color(255, 120 + h / 2, 60 + h / 3, 255));
        }
        RenderStates::SetOpaque3D(device);
        flat_->setViewProperty(ctx.view.view);
        flat_->setProjectionProperty(ctx.view.projection);
        flat_->setWorldProperty(Matrix::CreateTranslation(R().centre + Vector3(0, 0.2f, 0)));
        EffectBank::Apply(*flat_);
        try {
            device.DrawUserPrimitives(PrimitiveType::PointListEXT, pv_.data(), 0, count);
        } catch (const std::exception& e) {
            Museum::Log("perf", "points: %s", e.what());
            static bool reported = false;   // per-frame path: report the first failure only
            if (!reported) { reported = true; ReportError(std::string("PointListEXT draw: ") + e.what()); }
        }
        cpuMs_ = cpuMs_ * 0.9 + MillisSince(t0) * 0.1;
    }

    Mode mode_;
    std::unique_ptr<BasicEffect> fx_, flat_;
    std::map<std::string, long long> metrics_;
    float time_ = 0.0f, sampleTimer_ = 0.0f, delay_ = 0.0f;
    bool inRoom_ = false, backedOff_ = false;
    int level_ = 0, slow_ = 0;
    double cpuMs_ = 0, updateMs_ = 0, avg_ = 0, worst_ = 0, low1_ = 0;
    // objects
    bool instanced_ = false;
    int instCount_ = 0;
    std::string instanceRefusal_;   // why DrawInstancedPrimitives was refused on this renderer; empty while it works
    std::unique_ptr<VertexBuffer> instV_;
    std::unique_ptr<Effect> instancingFx_;
    // sprites / state / buffers
    int sort_ = 0, textureChoice_ = 0, upload_ = 0;
    bool sortByState_ = false, staticBuilt_ = false;
    std::vector<VertexPositionColor> verts_, pv_;
    std::unique_ptr<DynamicVertexBuffer> dyn_;
    std::vector<std::unique_ptr<RenderTarget2D>> targets_;
    std::vector<P> parts_;
    std::uint32_t seed_ = 99;
};

}  // namespace

std::unique_ptr<Exhibit> MakePerfLab(const RoomDef& room, Services& services)
{
    return std::make_unique<PerfLab>(room, services);
}
