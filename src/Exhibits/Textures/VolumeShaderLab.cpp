// Volume sampling laboratory: a Texture3D bound to a ShaderEffect with ShaderEffect.SetTexture(unit, Texture3D) and read by a
// `sampler3D` in the fragment shader. A 32 x 32 x 32 cube of colour (red grows along x, green along y, blue along z, with a white
// grid every eight texels in x and y) is sampled across a screen; the coordinate of the third axis is a slider, and the SamplerState's AddressU,
// AddressV and AddressW and filter decide what happens outside 0..1. A strip of slices beside the big picture shows the W axis at once.
// A renderer that does not take GLSL (WGSL, SPIR-V, no shaders at all) says so instead of showing a picture.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "CNA/ShaderLanguageEXT.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Exhibits/Shaders/ShaderSources.hpp"
#include "Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture3D.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureAddressMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureFilter.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

constexpr int kSize = 32;

// SpriteBatch's vertex layout: location 0 = pixel position, 1 = texcoord, 2 = colour; `projection` is set by CNA.
const char* const kVertex = R"(#version 300 es
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

// uRange widens the picture around its middle: 1 shows U and V from 0 to 1, 3 shows -1 to 2, so the addressing modes have something to do.
const char* const kFragment = R"(#version 300 es
precision highp float;
precision highp sampler3D;
uniform sampler3D VolumeSampler;
uniform float uW;
uniform float uRange;
in vec2 vTexCoord;
in vec4 vColor;
out vec4 FragColor;
void main() {
    vec2 uv = (vTexCoord - 0.5) * uRange + 0.5;
    FragColor = vec4(texture(VolumeSampler, vec3(uv, uW)).rgb, 1.0) * vColor;
}
)";

// The same picture in the WGSL a WEBGPU ShaderEffect takes: the lab's two floats in a 16-byte uniform block at binding 0,
// the sampler at 1 and the volume at 2 (CNA's WGSL binding convention, see ShaderSources).
const char* const kWgslFragment = R"WGSL(
struct U { uW: f32, uRange: f32, pad: vec2f };
@group(0) @binding(0) var<uniform> u: U;
@group(0) @binding(1) var texSampler: sampler;
@group(0) @binding(2) var tex: texture_3d<f32>;
@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let p = (uv - vec2f(0.5, 0.5)) * u.uRange + vec2f(0.5, 0.5);
    return vec4f(textureSampleLevel(tex, texSampler, vec3f(p, u.uW), 0.0).rgb, 1.0) * color;
}
)WGSL";

// And in GLSL 450 for SDL_GPU, whose ShaderEffect has one fixed uniform block: W and the range travel together in its vec4.
const char* const kVulkanFragment = R"GLSL(#version 450
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 0) uniform sampler3D VolumeSampler;
layout(set = 3, binding = 0) uniform PC {
    vec4 vpSize_pad;
    mat4 matrix;
    vec4 color;
    vec4 slot0_pad;
} pc;
void main() {
    vec2 uv = (vTexCoord - 0.5) * pc.color.y + 0.5;
    FragColor = vec4(texture(VolumeSampler, vec3(uv, pc.color.x)).rgb, 1.0) * vColor;
}
)GLSL";

const TextureAddressMode kModes[] = {TextureAddressMode::Clamp, TextureAddressMode::Wrap, TextureAddressMode::Mirror};
const char* const kModeNames[] = {"Clamp", "Wrap", "Mirror"};

// Where a coordinate lands after an address mode, in 0..1.
float Address(float t, int mode)
{
    switch (mode) {
        case 1: return t - std::floor(t);
        case 2: {
            const float f = t - 2.0f * std::floor(t * 0.5f);
            return f <= 1.0f ? f : 2.0f - f;
        }
        default: return std::clamp(t, 0.0f, 1.0f);
    }
}

