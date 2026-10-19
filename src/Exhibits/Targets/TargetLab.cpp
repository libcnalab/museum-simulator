// Render-target laboratory: RenderTarget2D from the basics to a control room with six feeds, feedback,
// a planar mirror, multiple render targets and format support. One class, seven rooms ("mode").
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <string>

#include "CNA/GraphicsCapability.hpp"
#include "CNA/RendererCapabilityProfile.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/ColorWriteChannels.hpp"
#include "Microsoft/Xna/Framework/Graphics/CubeMapFace.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/EnvironmentMapEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTargetBinding.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTargetCube.hpp"
#include "Exhibits/Shaders/ShaderSources.hpp"
#include "Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/MiniView.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Basic, LowRes, Control, Recursive, Mirror, Mrt, Formats, Cube, Bloom };

Mode ParseMode(const std::string& s)
{
    if (s == "lowres") return Mode::LowRes;
    if (s == "control") return Mode::Control;
    if (s == "recursive") return Mode::Recursive;
    if (s == "mirror") return Mode::Mirror;
    if (s == "mrt") return Mode::Mrt;
    if (s == "formats") return Mode::Formats;
    if (s == "cube") return Mode::Cube;
    if (s == "bloom") return Mode::Bloom;
    return Mode::Basic;
}

const char* kMrtVert = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
uniform mat4 projection;
out vec2 vTexCoord;
void main() { gl_Position = projection * vec4(aPos, 0.0, 1.0); vTexCoord = aTexCoord; }
)";
// Four outputs from one pass: grayscale, edges, source colour and posterized colour.
const char* kMrtFrag = R"(#version 300 es
precision highp float;
in vec2 vTexCoord;
uniform sampler2D texture1;
uniform vec2 uTexel;
layout(location = 0) out vec4 outGray;
layout(location = 1) out vec4 outEdge;
layout(location = 2) out vec4 outColor;
layout(location = 3) out vec4 outPoster;
float lum(vec2 uv) { return dot(texture(texture1, uv).rgb, vec3(0.299, 0.587, 0.114)); }
void main() {
    float g = lum(vTexCoord);
    float gx = lum(vTexCoord + vec2(uTexel.x, 0.0)) - lum(vTexCoord - vec2(uTexel.x, 0.0));
    float gy = lum(vTexCoord + vec2(0.0, uTexel.y)) - lum(vTexCoord - vec2(0.0, uTexel.y));
    vec3 source = texture(texture1, vTexCoord).rgb;
    outGray = vec4(vec3(g), 1.0);
    outEdge = vec4(vec3(clamp(length(vec2(gx, gy)) * 4.0, 0.0, 1.0)), 1.0);
    outColor = vec4(source, 1.0);
    outPoster = vec4(floor(source * 3.0 + 0.5) / 3.0, 1.0);
}
)";

