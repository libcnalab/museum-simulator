// CNA extension laboratory: what CNA offers beyond XNA 4.0. DebugDraw and the CRT, colour-depth and ASCII
// post-process effects (CNA_CNAEXT), the engine statistics counters (CNA_DIAGNOSTICS), point-list drawing and
// debug markers, and the device extensions (CNA_DEVICES). Every room says plainly when its feature was not
// compiled into this build.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "CNA/Diagnostics/Diagnostics.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/MiniView.hpp"
#include "Rendering/RenderStates.hpp"

#ifdef CNA_CNAEXT
#include "CNA/Graphics/AsciiPostProcessEffect.hpp"
#include "CNA/Graphics/CRTEffect.hpp"
#include "CNA/Graphics/DebugDraw.hpp"
#include "CNA/Graphics/DepthEffect.hpp"
#include "CNA/Graphics/ShaderPackageEXT.hpp"
#endif
#ifdef CNA_DEVICES
#include "CNA/Devices/DisplayInfo.hpp"
#include "CNA/Devices/FileDialog.hpp"
#include "CNA/Devices/MessageBox.hpp"
#include "CNA/Devices/SystemTray.hpp"
#include "CNA/Devices/Locale.hpp"
#include "CNA/Devices/PowerInfo.hpp"
#include "CNA/Devices/SystemInfo.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { DebugDraw, Crt, Depth, Ascii, Stats, Points, Devices, Package };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"crt", Mode::Crt},     {"depth", Mode::Depth}, {"ascii", Mode::Ascii},
                                                         {"stats", Mode::Stats}, {"points", Mode::Points}, {"devices", Mode::Devices}, {"package", Mode::Package}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::DebugDraw;
}