class VolumeShaderLab final : public ScreenExhibit {
public:
    VolumeShaderLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ChooseDialect();
        ControlPanel& p = AddPanel("Sampling a Texture3D", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddSlider("W coordinate", -1.0f, 2.0f, w_, [this](float v) { w_ = v; }, "%.2f", 0.05f);
        p.AddToggle("Sweep W", true, [this](bool v) { sweep_ = v; });
        p.AddSlider("U and V range (1 = 0..1)", 1.0f, 3.0f, range_, [this](float v) { range_ = v; }, "%.1f", 0.1f);
        p.AddChoice("SamplerState.AddressU and V", {kModeNames[0], kModeNames[1], kModeNames[2]}, 0, [this](int v) { addressUV_ = v; });
        p.AddChoice("SamplerState.AddressW", {kModeNames[0], kModeNames[1], kModeNames[2]}, 1, [this](int v) { addressW_ = v; });
        p.AddChoice("SamplerState.Filter", {"Point", "Linear"}, 0, [this](int v) { linear_ = v == 1; });
        p.AddButton("Check: storage round trip, then shader sampling of slice 20", [this] { Check(); });
        p.AddReadout("Dialect", [this] { return supported_ ? std::string(DialectName()) : std::string("no shader source this renderer takes: nothing drawn"); });
        p.AddReadout("Shader compiled", [this] { return effect_ && effect_->IsEffectValid() ? std::string("yes") : (supported_ ? std::string("NO") : std::string("not run")); });
        p.AddReadout("W after AddressW", [this] { return Fmt(Address(w_, addressW_)) + " (slice " + std::to_string(Slice(w_, addressW_)) + " of 32)"; });
        p.AddReadout("Texture3D", [this] { return volume_ ? std::string("32 x 32 x 32 Color, SetTexture(0)") : std::string("not created"); });
        AddInfoSign(b, "Reading a volume in a shader",
                    {"No stock effect samples a Texture3D, but ShaderEffect.SetTexture(unit, Texture3D) binds one to a sampler3D in your own shader. The third texture coordinate picks the slice; SamplerState.AddressW",
                     "decides what a coordinate outside 0..1 means (Clamp holds the end slice, Wrap repeats, Mirror folds back), exactly as AddressU and AddressV do for the other two axes."});
    }