class TargetLab final : public Exhibit {
public:
    TargetLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Basic:
                p.AddChoice("Size", {"64 x 36", "160 x 90", "320 x 180", "640 x 360"}, 3, [this](int v) { sizeIndex_ = v; dirty_ = true; });
                p.AddChoice("Depth format", {"None", "Depth16", "Depth24"}, 2, [this](int v) { depthIndex_ = v; dirty_ = true; });
                p.AddToggle("Mip levels", false, [this](bool v) { mips_ = v; dirty_ = true; });
                p.AddReadout("RenderTarget2D", [this] { return view_ ? std::to_string(view_->Target()->getWidthProperty()) + " x " + std::to_string(view_->Target()->getHeightProperty()) : std::string("-"); });
                p.AddReadout("Levels / usage", [this] { return view_ ? std::to_string(view_->Target()->getLevelCountProperty()) + " / " + std::to_string(static_cast<int>(view_->Target()->getRenderTargetUsageProperty())) : std::string("-"); });
                p.AddReadout("DepthStencilFormat", [this] { static const char* names[] = {"None", "Depth16", "Depth24", "Depth24Stencil8"}; return view_ ? std::string(names[std::clamp(static_cast<int>(view_->Target()->getDepthStencilFormatProperty()), 0, 3)]) : std::string("-"); });
                p.AddReadout("IsContentLost", [this] { return view_ ? std::string(view_->Target()->getIsContentLostProperty() ? "true" : "false") : std::string("-"); });
                break;
            case Mode::LowRes:
                p.AddSlider("Render width", 40.0f, 640.0f, static_cast<float>(lowW_), [this](float v) { lowW_ = static_cast<int>(v); dirty_ = true; }, "%.0f px", 8.0f);
                p.AddChoice("Upscale sampler", {"PointClamp (crisp)", "LinearClamp (soft)"}, 0, [this](int v) { linear_ = v == 1; });
                p.AddReadout("Pixels shaded", [this] { return std::to_string(lowW_ * lowW_ * 9 / 16); });
                break;
            case Mode::Control:
                for (int i = 0; i < 6; ++i) p.AddToggle("Feed " + std::to_string(i + 1) + " live", true, [this, i](bool v) { live_[static_cast<std::size_t>(i)] = v; });
                p.AddReadout("Render targets drawn per frame", [this] { return std::to_string(std::count(live_.begin(), live_.end(), true)) + " of 6"; });
                p.AddReadout("Target switches", [this] { return std::to_string(std::count(live_.begin(), live_.end(), true) * 2) + " per frame"; });
                break;
            case Mode::Recursive:
                p.AddSlider("Feedback strength", 0.0f, 0.98f, feedback_, [this](float v) { feedback_ = v; });
                p.AddSlider("Rotation per frame", -6.0f, 6.0f, rot_, [this](float v) { rot_ = v; }, "%.1f deg");
                p.AddHeading("The picture is drawn into itself every frame");
                break;
            case Mode::Mirror:
                p.AddSlider("Resolution", 128.0f, 1024.0f, static_cast<float>(mirrorSize_), [this](float v) { mirrorSize_ = static_cast<int>(v); dirty_ = true; }, "%.0f", 64.0f);
                p.AddToggle("Show without reflection", false, [this](bool v) { plain_ = v; });
                p.AddReadout("Technique", [] { return std::string("reflected camera + projective UVs"); });
                break;
            case Mode::Mrt:
                p.AddReadout("MultipleRenderTargets", [this] { return std::string(S().device->SupportsCapability(CNA::GraphicsCapability::MultipleRenderTargets) ? "supported" : "NOT supported here"); });
                p.AddToggle("Write target 1 (ColorWriteChannels1)", true, [this](bool v) { writeMrt_[0] = v; ResetMrtProbe(); });
                p.AddToggle("Write target 2 (ColorWriteChannels2)", true, [this](bool v) { writeMrt_[1] = v; ResetMrtProbe(); });
                p.AddToggle("Write target 3 (ColorWriteChannels3)", true, [this](bool v) { writeMrt_[2] = v; ResetMrtProbe(); });
                p.AddReadout("Active targets / renderer limit", [this] { return std::to_string(mrtTargetCount_) + " / " + mrtLimit_; });
                p.AddReadout("Lit pixels in targets 0 / 1 / 2 / 3", [this] { return mrtTargetPixels_; });
                p.AddReadout("GetRenderTargets() while bound", [this] { return boundTargets_; });
                p.AddReadout("ShaderEffect source", [this] { return effect_ && effect_->IsEffectValid() ? "compiled (" + dialect_ + ")" : std::string("unavailable"); });
                p.AddHeading("One pass, four distinct outputs");
                break;
            case Mode::Cube:
                p.AddChoice("Face size", {"16 x 16", "32 x 32", "64 x 64", "128 x 128", "256 x 256"}, 2, [this](int v) { cubeSize_ = 16 << v; dirty_ = true; });
                p.AddSlider("Refresh every N frames", 1.0f, 12.0f, static_cast<float>(cubeEvery_), [this](float v) { cubeEvery_ = static_cast<int>(v); }, "%.0f", 1.0f);
                p.AddSlider("EnvironmentMapAmount", 0.0f, 1.0f, cubeAmount_, [this](float v) { cubeAmount_ = v; });
                p.AddSlider("FresnelFactor (0 = a flat mirror)", 0.0f, 3.0f, cubeFresnel_, [this](float v) { cubeFresnel_ = v; });
                p.AddToggle("Mip chain on the cube", false, [this](bool v) { cubeMips_ = v; dirty_ = true; });
                p.AddReadout("RenderTargetCube", [this] { return cube_ ? std::to_string(cube_->getWidthProperty()) + " px per face, 6 faces, " + std::to_string(cube_->getLevelCountProperty()) + " level(s)" : cubeStatus_; });
                p.AddReadout("Faces drawn per refresh", [] { return std::string("6 (+X -X +Y -Y +Z -Z)"); });
                p.AddReadout("RenderTargetBinding target / CubeMapFace", [this] { return cubeBinding_; });
                break;
            case Mode::Bloom:
                p.AddSlider("Threshold (ReverseSubtract colour)", 0.2f, 0.95f, bloomThreshold_, [this](float v) { bloomThreshold_ = v; });
                p.AddSlider("Intensity (additive tint)", 0.0f, 1.0f, bloomIntensity_, [this](float v) { bloomIntensity_ = v; });
                p.AddChoice("Blur levels", {"1  (1/2 size)", "2  (1/4 size)", "3  (1/8 size)"}, 2, [this](int v) { bloomLevels_ = v + 1; });
                p.AddToggle("Bloom on", true, [this](bool v) { bloomOn_ = v; });
                p.AddReadout("Render targets per frame", [this] { return std::to_string(2 + bloomLevels_ + 1) + " (bright pass, " + std::to_string(bloomLevels_) + " levels, composite)"; });
                p.AddReadout("Shaders used", [] { return std::string("none: only BlendState and SpriteBatch"); });
                break;
            case Mode::Formats:
                p.AddHeading("SupportsSurfaceFormatAsRenderTargetEXT");
                for (int i = 0; i < 6; ++i) p.AddReadout(kFormats[i].name, [this, i] { return formatStatus_[static_cast<std::size_t>(i)]; });
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        GraphicsDevice& d = *S().device;
        dirty_ = true;
        if (mode_ == Mode::Control)
            for (int i = 0; i < 6; ++i) feeds_[static_cast<std::size_t>(i)] = std::make_unique<MiniView>(S(), 256, 144);
        if (mode_ == Mode::Mrt) {
            source_ = std::make_unique<MiniView>(S(), 480, 270);
            for (auto& target : mrtTargets_)
                target = std::make_unique<RenderTarget2D>(d, 480, 270, false, SurfaceFormat::Color, DepthFormat::None);
            const CNA::RendererLimitValue limit = d.GetRendererCapabilityProfileEXT().GetLimit(CNA::RendererLimit::MaxColorAttachments);
            mrtLimit_ = limit.known ? std::to_string(limit.value) : "unknown";
            mrtTargetCount_ = limit.known ? std::clamp(static_cast<int>(limit.value), 1, 4) : 2;
            // Source in the renderer's own dialect: GLSL ES, WGSL, or Vulkan GLSL for the SPIR-V renderer.
            try {
                if (d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::GlslEs, CNA::ShaderStageEXT::Fragment) || d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::GlslDesktop, CNA::ShaderStageEXT::Fragment)) {
                    effect_ = std::make_unique<ShaderEffect>(d, kMrtVert, kMrtFrag);
                } else if (d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::Wgsl, CNA::ShaderStageEXT::Fragment)) {
                    static const char* const names[] = {"uTexel"};
                    static const int offsets[] = {0};
                    effect_ = std::make_unique<ShaderEffect>(d, ShaderSources::kWgslVertex, ShaderSources::kMrtWgslFragment);
                    effect_->DeclareUniformBlockEXT(ShaderSources::kMrtBlockSize, names, offsets, 1);
                    dialect_ = "WGSL";
                } else if (d.GetShaderDialectEXT() == CNA::ShaderDialectEXT::SpirV && d.ExecutesShaderEffectSourceEXT()) {
                    effect_ = std::make_unique<ShaderEffect>(d, ShaderSources::kVulkanGlslVertex, ShaderSources::kMrtVulkanFragment);
                    dialect_ = "GLSL 450 for Vulkan";
                    packedUniforms_ = true;
                }
            } catch (const std::exception& e) {
                Museum::Log("targets", "MRT shader: %s", e.what());
            }
        }
        if (mode_ == Mode::Bloom) {
            source_ = std::make_unique<MiniView>(S(), 480, 270);
            const auto make = [&](int w, int h) { return std::make_unique<RenderTarget2D>(d, w, h, false, SurfaceFormat::Color, DepthFormat::None); };
            bright_ = make(480, 270);
            out_ = make(480, 270);
            for (int i = 0; i < 3; ++i) chain_[static_cast<std::size_t>(i)] = make(480 >> (i + 1), 270 >> (i + 1));
        }
        if (mode_ == Mode::Formats) ProbeFormats();
        if (mode_ == Mode::Cube) {
            try {
                env_ = std::make_unique<EnvironmentMapEffect>(d);
            } catch (const std::exception& e) {
                cubeStatus_ = std::string("EnvironmentMapEffect unavailable: ") + e.what();
            }
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        if (ctx.playerInRoom) {
            player_ = ctx.playerPosition;
            forward_ = ctx.playerForward;
        }
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        GraphicsDevice& d = ctx.device;
        const Vector3 stage = R().ToWorld(0.0f, 0.02f, 0.0f);
        const auto scene = [this, stage](const DrawContext& sub) { DrawDiorama(sub, S(), stage, time_, 2.4f); };
        const auto camera = [&](float angle, float radius, float height, Matrix& v, Vector3& eye) {
            eye = stage + Off(std::cos(angle) * radius, height, std::sin(angle) * radius);
            v = Matrix::CreateLookAt(eye, stage + Vector3(0, 0.4f, 0), Vector3::Up);
        };
        switch (mode_) {
            case Mode::Basic: {
                if (dirty_ || !view_) {
                    const int w[] = {64, 160, 320, 640}, h[] = {36, 90, 180, 360};
                    const DepthFormat depth[] = {DepthFormat::None, DepthFormat::Depth16, DepthFormat::Depth24};
                    view_ = std::make_unique<MiniView>(S(), w[sizeIndex_], h[sizeIndex_], depth[depthIndex_], mips_);
                    dirty_ = false;
                }
                Matrix v; Vector3 eye;
                camera(time_ * 0.4f, 5.0f, 2.5f, v, eye);
                view_->Render(ctx, v, Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(55.0f), view_->Aspect(), 0.1f, 60.0f), eye, scene);
                break;
            }
            case Mode::LowRes: {
                if (dirty_ || !view_) {
                    view_ = std::make_unique<MiniView>(S(), lowW_, std::max(9, lowW_ * 9 / 16));
                    dirty_ = false;
                }
                Matrix v; Vector3 eye;
                camera(time_ * 0.4f, 5.0f, 2.5f, v, eye);
                view_->Render(ctx, v, Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(55.0f), view_->Aspect(), 0.1f, 60.0f), eye, scene);
                break;
            }
            case Mode::Control: {
                for (std::size_t i = 0; i < 6; ++i) {
                    if (!live_[i] || !feeds_[i]) continue;
                    Matrix v; Vector3 eye;
                    camera(time_ * 0.2f * (static_cast<float>(i % 3) + 1.0f) + static_cast<float>(i) * 1.05f, 3.5f + static_cast<float>(i % 3) * 1.4f, 1.0f + static_cast<float>(i) * 0.7f, v, eye);
                    feeds_[i]->Render(ctx, v, Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(60.0f), feeds_[i]->Aspect(), 0.1f, 60.0f), eye, scene,
                                      Color(40 + 15 * static_cast<int>(i), 60, 100, 255));
                }
                break;
            }
            case Mode::Recursive: RenderRecursive(ctx, scene); break;
            case Mode::Mirror: RenderMirror(ctx, scene); break;
            case Mode::Mrt: RenderMrt(ctx, scene); break;
            case Mode::Cube: RenderCube(ctx, scene); break;
            case Mode::Bloom: RenderBloom(ctx); break;
            default: break;
        }
        (void)d;
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        const float h = 2.2f;
        switch (mode_) {
            case Mode::Basic: if (view_) { float yw; const Vector3 c = FarPoint(0.0f, h, yw); view_->DrawInRoom(ctx, c, yw, 6.4f); }; break;
            case Mode::LowRes:
                if (view_) {
                    // DrawInRoom sets the opaque 3D states first, so the upscale sampler goes in as its argument (set before it,
                    // the choice would be overwritten and the control would do nothing).
                    const SamplerState& upscale = linear_ ? SamplerState::LinearClamp : SamplerState::PointClamp;
                    { float yw; const Vector3 c = FarPoint(0.0f, h, yw); view_->DrawInRoom(ctx, c, yw, 6.4f, &upscale); };
                }
                break;
            case Mode::Control:
                for (int i = 0; i < 6; ++i)
                    if (feeds_[static_cast<std::size_t>(i)]) {
                        const float x = -2.45f + static_cast<float>(i % 3) * 2.45f, y = 3.15f - static_cast<float>(i / 3) * 1.45f;
                        { float yw; const Vector3 c = FarPoint(x, y, yw); feeds_[static_cast<std::size_t>(i)]->DrawInRoom(ctx, c, yw, 2.3f); }
                        { float yw; gizmos_.Label(FarPoint(x, y + 0.7f, yw), "CAM " + std::to_string(i + 1) + (live_[static_cast<std::size_t>(i)] ? "" : "  (frozen)"), Color(150, 255, 170, 255), 0.45f); }
                    }
                break;
            case Mode::Recursive: if (fbA_) { float yw; const Vector3 c = FarPoint(0.0f, h, yw); DrawTarget(ctx, fbA_.get(), c, yw, 6.4f, 16.0f / 9.0f, false); } break;
            case Mode::Mirror: DrawMirrorWall(ctx); break;
            case Mode::Mrt: {
                float yw = 0.0f;
                static constexpr float xs[] = {-3.8f, -1.9f, 0.0f, 1.9f, 3.8f};
                static constexpr const char* labels[] = {"source", "target 0: gray", "target 1: edges", "target 2: colour", "target 3: poster"};
                const Vector3 sourceCentre = FarPoint(xs[0], h, yw);
                if (source_) source_->DrawInRoom(ctx, sourceCentre, yw, 1.75f);
                gizmos_.Label(sourceCentre + Vector3(0, 0.8f, 0), labels[0], Color(255, 230, 150, 255), 0.4f);
                for (int i = 0; i < 4; ++i) {
                    const Vector3 centre = FarPoint(xs[i + 1], h, yw);
                    if (mrtTargets_[static_cast<std::size_t>(i)]) DrawTarget(ctx, mrtTargets_[static_cast<std::size_t>(i)].get(), centre, yw, 1.75f, 16.0f / 9.0f, false);
                    gizmos_.Label(centre + Vector3(0, 0.8f, 0), labels[i + 1], i < mrtTargetCount_ ? Color(255, 230, 150, 255) : Color(140, 140, 140, 255), 0.4f);
                }
                break;
            }
            case Mode::Formats: DrawFormatTiles(ctx); break;
            case Mode::Cube: DrawCubeReflection(ctx); break;
            case Mode::Bloom: DrawBloom(ctx); break;
        }
        RenderStates::SetOpaque3D(ctx.device);
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    struct FormatDef { const char* name; SurfaceFormat format; };
    static constexpr FormatDef kFormats[6] = {{"Color", SurfaceFormat::Color}, {"Bgr565", SurfaceFormat::Bgr565}, {"Bgra4444", SurfaceFormat::Bgra4444},
                                              {"HalfVector4", SurfaceFormat::HalfVector4}, {"Single", SurfaceFormat::Single}, {"Vector4", SurfaceFormat::Vector4}};

    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"RenderTarget2D basics", "Low-resolution rendering", "A control room of render targets", "Feedback: a screen that sees itself", "A planar mirror", "Multiple render targets", "Render target formats", "A live cube map", "Bloom from blending alone"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Basic: return {"A RenderTarget2D is a texture you can draw into. SetRenderTarget redirects drawing to it, SetRenderTarget(null)", "returns to the screen; the texture then goes on a quad. Change its size, depth buffer and mip levels."};
            case Mode::LowRes: return {"Rendering at a small size and scaling up is cheap and gives a retro look. Point sampling keeps hard pixels,", "linear blurs them. The scene is drawn into a render target as small as 40 pixels wide."};
            case Mode::Control: return {"Six cameras, six small render targets updated every frame, each a switch away from the back buffer and", "back. Freeze a feed and its target simply keeps the last frame: render targets keep their contents."};
            case Mode::Recursive: return {"Two render targets swap roles every frame: the previous picture is drawn (rotated, faded) into the new one", "under the live scene, so the screen shows a tunnel of itself. Exactly one frame of memory per level."};
            case Mode::Mirror: return {"A real mirror: a second camera sits where your reflection's eye would be (the reflected eye), draws the room", "into a render target, and the mirror shows it with texture coordinates taken from your own view of the wall."};
            case Mode::Cube: return {"A RenderTargetCube is a cube texture you can draw into, one face at a time: the scene is rendered six times from the", "sphere's centre (SetRenderTarget(cube, face)), then the sphere is drawn with EnvironmentMapEffect using the cube as its mirror."};
            case Mode::Bloom: return {"Bloom needs no shader: subtract a threshold with BlendFunction.ReverseSubtract to keep only the bright part, shrink it through a", "chain of smaller render targets with linear filtering (each step doubles it, so it spreads), then add the chain back over the scene."};
            case Mode::Mrt: return {"With MultipleRenderTargets a single draw writes four textures at once (SetRenderTargets). One ShaderEffect emits grayscale, edges,", "source colour and posterized colour; ColorWriteChannels1/2/3 independently suppress attachments 1, 2 and 3."};
            default: return {"Which surface formats can be rendered into depends on the renderer. Each format is queried, a target is created,", "cleared to a colour and read back: the answer is what the renderer really does, not what a table says."};
        }
    }

    void DrawTarget(const DrawContext& ctx, Texture2D* tex, const Vector3& centre, float yaw, float width, float aspect, bool flip)
    {
        RenderStates::SetOpaque3D(ctx.device);
        const Vector3 n(std::sin(yaw), 0, std::cos(yaw));
        const Vector3 right = Vector3::Normalize(Vector3::Cross(-n, Vector3::Up)) * width * (flip ? -1.0f : 1.0f);
        const Vector3 up(0, width / aspect, 0);
        if (flip) ctx.device.setRasterizerStateProperty(RasterizerState::CullNone);
        QuadDraw::Textured(ctx, tex, centre - right * 0.5f - up * 0.5f, right, up);
    }

    void RenderRecursive(const DrawContext& ctx, const std::function<void(const DrawContext&)>& scene)
    {
        GraphicsDevice& d = ctx.device;
        if (dirty_ || !fbA_) {
            fbA_ = std::make_unique<RenderTarget2D>(d, 640, 360, false, SurfaceFormat::Color, DepthFormat::Depth24, 0, RenderTargetUsage::PreserveContents);
            fbB_ = std::make_unique<RenderTarget2D>(d, 640, 360, false, SurfaceFormat::Color, DepthFormat::Depth24, 0, RenderTargetUsage::PreserveContents);
            dirty_ = false;
        }
        // Draw into B: the previous picture (A) rotated and faded, then the live scene on top.
        d.SetRenderTarget(fbB_.get());
        d.Clear(Color(20, 24, 40, 255), 1.0f);
        const SamplerState smp = SamplerState::LinearClamp;
        S().sprites->Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &smp, nullptr, nullptr);
        const float ang = MathHelper::ToRadians(rot_);
        S().sprites->Draw(*fbA_, Vector2(320, 180), std::nullopt, Color(static_cast<std::uint8_t>(255 * feedback_), static_cast<std::uint8_t>(255 * feedback_), static_cast<std::uint8_t>(255 * feedback_), static_cast<std::uint8_t>(255)),
                          ang, Vector2(320, 180), 0.9f, SpriteEffects::None, 0.0f);
        S().sprites->End();
        const Vector3 stage = R().ToWorld(0.0f, 0.02f, 0.0f);
        const Vector3 eye = stage + Off(std::cos(time_ * 0.5f) * 4.0f, 2.2f, std::sin(time_ * 0.5f) * 4.0f);
        const Matrix view = Matrix::CreateLookAt(eye, stage + Vector3(0, 0.4f, 0), Vector3::Up);
        const Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(55.0f), 16.0f / 9.0f, 0.1f, 60.0f);
        const SceneView sv(view, proj, eye);
        DrawContext sub{d, ctx.fx, sv, ctx.time, true};
        ctx.fx.SetCamera(view, proj);
        d.Clear(ClearOptions::DepthBuffer, Color(0, 0, 0, 0), 1.0f, 0);
        RenderStates::SetOpaque3D(d);
        scene(sub);
        d.SetRenderTarget(nullptr);
        std::swap(fbA_, fbB_);
    }

    void RenderMirror(const DrawContext& ctx, const std::function<void(const DrawContext&)>& scene)
    {
        GraphicsDevice& d = ctx.device;
        if (dirty_ || !mirror_) {
            mirror_ = std::make_unique<RenderTarget2D>(d, mirrorSize_, mirrorSize_ * 9 / 16, false, SurfaceFormat::Color, DepthFormat::Depth24);
            dirty_ = false;
        }
        // The mirror is the east wall: plane x = const, facing -X. The reflected eye mirrors the visitor's across it.
        const float wallX = R().centre.X + R().halfW - 0.1f;
        Vector3 eye = player_;
        Vector3 fwd = Vector3::Normalize(forward_ + Vector3(1e-5f, 0, 0));
        eye.X = 2.0f * wallX - eye.X;
        fwd.X = -fwd.X;
        const Matrix view = Matrix::CreateLookAt(eye, eye + fwd, Vector3::Up);
        const Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(70.0f), 16.0f / 9.0f, 0.2f, 60.0f);
        const SceneView sv(view, proj, eye);
        DrawContext sub{d, ctx.fx, sv, ctx.time, true};
        ctx.fx.SetCamera(view, proj);
        d.SetRenderTarget(mirror_.get());
        d.Clear(Color(30, 36, 50, 255), 1.0f);
        RenderStates::SetOpaque3D(d);
        d.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);   // the reflection flips triangle winding
        scene(sub);
        d.SetRenderTarget(nullptr);
        mirrorView_ = view;
        mirrorProj_ = proj;
        (void)ctx;
    }
    void DrawMirrorWall(const DrawContext& ctx)
    {
        GraphicsDevice& d = ctx.device;
        // The room's own copy of the scene (real objects), and the mirror quad on the east wall with projective UVs.
        DrawDiorama(ctx, S(), R().ToWorld(0.0f, 0.02f, 0.0f), time_, 2.4f);
        if (!mirror_) return;
        const float wallX = R().halfW - 0.12f, half = 3.0f, bottom = 0.3f, top = 4.4f;
        const int n = 16;
        const Matrix vp = ctx.view.view * ctx.view.projection;
        std::vector<VertexPositionColorTexture> v;
        const auto vertex = [&](float z, float y) {
            const Vector3 p = R().ToWorld(wallX, y, z);
            const Vector4 c = Vector4::Transform(Vector4(p, 1.0f), vp);
            const float w = std::max(c.W, 0.01f);
            // The mirror image seen from the real camera: the same screen position in the reflected camera's picture.
            return VertexPositionColorTexture(p, Color::White, Vector2((c.X / w) * 0.5f + 0.5f, 1.0f - ((c.Y / w) * 0.5f + 0.5f)));
        };
        (void)half;
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                const float z0 = -R().halfD + 1.0f + (2.0f * R().halfD - 2.0f) * static_cast<float>(i) / n, z1 = -R().halfD + 1.0f + (2.0f * R().halfD - 2.0f) * static_cast<float>(i + 1) / n;
                const float y0 = bottom + (top - bottom) * static_cast<float>(j) / n, y1 = bottom + (top - bottom) * static_cast<float>(j + 1) / n;
                const auto a = vertex(z0, y0), b = vertex(z1, y0), c = vertex(z1, y1), e = vertex(z0, y1);
                for (const auto& vv : {a, b, c, a, c, e}) v.push_back(vv);
            }
        BasicEffect& fx = ctx.fx.World();
        fx.setTextureProperty(plain_ ? S().textures->Get("metal") : mirror_.get());
        fx.setTextureEnabledProperty(true);
        fx.setVertexColorEnabledProperty(true);
        fx.setDiffuseColorProperty(Vector3(1, 1, 1));
        fx.setWorldProperty(Matrix::getIdentityProperty());
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        d.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        EffectBank::Apply(fx);
        d.DrawUserPrimitives(PrimitiveType::TriangleList, v.data(), 0, static_cast<int>(v.size()) / 3);
        gizmos_.Label(R().ToWorld(wallX, 4.7f, 0), "mirror", Color(255, 230, 150, 255), 0.55f);
    }

    void RenderCube(const DrawContext& ctx, const std::function<void(const DrawContext&)>& scene)
    {
        if (!env_) return;
        GraphicsDevice& d = ctx.device;
        if (dirty_ || !cube_) {
            try {
                cube_ = std::make_unique<RenderTargetCube>(d, cubeSize_, cubeMips_, SurfaceFormat::Color, DepthFormat::Depth24);
                cubeStatus_ = "ok";
            } catch (const std::exception& e) {
                cube_.reset();
                cubeStatus_ = std::string("not available: ") + std::string(e.what()).substr(0, 36);
                Museum::Log("targets", "RenderTargetCube failed: %s", e.what());
            }
            dirty_ = false;
            frame_ = 0;
        }
        if (!cube_ || (frame_++ % std::max(1, cubeEvery_)) != 0) return;
        struct Face { CubeMapFace face; Vector3 dir, up; };
        static const Face faces[6] = {{CubeMapFace::PositiveX, {1, 0, 0}, {0, 1, 0}},  {CubeMapFace::NegativeX, {-1, 0, 0}, {0, 1, 0}},
                                      {CubeMapFace::PositiveY, {0, 1, 0}, {0, 0, 1}},  {CubeMapFace::NegativeY, {0, -1, 0}, {0, 0, -1}},
                                      {CubeMapFace::PositiveZ, {0, 0, 1}, {0, 1, 0}},  {CubeMapFace::NegativeZ, {0, 0, -1}, {0, 1, 0}}};
        const Vector3 eye = SphereCentre();
        const Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::PiOver2, 1.0f, 0.1f, 60.0f);
        for (const Face& f : faces) {
            const Matrix view = Matrix::CreateLookAt(eye, eye + f.dir, f.up);
            const SceneView sceneView(view, proj, eye);
            const DrawContext sub{d, ctx.fx, sceneView, ctx.time, true};
            ctx.fx.SetCamera(view, proj);
            d.SetRenderTarget(cube_.get(), f.face);
            const std::vector<RenderTargetBinding> bound = d.GetRenderTargets();
            cubeBinding_ = bound.size() == 1 && bound[0].getRenderTargetProperty() == cube_.get() && bound[0].getCubeMapFaceProperty() == f.face
                ? "matches the bound face" : "MISMATCH";
            d.Clear(Color(120, 170, 225, 255), 1.0f);
            RenderStates::SetOpaque3D(d);
            scene(sub);
        }
        d.SetRenderTarget(static_cast<RenderTarget2D*>(nullptr));
    }

    [[nodiscard]] Vector3 SphereCentre() const { return R().ToWorld(0.0f, 1.25f, 0.0f); }

    void DrawCubeReflection(const DrawContext& ctx)
    {
        GraphicsDevice& d = ctx.device;
        DrawDiorama(ctx, S(), R().ToWorld(0.0f, 0.02f, 0.0f), time_, 2.4f);
        if (!env_ || !cube_) return;
        env_->setEnvironmentMapProperty(cube_.get());
        env_->setEnvironmentMapAmountProperty(cubeAmount_);
        env_->setTextureProperty(S().textures->White());
        env_->setDiffuseColorProperty(Vector3(0.9f, 0.9f, 0.95f));
        env_->setFresnelFactorProperty(cubeFresnel_);
        env_->setViewProperty(ctx.view.view);
        env_->setProjectionProperty(ctx.view.projection);
        env_->setWorldProperty(Matrix::CreateScale(1.8f) * Matrix::CreateTranslation(SphereCentre()));
        RenderStates::SetOpaque3D(d);
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        for (auto& pass : env_->getCurrentTechniqueProperty()->getPassesProperty()) pass.Apply();
        S().shapes->Sphere().Draw(d);
    }

    // Bloom with nothing but blending. Every step is a SpriteBatch draw into a render target:
    //   bright   = scene, then a flat grey drawn with ReverseSubtract (dest - source), which clamps at 0: only what exceeds the threshold survives
    //   chain[i] = the previous image drawn twice, additively, at half size with a linear sampler: the shrink blurs and the second draw doubles
    //   out      = the scene, then every chain level added back at full size (linear again, so each upscale blurs once more)
    void RenderBloom(const DrawContext& ctx)
    {
        GraphicsDevice& d = ctx.device;
        if (!source_ || !bright_ || !out_) return;
        const Vector3 stage = R().ToWorld(0.0f, 0.02f, 0.0f);
        const float a = time_ * 0.35f;
        const Vector3 eye = stage + Off(std::cos(a) * 5.0f, 2.4f, std::sin(a) * 5.0f);
        source_->Render(ctx, Matrix::CreateLookAt(eye, stage + Vector3(0, 0.5f, 0), Vector3::Up),
                        Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(55.0f), source_->Aspect(), 0.1f, 60.0f), eye,
                        [this, stage](const DrawContext& sub) {
                            DrawDiorama(sub, S(), stage, time_, 3.0f);
                            for (int i = 0; i < 3; ++i) {   // three lamps far brighter than anything else in the picture
                                const float t = time_ * 0.7f + static_cast<float>(i) * 2.09f;
                                gizmos_.SolidSphere(sub, stage + Off(std::cos(t) * 1.8f, 1.4f + 0.3f * std::sin(t * 1.7f), std::sin(t) * 1.8f), 0.16f, Vector3(1.0f, 0.95f, 0.8f));
                            }
                        }, Color(40, 60, 100, 255));
        SpriteBatch& sb = *S().sprites;
        Texture2D& white = *S().whiteTexture;
        const Rectangle full(0, 0, 480, 270);
        const auto into = [&](RenderTarget2D& rt, const Color& clear) {
            RenderStates::UnbindTextures(d);
            d.SetRenderTarget(&rt);
            d.setViewportProperty(Viewport(0, 0, rt.getWidthProperty(), rt.getHeightProperty()));
            d.Clear(clear);
        };
        BlendState subtract;
        subtract.setColorBlendFunctionProperty(BlendFunction::ReverseSubtract);
        subtract.setColorSourceBlendProperty(Blend::One);
        subtract.setColorDestinationBlendProperty(Blend::One);
        subtract.setAlphaBlendFunctionProperty(BlendFunction::Add);
        subtract.setAlphaSourceBlendProperty(Blend::Zero);
        subtract.setAlphaDestinationBlendProperty(Blend::One);
        // 1. the bright pass
        into(*bright_, Color::Black);
        sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearClamp, nullptr, nullptr);
        sb.Draw(*source_->Texture(), full, Color::White);
        sb.End();
        const std::uint8_t level = static_cast<std::uint8_t>(std::clamp(bloomThreshold_, 0.0f, 1.0f) * 255.0f);
        sb.Begin(SpriteSortMode::Deferred, &subtract, &SamplerState::PointClamp, nullptr, nullptr);
        sb.Draw(white, full, Color(level, level, level, static_cast<std::uint8_t>(255)));
        sb.End();
        // 2. the chain
        RenderTarget2D* previous = bright_.get();
        for (int i = 0; i < bloomLevels_; ++i) {
            RenderTarget2D& next = *chain_[static_cast<std::size_t>(i)];
            into(next, Color::Black);
            const Rectangle to(0, 0, next.getWidthProperty(), next.getHeightProperty());
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Additive, &SamplerState::LinearClamp, nullptr, nullptr);
            sb.Draw(*previous, to, Color::White);
            sb.Draw(*previous, to, Color::White);
            sb.End();
            previous = &next;
        }
        // 3. the composite
        into(*out_, Color::Black);
        sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearClamp, nullptr, nullptr);
        sb.Draw(*source_->Texture(), full, Color::White);
        sb.End();
        if (bloomOn_) {
            const std::uint8_t tint = static_cast<std::uint8_t>(std::clamp(bloomIntensity_, 0.0f, 1.0f) * 255.0f);
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Additive, &SamplerState::LinearClamp, nullptr, nullptr);
            for (int i = 0; i < bloomLevels_; ++i) sb.Draw(*chain_[static_cast<std::size_t>(i)], full, Color(tint, tint, tint, tint));
            sb.End();
        }
        d.SetRenderTarget(nullptr);
    }

    void DrawBloom(const DrawContext& ctx)
    {
        float yw = 0.0f;
        const Vector3 big = FarPoint(0.0f, 2.9f, yw);
        if (out_) DrawTarget(ctx, out_.get(), big, yw, 5.6f, 16.0f / 9.0f, false);
        const float x[3] = {-3.4f, 0.0f, 3.4f};
        const char* names[3] = {"scene", "bright pass", "last blur level"};
        Texture2D* tex[3] = {source_ ? source_->Texture() : nullptr, bright_.get(), chain_[static_cast<std::size_t>(std::max(0, bloomLevels_ - 1))].get()};
        for (int i = 0; i < 3; ++i) {
            const Vector3 c = FarPoint(x[i], 0.95f, yw);
            if (tex[i]) DrawTarget(ctx, tex[i], c, yw, 2.5f, 16.0f / 9.0f, false);
            gizmos_.Label(c + Vector3(0, 0.85f, 0), names[i], Color(255, 230, 150, 255), 0.45f);
        }
    }

    void RenderMrt(const DrawContext& ctx, const std::function<void(const DrawContext&)>& scene)
    {
        GraphicsDevice& d = ctx.device;
        const Vector3 stage = R().ToWorld(0.0f, 0.02f, 0.0f);
        const Vector3 eye = stage + Off(std::cos(time_ * 0.4f) * 4.5f, 2.4f, std::sin(time_ * 0.4f) * 4.5f);
        source_->Render(ctx, Matrix::CreateLookAt(eye, stage + Vector3(0, 0.4f, 0), Vector3::Up), Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(55.0f), source_->Aspect(), 0.1f, 60.0f), eye, scene);
        if (!effect_ || !effect_->IsEffectValid() || !d.SupportsCapability(CNA::GraphicsCapability::MultipleRenderTargets)) return;
        std::vector<RenderTargetBinding> bindings;
        for (int i = 0; i < mrtTargetCount_; ++i)
            bindings.emplace_back(mrtTargets_[static_cast<std::size_t>(i)].get());
        d.SetRenderTargets(bindings);
        const std::vector<RenderTargetBinding> bound = d.GetRenderTargets();
        bool same = bound.size() == bindings.size();
        for (std::size_t i = 0; same && i < bound.size(); ++i)
            same = bound[i].getRenderTargetProperty() == mrtTargets_[i].get();
        boundTargets_ = std::to_string(bound.size()) + " bindings, textures " + (same ? "as set" : "NOT as set");
        if (mrtFrames_ == 0) Museum::Log("targets", "GetRenderTargets while bound: %s", boundTargets_.c_str());
        d.Clear(Color::Black);
        effect_->Apply();
        if (packedUniforms_) effect_->SetUniformVec4("color", 1.0f / 480.0f, 1.0f / 270.0f, 0.0f, 0.0f);   // SDL_GPU: one fixed uniform block, names ignored
        else effect_->SetUniformVec2("uTexel", 1.0f / 480.0f, 1.0f / 270.0f);
        // Per-target write masks: the base property is attachment 0's; suffixes 1, 2 and 3
        // independently control attachments 1, 2 and 3.
        BlendState mrtBlend = BlendState::Opaque;
        mrtBlend.setColorWriteChannels1Property(writeMrt_[0] ? ColorWriteChannels::All : ColorWriteChannels::None);
        mrtBlend.setColorWriteChannels2Property(writeMrt_[1] ? ColorWriteChannels::All : ColorWriteChannels::None);
        mrtBlend.setColorWriteChannels3Property(writeMrt_[2] ? ColorWriteChannels::All : ColorWriteChannels::None);
        S().sprites->Begin(SpriteSortMode::Deferred, &mrtBlend, &SamplerState::LinearClamp, nullptr, nullptr, effect_.get());
        S().sprites->Draw(*source_->Texture(), Rectangle(0, 0, 480, 270), Color::White);
        S().sprites->End();
        d.SetRenderTarget(nullptr);
        // Once, a while in: read every active target back. This distinguishes an actual four-way
        // draw and each mask's None path from a renderer that fans out or writes attachment 0 only.
        if (++mrtFrames_ == 20) {
            try {
                std::array<int, 4> lit{-1, -1, -1, -1};
                std::vector<Color> px(static_cast<std::size_t>(480 * 270));
                for (int i = 0; i < mrtTargetCount_; ++i) {
                    mrtTargets_[static_cast<std::size_t>(i)]->GetData(px.data(), static_cast<int>(px.size()));
                    lit[static_cast<std::size_t>(i)] = 0;
                    for (const Color& c : px)
                        lit[static_cast<std::size_t>(i)] += (c.getRProperty() > 8 || c.getGProperty() > 8 || c.getBProperty() > 8) ? 1 : 0;
                }
                mrtTargetPixels_.clear();
                for (int i = 0; i < 4; ++i) {
                    if (i != 0) mrtTargetPixels_ += " / ";
                    mrtTargetPixels_ += lit[static_cast<std::size_t>(i)] < 0 ? "n/a" : std::to_string(lit[static_cast<std::size_t>(i)]);
                }
                Museum::Log("targets", "MRT target lit pixels: %s", mrtTargetPixels_.c_str());
            } catch (const std::exception& e) {
                mrtTargetPixels_ = std::string("unreadable: ") + std::string(e.what()).substr(0, 36);
            }
        }
    }

    void ResetMrtProbe()
    {
        mrtFrames_ = 0;
        mrtTargetPixels_ = "checking...";
    }

    void ProbeFormats()
    {
        GraphicsDevice& d = *S().device;
        for (int i = 0; i < 6; ++i) {
            std::string s;
            try {
                const bool claims = d.SupportsSurfaceFormatAsRenderTargetEXT(kFormats[i].format);
                RenderTarget2D rt(d, 16, 16, false, kFormats[i].format, DepthFormat::None);
                d.SetRenderTarget(&rt);
                d.Clear(Color(200, 100, 50, 255));
                d.SetRenderTarget(nullptr);
                const bool actual = rt.getFormatProperty() == kFormats[i].format;
                s = std::string(claims ? "supported" : "not claimed") + (actual ? ", stored as is" : ", falls back to another format");
            } catch (const std::exception& e) {
                d.SetRenderTarget(nullptr);
                s = std::string("throws: ") + std::string(e.what()).substr(0, 24);
            }
            formatStatus_[static_cast<std::size_t>(i)] = s;
        }
    }
    void DrawFormatTiles(const DrawContext& ctx)
    {
        for (int i = 0; i < 6; ++i)
            gizmos_.Label(R().ToWorld(-2.5f + static_cast<float>(i % 3) * 2.5f, 2.6f - static_cast<float>(i / 3) * 0.8f, 0), std::string(kFormats[i].name) + ": " + formatStatus_[static_cast<std::size_t>(i)], Color(255, 230, 150, 255), 0.42f);
        (void)ctx;
    }

    std::unique_ptr<RenderTarget2D> bright_, out_;
    std::array<std::unique_ptr<RenderTarget2D>, 3> chain_;
    float bloomThreshold_ = 0.6f, bloomIntensity_ = 1.0f;
    int bloomLevels_ = 3;
    bool bloomOn_ = true;
    std::unique_ptr<RenderTargetCube> cube_;
    std::unique_ptr<EnvironmentMapEffect> env_;
    std::string cubeStatus_ = "-";
    std::string cubeBinding_ = "waiting for a refresh";
    int cubeSize_ = 64, cubeEvery_ = 1, frame_ = 0;
    float cubeAmount_ = 1.0f, cubeFresnel_ = 0.0f;
    bool cubeMips_ = false;
    Mode mode_;
    Gizmos gizmos_;
    std::unique_ptr<MiniView> view_, source_;
    std::array<std::unique_ptr<MiniView>, 6> feeds_;
    std::array<bool, 6> live_{true, true, true, true, true, true};
    std::unique_ptr<RenderTarget2D> fbA_, fbB_, mirror_;
    std::array<std::unique_ptr<RenderTarget2D>, 4> mrtTargets_;
    std::unique_ptr<ShaderEffect> effect_;
    std::string dialect_ = "GLSL", mrtTargetPixels_ = "checking...", boundTargets_ = "-", mrtLimit_ = "unknown";
    int mrtFrames_ = 0, mrtTargetCount_ = 0;
    std::array<bool, 3> writeMrt_{true, true, true};
    bool packedUniforms_ = false;
    std::array<std::string, 6> formatStatus_{"-", "-", "-", "-", "-", "-"};
    Matrix mirrorView_, mirrorProj_;
    Vector3 player_{0, 1.65f, 0}, forward_{0, 0, -1};
    float time_ = 0.0f, feedback_ = 0.85f, rot_ = 2.0f;
    int sizeIndex_ = 3, depthIndex_ = 2, lowW_ = 160, mirrorSize_ = 512;
    bool dirty_ = true, mips_ = false, linear_ = false, plain_ = false;
};

}  // namespace

std::unique_ptr<Exhibit> MakeTargetLab(const RoomDef& room, Services& services)
{
    return std::make_unique<TargetLab>(room, services);
}