std::string Fmt(double v, const char* f = "%.2f")
{
    char buf[48];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

class CnaExtLab final : public ScreenExhibit {
public:
    CnaExtLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::DebugDraw:
                p.AddToggle("Depth test (setDepthTested)", true, [this](bool v) { depthTest_ = v; });
                p.AddSlider("Sphere segments", 6.0f, 48.0f, static_cast<float>(segments_), [this](float v) { segments_ = static_cast<int>(v); }, "%.0f", 1.0f);
                p.AddToggle("Animate", true, [this](bool v) { animate_ = v; });
                p.AddReadout("Lines this frame", [this] { return std::to_string(lines_); });
                break;
            case Mode::Crt:
                p.AddSlider("Scanline intensity", 0.0f, 1.0f, scan_, [this](float v) { scan_ = v; });
                p.AddSlider("Curvature", 0.0f, 0.4f, curve_, [this](float v) { curve_ = v; });
                p.AddSlider("Vignette", 0.0f, 1.0f, vignette_, [this](float v) { vignette_ = v; });
                p.AddSlider("Mask intensity", 0.0f, 1.0f, mask_, [this](float v) { mask_ = v; });
                p.AddChoice("Mask type", {"None", "ApertureGrille", "ShadowMask"}, 1, [this](int v) { maskType_ = v; });
                break;
            case Mode::Depth:
                p.AddChoice("Mode", {"Color16Bit", "Color8Bit", "Grayscale4Bit", "Grayscale2Bit", "Grayscale1Bit", "Palette256", "Palette16"}, 0, [this](int v) { depthMode_ = v; });
                p.AddChoice("Dither", {"None", "Bayer4x4", "Bayer8x8"}, 0, [this](int v) { dither_ = v; });
                break;
            case Mode::Ascii:
                p.AddSlider("Cell size (px)", 4.0f, 24.0f, static_cast<float>(cell_), [this](float v) { cell_ = static_cast<int>(v); }, "%.0f", 1.0f);
                p.AddChoice("Quantize", {"BlackWhite", "Color"}, 1, [this](int v) { asciiColor_ = v == 1; });
                p.AddReadout("Grid", [this] { return std::to_string(gridCols_) + " x " + std::to_string(gridRows_) + " characters"; });
                break;
            case Mode::Stats:
                p.AddChoice("Runtime mode", {"Off", "Stats"}, 1, [this](int v) { SetStatsMode(v); });
                p.AddReadout("Build mode", [] { return std::string(BuildModeName()); });
                p.AddReadout("Metrics reported", [this] { return std::to_string(snapshot_.metrics.size()); });
                p.AddReadout("Resources tracked", [this] { return std::to_string(snapshot_.resources.size()); });
                break;
            case Mode::Points:
                p.AddSlider("Points", 1000.0f, 60000.0f, static_cast<float>(pointCount_), [this](float v) { pointCount_ = static_cast<int>(v); Regenerate(); }, "%.0f", 1000.0f);
                p.AddToggle("Animate", true, [this](bool v) { animate_ = v; });
                p.AddReadout("Primitives (engine counter)", [this] { return pointsMetric_; });
                break;
            case Mode::Package:
                p.AddChoice("Variants in the package", {"GLSL ES, GLSL desktop and WGSL", "GLSL ES only", "GLSL desktop only", "HLSL and MSL only", "WGSL only"}, 0, [this](int v) { packageChoice_ = v; packageDirty_ = true; });
                p.AddSlider("Tint amount", 0.0f, 1.0f, tint_, [this](float v) { tint_ = v; });
                p.AddReadout("Selected variant", [this] { return selectedLabel_; });
                p.AddReadout("This renderer's dialect", [this] { return std::string(S().device->GetShaderDialectEXT() == CNA::ShaderDialectEXT::GlslDesktop ? "GLSL desktop" : (S().device->GetShaderDialectEXT() == CNA::ShaderDialectEXT::GlslEs ? "GLSL ES" : "other")); });
                break;
            case Mode::Devices:
                p.AddReadout("Compiled in", [] {
#ifdef CNA_DEVICES
                    return std::string("yes (CNA_DEVICES)");
#else
                    return std::string("no");
#endif
                });
                break;
        }
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
        scene_ = std::make_unique<MiniView>(S(), 960, 540);
        GraphicsDevice& d = *S().device;
        try {
#ifdef CNA_CNAEXT
            if (mode_ == Mode::DebugDraw) debug_ = std::make_unique<CNA::Graphics::DebugDraw>(d);
            if (mode_ == Mode::Crt) crt_ = std::make_unique<CNA::Graphics::CRTEffect>(d);
            if (mode_ == Mode::Depth) depth_ = std::make_unique<CNA::Graphics::DepthEffect>(d);
            if (mode_ == Mode::Ascii) ascii_ = std::make_unique<CNA::Graphics::AsciiPostProcessEffect>(d);
#endif
        } catch (const std::exception& e) {
            error_ = e.what();
            Museum::Log("cnaext", "%s: %s", Title().c_str(), e.what());
        }
        if (mode_ == Mode::Points) {
            effect_ = std::make_unique<BasicEffect>(d);
            effect_->setVertexColorEnabledProperty(true);
            effect_->setLightingEnabledProperty(false);
            effect_->setTextureEnabledProperty(false);
            Regenerate();
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        if (animate_ || mode_ == Mode::Crt || mode_ == Mode::Depth || mode_ == Mode::Ascii) time_ += ctx.dt;
        if (mode_ == Mode::Stats) {
            sampleTimer_ -= ctx.dt;
            if (sampleTimer_ <= 0.0f) {
                sampleTimer_ = 0.25f;
                try { snapshot_ = CNA::Diagnostics::GetProvider().CaptureSnapshot(); } catch (const std::exception&) {}
            }
        }
        if (mode_ == Mode::Points) {
            sampleTimer_ -= ctx.dt;
            if (sampleTimer_ <= 0.0f) {
                sampleTimer_ = 0.5f;
                try { snapshot_ = CNA::Diagnostics::GetProvider().CaptureSnapshot(); } catch (const std::exception&) {}
                const auto it = std::find_if(snapshot_.metrics.begin(), snapshot_.metrics.end(), [](const CNA::Diagnostics::MetricSample& m) { return m.name.find("SubmittedPrimitives") != std::string::npos; });
                pointsMetric_ = it != snapshot_.metrics.end() ? std::to_string(it->value) + " (all drawing, last frame)" : "not reported by this build";
            }
        }
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (!scene_ || !screen_) return;
        const bool post = mode_ == Mode::Crt || mode_ == Mode::Depth || mode_ == Mode::Ascii || mode_ == Mode::Package;
        if (post) {
            const float a = time_ * 0.35f;
            const Vector3 c = R().centre;
            const Vector3 eye = c + Off(std::cos(a) * 5.0f, 2.4f, std::sin(a) * 5.0f);
            scene_->Render(ctx, Matrix::CreateLookAt(eye, c + Vector3(0, 0.5f, 0), Vector3::Up),
                           Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(55.0f), scene_->Aspect(), 0.1f, 60.0f), eye,
                           [this, c](const DrawContext& sub) { DrawDiorama(sub, S(), c + Vector3(0, 0.02f, 0), time_, 3.0f); }, Color(40, 60, 100, 255));
        }
        ScreenExhibit::RenderOffscreen(ctx);
    }

    void DrawExtra(const DrawContext& ctx) override
    {
        if (mode_ == Mode::DebugDraw) DrawDebug(ctx);
        if (mode_ == Mode::Points) DrawPoints(ctx);
    }

    ~CnaExtLab() override
    {
        if (mode_ == Mode::Stats) SetStatsMode(1);
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        Texture2D& white = *S().whiteTexture;
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        const bool post = mode_ == Mode::Crt || mode_ == Mode::Depth || mode_ == Mode::Ascii || mode_ == Mode::Package;
        if (post && Available()) {
            DrawPost(sb, white);
            return;
        }
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        text(Title(), 24, 12, Kit::kGold, 1.0f);
        if (!Available()) {
            text("This build of the museum was compiled without this CNA feature.", 24, 100, Kit::Rgba(255, 190, 150), 0.85f);
            text(Requirement(), 24, 150, Kit::kDim, 0.75f);
            sb.End();
            return;
        }
        switch (mode_) {
            case Mode::DebugDraw:
                text("DebugDraw draws wire boxes, spheres, frusta, crosses and lines in the room itself:", 24, 70, Kit::kText, 0.75f);
                text("look around the centre of this room. Lines this frame: " + std::to_string(lines_), 24, 110, Kit::kText, 0.75f);
                text("The depth test option makes the shapes hide behind the solid centrepiece, or show through it.", 24, 150, Kit::kDim, 0.7f);
                break;
            case Mode::Stats: DrawStats(sb, white); break;
            case Mode::Points:
                text(std::to_string(pointCount_) + " vertices drawn as PrimitiveType::PointListEXT in one DrawUserPrimitives call.", 24, 70, Kit::kText, 0.75f);
                text("The cloud floats in the middle of this room. Points are one pixel, so many are needed.", 24, 110, Kit::kDim, 0.7f);
                text("GraphicsDevice::SetStringMarkerEXT puts a label into the frame for graphics debuggers.", 24, 150, Kit::kDim, 0.7f);
                break;
            case Mode::Devices: DrawDevices(sb, white); break;
            default: break;
        }
        sb.End();
    }