    void OnScreenLoaded() override
    {
        GraphicsDevice& d = *S().device;
        try {
            dummy_ = std::make_unique<Texture2D>(d, 1, 1, false, SurfaceFormat::Color);
            const Color white = Kit::Rgba(255, 255, 255);
            dummy_->SetData(&white, 1);
            if (!supported_) return;
            volume_ = std::make_unique<Texture3D>(d, kSize, kSize, kSize, false, SurfaceFormat::Color);
            std::vector<Color> texels(static_cast<std::size_t>(kSize) * kSize * kSize);
            for (int z = 0; z < kSize; ++z)
                for (int y = 0; y < kSize; ++y)
                    for (int x = 0; x < kSize; ++x) {
                        const bool grid = x % 8 == 0 || y % 8 == 0;
                        const auto level = [](int v) { return v * 255 / (kSize - 1); };
                        texels[(static_cast<std::size_t>(z) * kSize + y) * kSize + x] = grid ? Kit::Rgba(255, 255, 255) : Kit::Rgba(level(x), level(y), level(z));
                    }
            volume_->SetData(texels.data(), static_cast<int>(texels.size()));
            if (dialect_ == Dialect::Wgsl) {
                static const char* const names[] = {"uW", "uRange"};
                static const int offsets[] = {0, 4};
                effect_ = std::make_unique<ShaderEffect>(d, ShaderSources::kWgslVertex, kWgslFragment);
                effect_->DeclareUniformBlockEXT(16, names, offsets, 2);
            } else if (dialect_ == Dialect::Vulkan) {
                effect_ = std::make_unique<ShaderEffect>(d, ShaderSources::kVulkanGlslVertex, kVulkanFragment);
            } else {
                effect_ = std::make_unique<ShaderEffect>(d, kVertex, kFragment);
            }
            if (!effect_->IsEffectValid()) Museum::Log("volumeshader", "compile failed: %s", effect_->GetCompileErrorEXT().c_str());
        } catch (const std::exception& e) {
            volume_.reset();
            effect_.reset();
            Museum::Log("volumeshader", "unavailable: %s", e.what());
            ReportError(std::string("Texture3D or sampler3D shader: ") + e.what());
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        ScreenExhibit::Update(ctx);
        if (sweep_ && ctx.nearby) {
            time_ += ctx.dt;
            w_ = -1.0f + 3.0f * (0.5f + 0.5f * std::sin(time_ * 0.5f));
        }
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Text(sb, "Texture3D read by a sampler3D", 24, 6, 0.8f, Kit::kGold);
        Text(sb, std::string("W = ") + Fmt(w_) + "   AddressW " + kModeNames[addressW_] + " -> slice " + std::to_string(Slice(w_, addressW_)) + "   U,V " + kModeNames[addressUV_] + "   " + (linear_ ? "Linear" : "Point"), 24, 46, 0.55f, Kit::kDim);
        sb.End();
        if (!effect_ || !effect_->IsEffectValid() || !volume_ || !dummy_) {
            sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
            Text(sb, supported_ ? "the shader did not compile or the volume could not be made" : "this renderer runs no shader source: a volume shader cannot run here", 34, 200, 0.6f, Kit::Rgba(255, 190, 150));
            sb.End();
            return;
        }
        Quad(sb, w_, 24, 80, 380);
        // The W axis at once: eight slices from -0.5 to 1.5 beside the big picture.
        for (int i = 0; i < 8; ++i) {
            const float w = -0.5f + 2.0f * static_cast<float>(i) / 7.0f;
            const int col = i % 4, row = i / 4;
            const float x = 430.0f + static_cast<float>(col) * 128.0f, y = 80.0f + static_cast<float>(row) * 176.0f;
            Quad(sb, w, x, y, 120);
            sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
            Text(sb, "w " + Fmt(w), x, y + 122.0f, 0.5f, Kit::kDim);
            Text(sb, "slice " + std::to_string(Slice(w, addressW_)), x, y + 142.0f, 0.46f, Kit::Rgba(170, 220, 255));
            sb.End();
        }
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Text(sb, "red grows with x, green with y, blue with z; white lines every 8 texels in x and y", 24, 470, 0.5f, Kit::kDim);
        sb.End();
    }

private:
    static std::string Fmt(float v)
    {
        char buffer[16];
        std::snprintf(buffer, sizeof buffer, "%.2f", static_cast<double>(v));
        return buffer;
    }

    void Text(SpriteBatch& sb, const std::string& s, float x, float y, float scale, Color c)
    {
        sb.DrawString(*S().font, s, Vector2(x, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
    }

    // The slice a Point sample at coordinate w reads after the address mode (texel centres at (i + 0.5) / 32).
    static int Slice(float w, int mode) { return std::clamp(static_cast<int>(Address(w, mode) * static_cast<float>(kSize)), 0, kSize - 1); }

    void Quad(SpriteBatch& sb, float w, float x, float y, float size)
    {
        SamplerState sampler;
        sampler.setFilterProperty(linear_ ? TextureFilter::Linear : TextureFilter::Point);
        sampler.setAddressUProperty(kModes[addressUV_]);
        sampler.setAddressVProperty(kModes[addressUV_]);
        sampler.setAddressWProperty(kModes[addressW_]);
        effect_->SetTexture(0, *volume_);
        effect_->Apply();
        if (dialect_ == Dialect::Vulkan) {
            effect_->SetUniformVec4("color", w, range_, 0.0f, 0.0f);   // SDL_GPU: one fixed block, names ignored
        } else {
            effect_->SetUniformFloat("uW", w);
            effect_->SetUniformFloat("uRange", range_);
        }
        sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &sampler, nullptr, nullptr, effect_.get());
        sb.Draw(*dummy_, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), static_cast<int>(size)), Rectangle(0, 0, 1, 1), Color::White, 0.0f, Vector2(0.0f, 0.0f), SpriteEffects::None, 0.0f);
        sb.End();
    }