private:
    [[nodiscard]] static const char* BuildModeName()
    {
        try {
            switch (CNA::Diagnostics::GetBuildMode()) {
                case CNA::Diagnostics::Mode::Off: return "Off";
                case CNA::Diagnostics::Mode::Stats: return "Stats";
                default: return "Full";
            }
        } catch (const std::exception&) {
            return "unknown";
        }
    }

    [[nodiscard]] bool Available() const
    {
        switch (mode_) {
            case Mode::DebugDraw:
            case Mode::Crt:
            case Mode::Depth:
            case Mode::Ascii:
            case Mode::Package:
#ifdef CNA_CNAEXT
                return true;
#else
                return false;
#endif
            case Mode::Devices:
#ifdef CNA_DEVICES
                return true;
#else
                return false;
#endif
            default: return true;
        }
    }
    [[nodiscard]] std::string Requirement() const
    {
        return mode_ == Mode::Devices ? "Configure CNA with -DCNA_DEVICES=ON (the museum's CMakeLists does by default)." : "Configure CNA with -DCNA_CNAEXT=ON (the museum's CMakeLists does by default).";
    }

    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"DebugDraw", "CRTEffect", "DepthEffect", "AsciiPostProcessEffect", "Engine statistics", "Point lists and debug markers", "Device extensions", "Shader packages"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::DebugDraw: return {"CNA's DebugDraw batches lines, boxes, spheres, frusta and crosses and draws them with one call. This room", "draws a few hundred lines every frame; the depth test toggle shows how they interact with solid geometry."};
            case Mode::Crt: return {"CRTEffect is a CNA ShaderEffect: scanlines, barrel curvature, vignette and a phosphor mask. It is applied by", "passing it to SpriteBatch.Begin while drawing the 3D scene that was first rendered to a texture."};
            case Mode::Depth: return {"DepthEffect reduces colour depth to 16 or 8 bits, 4, 2 or 1-bit grays, or a 256 or 16 colour palette, with", "optional ordered dithering. Retro looks from one SpriteBatch effect, no renderer-specific code."};
            case Mode::Ascii: return {"AsciiPostProcessEffect draws a texture as text: each cell becomes a character chosen from its brightness,", "in black and white or in colour. It owns its font atlas and SpriteBatch; you only hand it the source texture."};
            case Mode::Stats: return {"CNA counts its own work: draw calls, primitives, state changes, sprite submissions and every live graphics", "resource with its size. Pick Off to stop counting (the numbers freeze); this museum builds CNA with STATS."};
            case Mode::Points: return {"PrimitiveType.PointListEXT is a CNA extension: each vertex is one point, so a particle cloud or a star field", "is one draw call with no quads. All points here are submitted at once with DrawUserPrimitives."};
            case Mode::Package: return {"A ShaderPackageEXT carries the same shader in several languages. selectFor(device) picks the variant this renderer", "can run, or reports why none can, and ShaderEffect(device, package) builds the effect from it: one asset, many renderers."};
            default: return {"CNA_DEVICES adds queries beyond XNA 4.0: power, locale, CPU and memory, display scale and safe area. Some report", "unsupported on a desktop; the museum shows exactly what each call returns on this machine."};
        }
    }

    // --- post processing ---------------------------------------------------------------------------
    void DrawPost(SpriteBatch& sb, Texture2D& white)
    {
        if (!scene_ || !scene_->HasContent()) return;
#ifdef CNA_CNAEXT
        bool failed = !error_.empty();
        const Rectangle full(0, 0, 960, 540);
        try {
            if (mode_ == Mode::Crt && crt_) {
                crt_->setScanlineIntensity(scan_);
                crt_->setCurvature(curve_);
                crt_->setVignetteIntensity(vignette_);
                crt_->setMaskIntensity(mask_);
                crt_->setMaskType(maskType_ == 0 ? CNA::Graphics::CRTMaskType::None : (maskType_ == 1 ? CNA::Graphics::CRTMaskType::ApertureGrille : CNA::Graphics::CRTMaskType::ShadowMask));
                sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearClamp, nullptr, nullptr, crt_.get());
                sb.Draw(*scene_->Texture(), full, Color::White);
                sb.End();
            } else if (mode_ == Mode::Depth && depth_) {
                static const CNA::Graphics::DepthEffectMode modes[] = {CNA::Graphics::DepthEffectMode::Color16Bit,    CNA::Graphics::DepthEffectMode::Color8Bit,    CNA::Graphics::DepthEffectMode::Grayscale4Bit,
                                                                       CNA::Graphics::DepthEffectMode::Grayscale2Bit, CNA::Graphics::DepthEffectMode::Grayscale1Bit, CNA::Graphics::DepthEffectMode::Palette256,
                                                                       CNA::Graphics::DepthEffectMode::Palette16};
                static const CNA::Graphics::DitherMode dithers[] = {CNA::Graphics::DitherMode::None, CNA::Graphics::DitherMode::Bayer4x4, CNA::Graphics::DitherMode::Bayer8x8};
                depth_->setMode(modes[depthMode_]);
                depth_->setDitherMode(dithers[dither_]);
                sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr, depth_.get());
                sb.Draw(*scene_->Texture(), full, Color::White);
                sb.End();
            } else if (mode_ == Mode::Package) {
                BuildPackage();
                if (packageEffect_ && packageEffect_->IsEffectValid()) {
                    packageEffect_->Apply();
                    packageEffect_->SetUniformFloat("uAmount", tint_);
                    sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearClamp, nullptr, nullptr, packageEffect_.get());
                    sb.Draw(*scene_->Texture(), full, Color::White);
                    sb.End();
                } else {
                    failed = true;
                    error_ = packageNote_;
                }
            } else if (mode_ == Mode::Ascii && ascii_) {
                ascii_->setCellSize(cell_, cell_ * 2);
                ascii_->setQuantizeMode(asciiColor_ ? CNA::Graphics::AsciiQuantizeMode::Color : CNA::Graphics::AsciiQuantizeMode::BlackWhite);
                ascii_->Draw(*scene_->Texture(), full);
                ascii_->GetLastGridDimensions(gridCols_, gridRows_);
            } else {
                failed = true;
            }
        } catch (const std::exception& e) {
            failed = true;
            error_ = e.what();
        }
        if (mode_ == Mode::Package) {
            sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
            if (failed) sb.Draw(*scene_->Texture(), Rectangle(0, 0, 960, 540), Color::White);
            sb.Draw(white, Rectangle(0, 0, 960, 40 + 22 * static_cast<int>(packageLines_.size()) + 24), Color(0, 0, 0, 190));
            sb.DrawString(*S().font, "ShaderPackageEXT variants (" + packageNote_ + ")", Vector2(14, 8), Color(255, 226, 150, 255), 0.0f, Vector2(0, 0), 0.7f, SpriteEffects::None, 0.0f);
            float ly = 40.0f;
            for (const std::string& l : packageLines_) {
                sb.DrawString(*S().font, l, Vector2(24, ly), Color(230, 235, 245, 255), 0.0f, Vector2(0, 0), 0.55f, SpriteEffects::None, 0.0f);
                ly += 22.0f;
            }
            sb.End();
            return;
        }
        if (failed) {
            sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
            sb.Draw(*scene_->Texture(), Rectangle(0, 0, 960, 540), Color::White);
            sb.Draw(white, Rectangle(0, 470, 960, 70), Color(0, 0, 0, 200));
            sb.DrawString(*S().font, "this effect could not run here: " + (error_.empty() ? std::string("unavailable") : error_.substr(0, 60)), Vector2(14, 490), Color(255, 190, 150, 255), 0.0f, Vector2(0, 0), 0.7f, SpriteEffects::None, 0.0f);
            sb.End();
        }
#else
        (void)sb;
        (void)white;
#endif
    }

    // --- shader packages ------------------------------------------------------------------------------
    static const char* Vertex(bool es)
    {
        static const std::string esSrc = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
uniform mat4 projection;
out vec2 vTexCoord;
out vec4 vColor;
void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)";
        static const std::string desktopSrc = "#version 330 core\n" + esSrc.substr(esSrc.find('\n') + 1);
        return es ? esSrc.c_str() : desktopSrc.c_str();
    }
    // The package declares entry point "main" (the one name a packaged ShaderEffect accepts); the WebGPU renderer then looks for
    // vs_main and fs_main inside the WGSL itself.
    static constexpr const char* kWgslVertex = R"WGSL(
struct VOut { @builtin(position) position: vec4f, @location(0) uv: vec2f, @location(1) color: vec4f };
@vertex fn vs_main(@location(0) position: vec3f, @location(1) uv: vec2f, @location(2) color: vec4f) -> VOut {
    var o: VOut;
    o.position = vec4f(position, 1.0);
    o.uv = uv;
    o.color = color;
    return o;
}
)WGSL";
    static constexpr const char* kWgslFragment = R"WGSL(
struct U { uAmount: vec4f };
@group(0) @binding(0) var<uniform> u: U;
@group(0) @binding(1) var texSampler: sampler;
@group(0) @binding(2) var tex: texture_2d<f32>;
@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let c = textureSampleLevel(tex, texSampler, uv, 0.0) * color;
    return vec4f(mix(c.rgb, c.rgb * vec3f(1.0, 0.55, 0.25), u.uAmount.x), c.a);   // warm tint
}
)WGSL";
    static std::string Fragment(bool es)
    {
        const std::string body = R"(
in vec2 vTexCoord;
in vec4 vColor;
uniform sampler2D texture1;
uniform float uAmount;
out vec4 FragColor;
void main() {
    vec4 c = texture(texture1, vTexCoord) * vColor;
    FragColor = vec4(mix(c.rgb, c.rgb * vec3(1.0, 0.55, 0.25), uAmount), c.a);   // warm tint
}
)";
        return es ? "#version 300 es\nprecision highp float;" + body : "#version 330 core\n" + body;
    }
    void BuildPackage()
    {
#ifdef CNA_CNAEXT
        if (!packageDirty_) return;
        packageDirty_ = false;
        packageEffect_.reset();
        packageLines_.clear();
        using CNA::ShaderLanguageEXT;
        using CNA::ShaderStageEXT;
        using CNA::Graphics::ShaderCodeEXT;
        std::vector<ShaderCodeEXT> variants;
        const bool es = packageChoice_ == 0 || packageChoice_ == 1, desktop = packageChoice_ == 0 || packageChoice_ == 2, other = packageChoice_ == 3, wgsl = packageChoice_ == 0 || packageChoice_ == 4;
        if (es) {
            variants.emplace_back(ShaderLanguageEXT::GlslEs, ShaderStageEXT::Vertex, "main", "tint.vert (GLSL ES 3.00)", std::string(Vertex(true)));
            variants.emplace_back(ShaderLanguageEXT::GlslEs, ShaderStageEXT::Fragment, "main", "tint.frag (GLSL ES 3.00)", Fragment(true));
        }
        if (desktop) {
            variants.emplace_back(ShaderLanguageEXT::GlslDesktop, ShaderStageEXT::Vertex, "main", "tint.vert (GLSL 3.30)", std::string(Vertex(false)));
            variants.emplace_back(ShaderLanguageEXT::GlslDesktop, ShaderStageEXT::Fragment, "main", "tint.frag (GLSL 3.30)", Fragment(false));
        }
        if (wgsl) {
            variants.emplace_back(ShaderLanguageEXT::Wgsl, ShaderStageEXT::Vertex, "main", "tint.vert.wgsl", std::string(kWgslVertex));
            variants.emplace_back(ShaderLanguageEXT::Wgsl, ShaderStageEXT::Fragment, "main", "tint.frag.wgsl", std::string(kWgslFragment));
        }
        if (other) {
            variants.emplace_back(ShaderLanguageEXT::Hlsl, ShaderStageEXT::Vertex, "main", "tint.vs.hlsl", std::string("float4 main() : SV_Position { return 0; }"));
            variants.emplace_back(ShaderLanguageEXT::Hlsl, ShaderStageEXT::Fragment, "main", "tint.ps.hlsl", std::string("float4 main() : SV_Target { return 1; }"));
            variants.emplace_back(ShaderLanguageEXT::Msl, ShaderStageEXT::Fragment, "main", "tint.metal", std::string("fragment float4 main0() { return float4(1); }"));
        }
        CNA::Graphics::ShaderPackageEXT package(variants, {ShaderStageEXT::Vertex, ShaderStageEXT::Fragment});
        try {
            const auto selection = package.selectFor(*S().device);
            selectedLabel_ = selection.isUsable() ? std::string(selection.getLanguage() == ShaderLanguageEXT::GlslEs ? "GLSL ES" : (selection.getLanguage() == ShaderLanguageEXT::GlslDesktop ? "GLSL desktop" : (selection.getLanguage() == ShaderLanguageEXT::Wgsl ? "WGSL" : "other"))) : std::string("none usable");
            packageNote_ = selection.isUsable() ? std::string("selected ") + selectedLabel_ : selection.getDiagnostic();
            for (const ShaderCodeEXT& v : package.getVariants())
                packageLines_.push_back(std::string(v.getLanguage() == ShaderLanguageEXT::GlslEs ? "GLSL ES     " : (v.getLanguage() == ShaderLanguageEXT::GlslDesktop ? "GLSL desktop" : (v.getLanguage() == ShaderLanguageEXT::Hlsl ? "HLSL        " : (v.getLanguage() == ShaderLanguageEXT::Wgsl ? "WGSL        " : "MSL         ")))) + "  " + v.getSourceLabel() + "  " + std::to_string(v.getPayloadByteSize()) + " bytes");
            if (selection.isUsable()) {
                packageEffect_ = std::make_unique<ShaderEffect>(*S().device, package);
                // WGSL and SPIR-V have no loose uniforms: this declares the block uAmount lives in (ignored by the GLSL renderers).
                static const char* const names[] = {"uAmount"};
                static const int offsets[] = {0};
                packageEffect_->DeclareUniformBlockEXT(16, names, offsets, 1);
            }
        } catch (const std::exception& e) {
            packageNote_ = e.what();
            selectedLabel_ = "error";
            Museum::Log("package", "%s", e.what());
        }
#endif
    }

    // --- DebugDraw ---------------------------------------------------------------------------------
    void DrawDebug(const DrawContext& ctx)
    {
#ifdef CNA_CNAEXT
        if (!debug_) return;
        const Vector3 c = R().centre;
        const float t = animate_ ? time_ : 0.0f;
        RenderStates::SetOpaque3D(ctx.device);
        // A solid centrepiece the depth test can hide the lines behind.
        debug_->setDepthTested(depthTest_);
        debug_->begin(ctx.view.view, ctx.view.projection);
        const Vector3 mid = c + Vector3(0, 1.4f, 0);
        for (int i = 0; i < 8; ++i) {
            const float a = t * 0.6f + static_cast<float>(i) * MathHelper::TwoPi / 8.0f;
            const Vector3 p = mid + Vector3(std::cos(a) * 2.4f, std::sin(a * 2.0f) * 0.5f, std::sin(a) * 2.4f);
            const Color col = i % 3 == 0 ? Color(255, 120, 90, 255) : (i % 3 == 1 ? Color(110, 220, 140, 255) : Color(120, 170, 255, 255));
            debug_->addSphere(p, 0.35f, col, segments_);
            debug_->addBox(BoundingBox(p - Vector3(0.5f, 0.5f, 0.5f), p + Vector3(0.5f, 0.5f, 0.5f)), Color(240, 240, 120, 255));
            debug_->addLine(mid, p, Color(200, 200, 220, 255));
            debug_->addCross(p, 0.25f, Color::White);
        }
        debug_->addSphere(BoundingSphere(mid, 1.0f), Color(255, 255, 255, 255), segments_);
        const Matrix view = Matrix::CreateLookAt(mid + Vector3(std::cos(t) * 1.8f, 1.2f, std::sin(t) * 1.8f), mid, Vector3::Up);
        const Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(50.0f), 1.6f, 0.4f, 1.6f);
        debug_->addFrustum(BoundingFrustum(view * proj), Color(255, 200, 80, 255));
        for (int i = -4; i <= 4; ++i) {
            debug_->addLine(c + Off(static_cast<float>(i), 0.01f, -4.0f), c + Off(static_cast<float>(i), 0.01f, 4.0f), Color(90, 100, 130, 255));
            debug_->addLine(c + Off(-4.0f, 0.01f, static_cast<float>(i)), c + Off(4.0f, 0.01f, static_cast<float>(i)), Color(90, 100, 130, 255));
        }
        lines_ = static_cast<int>(debug_->getLineCount());
        debug_->end();