    // Storage and sampling are checked apart: GetData must return every texel SetData stored, and the shader drawing slice 20
    // (Point, Clamp) into a 32 x 32 target must put texel (x, y, 20) at pixel (x, y): red = x, green = y, blue = 20 levels.
    void Check()
    {
        const auto check = [this](const std::string& name, const std::string& expected, const std::string& got) {
            Museum::Log("check", "%s %s: %s (expected %s, got %s)", R().key.c_str(), name.c_str(), expected == got ? "PASS" : "FAIL", expected.c_str(), got.c_str());
        };
        if (!volume_ || !effect_ || !effect_->IsEffectValid() || !dummy_) {
            Museum::Log("check", "%s volume checks: SKIP (%s)", R().key.c_str(), supported_ ? "the shader or the volume could not be made" : "this renderer runs no shader source");
            return;
        }
        const auto level = [](int v) { return v * 255 / (kSize - 1); };
        GraphicsDevice& d = *S().device;
        try {
            std::vector<Color> back(static_cast<std::size_t>(kSize) * kSize * kSize);
            volume_->GetData(back.data(), static_cast<int>(back.size()));
            int wrong = 0;
            for (int z = 0; z < kSize; ++z)
                for (int y = 0; y < kSize; ++y)
                    for (int x = 0; x < kSize; ++x) {
                        const bool grid = x % 8 == 0 || y % 8 == 0;
                        const Color want = grid ? Kit::Rgba(255, 255, 255) : Kit::Rgba(level(x), level(y), level(z));
                        const Color got = back[(static_cast<std::size_t>(z) * kSize + y) * kSize + x];
                        wrong += got.getPackedValueProperty() == want.getPackedValueProperty() ? 0 : 1;
                    }
            check("Texture3D.GetData returns what SetData stored", "0 of 32768 texels different", std::to_string(wrong) + " of 32768 texels different");
        } catch (const std::exception& e) {
            Museum::Log("check", "%s Texture3D.GetData: SKIP (%s)", R().key.c_str(), std::string(e.what()).substr(0, 80).c_str());
        }
        try {
            RenderTarget2D target(d, kSize, kSize, false, SurfaceFormat::Color, DepthFormat::None);
            d.SetRenderTarget(&target);
            d.Clear(Color::Black);
            const float w = (20.0f + 0.5f) / static_cast<float>(kSize);
            const int linear = linear_, uv = addressUV_, wmode = addressW_;
            const float range = range_;
            linear_ = false;
            addressUV_ = 0;
            addressW_ = 0;
            range_ = 1.0f;
            SpriteBatch batch(d);
            Quad(batch, w, 0.0f, 0.0f, static_cast<float>(kSize));
            linear_ = linear;
            addressUV_ = uv;
            addressW_ = wmode;
            range_ = range;
            d.SetRenderTarget(nullptr);
            std::vector<Color> px(static_cast<std::size_t>(kSize * kSize));
            target.GetData(px.data(), static_cast<int>(px.size()));
            int wrong = 0;
            for (int y = 0; y < kSize; ++y)
                for (int x = 0; x < kSize; ++x) {
                    const bool grid = x % 8 == 0 || y % 8 == 0;
                    const Color want = grid ? Kit::Rgba(255, 255, 255) : Kit::Rgba(level(x), level(y), level(20));
                    const Color got = px[static_cast<std::size_t>(y * kSize + x)];
                    if (std::abs(got.getRProperty() - want.getRProperty()) > 2 || std::abs(got.getGProperty() - want.getGProperty()) > 2 || std::abs(got.getBProperty() - want.getBProperty()) > 2) ++wrong;
                }
            check(std::string("the ") + DialectName() + " shader samples slice 20 texel for texel", "0 of 1024 pixels off by more than 2", std::to_string(wrong) + " of 1024 pixels off by more than 2");
        } catch (const std::exception& e) {
            d.SetRenderTarget(nullptr);
            Museum::Log("check", "%s shader sampling: SKIP (%s)", R().key.c_str(), std::string(e.what()).substr(0, 80).c_str());
        }
    }

    // The source is chosen in the renderer's own dialect: GLSL (OpenGL family, SOFTWARE), WGSL (WEBGPU), GLSL 450 (SDL_GPU).
    void ChooseDialect()
    {
        GraphicsDevice& d = *S().device;
        supported_ = d.ExecutesShaderEffectSourceEXT();
        if (!supported_) return;
        if (d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::GlslEs, CNA::ShaderStageEXT::Fragment) || d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::GlslDesktop, CNA::ShaderStageEXT::Fragment))
            dialect_ = Dialect::Glsl;
        else if (d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::Wgsl, CNA::ShaderStageEXT::Fragment))
            dialect_ = Dialect::Wgsl;
        else if (d.GetShaderDialectEXT() == CNA::ShaderDialectEXT::SpirV)
            dialect_ = Dialect::Vulkan;
        else
            supported_ = false;
        Museum::Log("volumeshader", "dialect %s", supported_ ? DialectName() : "none: the renderer takes no source this room has");
    }
    [[nodiscard]] const char* DialectName() const
    {
        return dialect_ == Dialect::Wgsl ? "WGSL (texture_3d)" : dialect_ == Dialect::Vulkan ? "GLSL 450 for SDL_GPU (sampler3D)" : "GLSL ES 3.00 (sampler3D)";
    }
    enum class Dialect { Glsl, Wgsl, Vulkan };
    Dialect dialect_ = Dialect::Glsl;

    std::unique_ptr<Texture3D> volume_;
    std::unique_ptr<Texture2D> dummy_;
    std::unique_ptr<ShaderEffect> effect_;
    float w_ = 0.3f, range_ = 1.0f, time_ = 0.0f;
    int addressUV_ = 0, addressW_ = 1;
    bool linear_ = false, sweep_ = true, supported_ = false;
};

}  // namespace

std::unique_ptr<Exhibit> MakeVolumeShaderLab(const RoomDef& room, Services& services)
{
    return std::make_unique<VolumeShaderLab>(room, services);
}