#else
        (void)ctx;
#endif
    }

    // --- point lists -------------------------------------------------------------------------------
    void Regenerate()
    {
        points_.clear();
        points_.reserve(static_cast<std::size_t>(pointCount_));
        std::uint32_t s = 12345;
        const auto rnd = [&s] {
            s = s * 1664525u + 1013904223u;
            return static_cast<float>(s >> 8) / 16777216.0f;
        };
        for (int i = 0; i < pointCount_; ++i) {
            const float arm = static_cast<float>(i % 3) * MathHelper::TwoPi / 3.0f;
            const float r = std::pow(rnd(), 0.7f) * 2.8f;
            const float a = arm + r * 1.6f + (rnd() - 0.5f) * 0.6f;
            const float h = (rnd() - 0.5f) * 0.35f * (1.2f - r / 3.0f);
            const float heat = 1.0f - r / 2.8f;
            points_.emplace_back(Vector3(std::cos(a) * r, h, std::sin(a) * r), Color(static_cast<int>(120 + 135 * heat), static_cast<int>(140 + 80 * heat), static_cast<int>(255 - 90 * heat), 255));
        }
    }
    void DrawPoints(const DrawContext& ctx)
    {
        if (!effect_ || points_.empty()) return;
        const Vector3 c = R().centre + Vector3(0, 1.6f, 0);
        const Matrix world = Matrix::CreateRotationY(animate_ ? time_ * 0.25f : 0.0f) * Matrix::CreateTranslation(c);
        RenderStates::SetOpaque3D(ctx.device);
        effect_->setWorldProperty(world);
        effect_->setViewProperty(ctx.view.view);
        effect_->setProjectionProperty(ctx.view.projection);
        ctx.device.SetStringMarkerEXT("museum: point cloud");
        try {
            for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty()) {
                pass.Apply();
                ctx.device.DrawUserPrimitives(PrimitiveType::PointListEXT, points_.data(), 0, static_cast<int>(points_.size()));
            }
        } catch (const std::exception& e) {
            Museum::Log("cnaext", "point list: %s", e.what());
            static bool reported = false;   // per-frame path: report the first failure only
            if (!reported) { reported = true; ReportError(std::string("point list draw: ") + e.what()); }
        }
    }

    // --- statistics --------------------------------------------------------------------------------
    static void SetStatsMode(int v)
    {
        try { (void)CNA::Diagnostics::SetRuntimeMode(v == 0 ? CNA::Diagnostics::Mode::Off : CNA::Diagnostics::Mode::Stats); } catch (const std::exception&) {}
    }
    void DrawStats(SpriteBatch& sb, Texture2D& white)
    {
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(std::string("runtime mode ") + (CNA::Diagnostics::GetRuntimeMode() == CNA::Diagnostics::Mode::Off ? "Off" : "Stats") + ", build mode " + BuildModeName() + ", frame " + std::to_string(snapshot_.currentFrameNumber), 24, 56, Kit::kDim, 0.65f);
        if (snapshot_.metrics.empty() && snapshot_.resources.empty()) {
            text("No metrics yet: this build reports none, or the runtime mode is Off.", 24, 100, Kit::kText, 0.75f);
            return;
        }
        std::vector<const CNA::Diagnostics::MetricSample*> metrics;
        for (const auto& m : snapshot_.metrics) metrics.push_back(&m);
        std::sort(metrics.begin(), metrics.end(), [](const auto* a, const auto* b) { return a->name < b->name; });
        float y = 90.0f;
        int shown = 0;
        for (const auto* m : metrics) {
            if (shown++ >= 14) break;
            sb.Draw(white, Rectangle(24, static_cast<int>(y), 430, 26), Color(24, 30, 46, 255));
            text(m->name, 32, y + 2, Kit::kText, 0.6f);
            text(std::to_string(m->value), 380, y + 2, Kit::Rgba(255, 210, 120), 0.6f);
            y += 29.0f;
        }
        // Resources by kind.
        static const char* kinds[] = {"Unknown", "Texture2D", "Texture3D", "TextureCube", "VertexBuffer", "IndexBuffer", "RenderTarget2D", "RenderTargetCube", "AudioVoice", "Custom"};
        std::map<int, std::pair<int, std::uint64_t>> byKind;
        for (const auto& r : snapshot_.resources) {
            auto& e = byKind[static_cast<int>(r.kind)];
            ++e.first;
            e.second += r.estimatedBytes;
        }
        text("Live graphics resources", 500, 90, Kit::kGold, 0.8f);
        float ry = 124.0f;
        for (const auto& [kind, e] : byKind) {
            text(std::string(kinds[std::min(kind, 9)]) + ": " + std::to_string(e.first) + "  (" + Fmt(static_cast<double>(e.second) / 1048576.0) + " MB)", 500, ry, Kit::kText, 0.65f);
            ry += 28.0f;
        }
        text("registered bytes: " + Fmt(static_cast<double>(snapshot_.registeredResourceBytes) / 1048576.0) + " MB", 500, ry + 10, Kit::kDim, 0.65f);
    }

    // --- devices -----------------------------------------------------------------------------------
    void DrawDevices(SpriteBatch& sb, Texture2D&)
    {
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        std::vector<std::string> lines;
#ifdef CNA_DEVICES
        try {
            lines.push_back("SystemInfo: " + std::to_string(CNA::Devices::SystemInfo::getLogicalCpuCoreCountProperty()) + " logical cores, " + std::to_string(CNA::Devices::SystemInfo::getSystemRamMegabytesProperty()) + " MB RAM");
        } catch (const std::exception& e) { lines.push_back(std::string("SystemInfo: ") + e.what()); }
        try {
            std::string l = "Locale preferred:";
            for (const auto& loc : CNA::Devices::Locale::getPreferredLocalesProperty()) l += " " + loc.Language + (loc.Country.empty() ? "" : "-" + loc.Country);
            lines.push_back(l);
        } catch (const std::exception& e) { lines.push_back(std::string("Locale: ") + e.what()); }
        try {
            const auto st = CNA::Devices::PowerInfo::getStateProperty();
            static const char* names[] = {"Error", "Unknown", "OnBattery", "NoBattery", "Charging", "Charged"};
            lines.push_back(std::string("PowerInfo: ") + names[std::min(static_cast<int>(st), 5)] + ", battery " + std::to_string(CNA::Devices::PowerInfo::getBatteryPercentProperty()) + " %, seconds left " + std::to_string(CNA::Devices::PowerInfo::getSecondsRemainingProperty()));
        } catch (const std::exception& e) { lines.push_back(std::string("PowerInfo: ") + e.what()); }
        try {
            lines.push_back("DisplayInfo: content scale " + Fmt(static_cast<double>(CNA::Devices::DisplayInfo::getContentScaleProperty(S().game->getWindowProperty()))));
            const Rectangle safe = CNA::Devices::DisplayInfo::getSafeAreaProperty(S().game->getWindowProperty());
            lines.push_back("DisplayInfo: safe area " + std::to_string(safe.X) + ", " + std::to_string(safe.Y) + "  " + std::to_string(safe.Width) + " x " + std::to_string(safe.Height));
        } catch (const std::exception& e) { lines.push_back(std::string("DisplayInfo: ") + e.what()); }
        try {
            // These open real windows or icons, so the museum only asks whether they are supported.
            const auto yn = [](bool v) { return v ? "supported" : "not supported"; };
            lines.push_back(std::string("MessageBox ") + yn(CNA::Devices::MessageBox::getIsSupportedProperty()) + ",  FileDialog " + yn(CNA::Devices::FileDialog::getIsSupportedProperty()) + ",  SystemTray " + yn(CNA::Devices::SystemTray::getIsSupportedProperty()));
            lines.push_back("(nothing is opened here: the museum never pops up a dialog)");
        } catch (const std::exception& e) { lines.push_back(std::string("dialogs: ") + e.what()); }
#endif
        float y = 70.0f;
        for (const std::string& l : lines) {
            text(l, 30, y, Kit::kText, 0.7f);
            y += 42.0f;
        }
    }

    Mode mode_;
    std::unique_ptr<MiniView> scene_;
    int packageChoice_ = 0;
    bool packageDirty_ = true;
    float tint_ = 0.8f;
    std::string selectedLabel_ = "-", packageNote_;
    std::vector<std::string> packageLines_;
    std::unique_ptr<ShaderEffect> packageEffect_;
    std::string error_;
    float time_ = 0.0f, sampleTimer_ = 0.0f;
    bool animate_ = true;
    // DebugDraw
    bool depthTest_ = true;
    int segments_ = 16, lines_ = 0;
    // CRT
    float scan_ = 0.35f, curve_ = 0.12f, vignette_ = 0.3f, mask_ = 0.35f;
    int maskType_ = 1, depthMode_ = 0, dither_ = 0, cell_ = 8, gridCols_ = 0, gridRows_ = 0;
    bool asciiColor_ = true;
    // stats / points
    CNA::Diagnostics::Snapshot snapshot_;
    int pointCount_ = 20000;
    std::vector<VertexPositionColor> points_;
    std::unique_ptr<BasicEffect> effect_;
    std::string pointsMetric_ = "-";
#ifdef CNA_CNAEXT
    std::unique_ptr<CNA::Graphics::DebugDraw> debug_;
    std::unique_ptr<CNA::Graphics::CRTEffect> crt_;
    std::unique_ptr<CNA::Graphics::DepthEffect> depth_;
    std::unique_ptr<CNA::Graphics::AsciiPostProcessEffect> ascii_;
#endif
};

}  // namespace

std::unique_ptr<Exhibit> MakeCnaExtLab(const RoomDef& room, Services& services)
{
    return std::make_unique<CnaExtLab>(room, services);
}
