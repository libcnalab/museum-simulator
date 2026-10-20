// Compiled XNA effects: Effect(device, bytes) loads the binary a Windows XNA project's effect compiler produced (an .fxb, the
// payload of an Effect asset). CNA reflects it (parameters, techniques, passes) and runs it where the renderer can: the capability
// CompiledEffects says whether this one can. The effect here is a two-tap sprite pixel shader; SpriteBatch supplies the rest.
#include <algorithm>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#ifdef MUSEUM_PIPELINE
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/EffectImporter.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/CompiledEffectContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/EffectProcessor.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentCompiler.hpp"
#endif
#include "Microsoft/Xna/Framework/Graphics/EffectParameterClass.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameter.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

#ifdef MUSEUM_PIPELINE
namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace Compiler = Microsoft::Xna::Framework::Content::Pipeline::Serialization::Compiler;
namespace Processors = Microsoft::Xna::Framework::Content::Pipeline::Processors;
#endif

const char* ClassName(EffectParameterClass c)
{
    switch (c) {
        case EffectParameterClass::Scalar: return "Scalar";
        case EffectParameterClass::Vector: return "Vector";
        case EffectParameterClass::Matrix: return "Matrix";
        case EffectParameterClass::Object: return "Object";
        default: return "Struct";
    }
}
const char* TypeName(EffectParameterType t)
{
    switch (t) {
        case EffectParameterType::Single: return "Single";
        case EffectParameterType::Int32: return "Int32";
        case EffectParameterType::Bool: return "Bool";
        case EffectParameterType::Texture2D: return "Texture2D";
        case EffectParameterType::String: return "String";
        default: return "other";
    }
}

class CompiledFxLab final : public ScreenExhibit {
public:
    CompiledFxLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Compiled effect", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_PIPELINE
        p.AddChoice("Effect pipeline input", {"precompiled .fxb", "source .fx via fxc"}, 0, [this](int v) {
            sourceEffect_ = v == 1;
            LoadPixelEffect();
        });
        p.AddChoice("EffectProcessor.DebugMode", {"Auto", "Debug", "Optimize"}, 0, [this](int v) {
            debugMode_ = v;
            if (sourceEffect_) LoadPixelEffect();
        });
        p.AddToggle("EffectProcessor.Defines MUSEUM_EFFECT=1", false, [this](bool v) {
            useDefine_ = v;
            if (sourceEffect_) LoadPixelEffect();
        });
#endif
        p.AddToggle("Draw through the effect", true, [this](bool v) { use_ = v; });
        p.AddSlider("Weight (parameter)", 0.0f, 1.0f, weight_, [this](float v) { weight_ = v; }, "%.2f");
        p.AddSlider("Offset.X (parameter, texels)", -24.0f, 24.0f, offset_.X, [this](float v) { offset_.X = v; }, "%.1f", 0.5f);
        p.AddSlider("Offset.Y (parameter, texels)", -24.0f, 24.0f, offset_.Y, [this](float v) { offset_.Y = v; }, "%.1f", 0.5f);
        p.AddChoice("Picture", {"logo", "checker", "circuit"}, 0, [this](int v) { picture_ = v; });
        p.AddChoice("VertexSamplerStates[2]", {"PointClamp", "PointWrap"}, 0, [this](int v) { vertexWrap_ = v == 1; });
        p.AddSlider("Vertex texture height", 0.1f, 0.5f, vertexAmount_, [this](float v) { vertexAmount_ = v; });
        p.AddReadout("CompiledEffects capability", [this] { return capability_; });
        p.AddReadout("Effect", [this] { return status_; });
        p.AddReadout("Vertex texture effect", [this] { return vertexStatus_; });
        p.AddButton("Check: reflection, the pixel shader's arithmetic, vertex texture addressing", [this] { checkPending_ = true; });
        p.AddReadout("VertexTextures[2] during draw", [this] { return vertexBinding_; });
        p.AddReadout("Parameters", [this] { return parameters_; });
        p.AddReadout("Techniques", [this] { return techniques_; });
        p.AddReadout("Read back (GetValue*)", [this] { return readBack_; });
        AddInfoSign(b, "Compiled XNA Effects",
                    {"An XNA project compiles its .fx files into Effect assets. The first choice wraps an existing .fxb in an XNB; the second runs EffectImporter and EffectProcessor, which needs Microsoft's legacy fxc (set CNA_FXC and, on Linux, CNA_FXC_LAUNCHER=wine), then writes and loads the XNB.",
                     "The lower graph is genuine vs_3_0 texture sampling: its sampler has no state assignment, so GraphicsDevice.VertexTextures[2] and VertexSamplerStates[2] are authoritative.",
                     "Switch PointClamp / PointWrap: samples beyond both texture edges clamp or wrap without reapplying the pass (float vertex textures are point-filtered in XNA)."});
    }

    void OnScreenLoaded() override
    {
        compiledSupported_ = S().device->SupportsCapability(CNA::GraphicsCapability::CompiledEffects);
        capability_ = compiledSupported_ ? "true" : "false: this renderer cannot run compiled effects";
        LoadPixelEffect();
        try {
            std::ifstream in(S().contentRoot + "/Effects/vertextexture.fxb", std::ios::binary);
            const std::vector<char> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (raw.empty()) throw std::runtime_error("Content/Effects/vertextexture.fxb is missing");
            const std::vector<SharpRuntime::bytecs> bytes(raw.begin(), raw.end());
            vertexEffect_ = std::make_unique<Effect>(*S().device, bytes);
            vertexStatus_ = std::to_string(bytes.size()) + " bytes loaded";
            BuildVertexTexture();
        } catch (const std::exception& e) {
            vertexStatus_ = std::string("refused: ") + std::string(e.what()).substr(0, 68);
            Museum::Log("fx", "vertex texture effect: %s", e.what());
        }
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        Texture2D* tex = S().textures->Get(picture_ == 0 ? "logo" : picture_ == 1 ? "checker" : "circuit");
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        sb.DrawString(*S().font, "Through a compiled XNA effect", Vector2(24, 12), Kit::kGold, 0.0f, Vector2(0, 0), 1.0f, SpriteEffects::None, 0.0f);
        if (tex != nullptr) sb.Draw(*tex, Rectangle(30, 55, 280, 210), Color::White);
        sb.DrawString(*S().font, "plain SpriteBatch", Vector2(30, 270), Kit::kDim, 0.0f, Vector2(0, 0), 0.55f, SpriteEffects::None, 0.0f);
        sb.End();
        const bool run = use_ && effect_ != nullptr && !failed_;
        try {
            if (run) {
                effect_->getParametersProperty()["Weight"]->SetValue(weight_);
                const float w = tex != nullptr ? static_cast<float>(std::max(1, tex->getWidthProperty())) : 256.0f, h = tex != nullptr ? static_cast<float>(std::max(1, tex->getHeightProperty())) : 256.0f;
                effect_->getParametersProperty()["Offset"]->SetValue(Vector2(offset_.X / w, offset_.Y / h));
                const Vector2 o = effect_->getParametersProperty()["Offset"]->GetValueVector2();
                char rb[96];
                std::snprintf(rb, sizeof rb, "Weight %.2f, Offset (%.4f, %.4f)", static_cast<double>(effect_->getParametersProperty()["Weight"]->GetValueSingle()), static_cast<double>(o.X), static_cast<double>(o.Y));
                readBack_ = rb;
            }
            sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr, run ? effect_.get() : nullptr);
            if (tex != nullptr) sb.Draw(*tex, Rectangle(340, 55, 280, 210), Color::White);
            sb.End();
        } catch (const std::exception& e) {
            failed_ = true;
            status_ = std::string("could not run: ") + std::string(e.what()).substr(0, 60);
            if (S().font != nullptr) {
                sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
                sb.DrawString(*S().font, status_, Vector2(340, 150), Kit::Rgba(255, 190, 150), 0.0f, Vector2(0, 0), 0.55f, SpriteEffects::None, 0.0f);
                sb.End();
            }
        }
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        sb.DrawString(*S().font, run ? "compiled pixel shader: two texture taps" : (effect_ ? "effect switched off" : "no pixel effect"), Vector2(340, 270), Kit::kDim, 0.0f, Vector2(0, 0), 0.55f, SpriteEffects::None, 0.0f);
        sb.End();
        DrawVertexTextureGraph();
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        sb.DrawString(*S().font, "compiled vertex shader samples VertexTextures[2]", Vector2(30, 300), Kit::kGold, 0.0f, Vector2(0, 0), 0.62f, SpriteEffects::None, 0.0f);
        sb.DrawString(*S().font, vertexBinding_, Vector2(30, 505), compiledSupported_ ? Kit::Rgba(170, 230, 180) : Kit::Rgba(255, 190, 150), 0.0f, Vector2(0, 0), 0.5f, SpriteEffects::None, 0.0f);
        sb.End();
    }

private:
    struct VertexTextureVertex {
        Vector3 position;
        Vector2 sample;
    };

    void LoadPixelEffect()
    {
        effect_.reset();
        failed_ = false;
        parameters_ = techniques_ = readBack_ = "-";
        try {
#ifdef MUSEUM_PIPELINE
            std::shared_ptr<Processors::CompiledEffectContent> compiled;
            std::string input;
            if (sourceEffect_) {
                PipelineSupport::ImporterContext importerContext;
                Xna::EffectImporter importer;
                const std::shared_ptr<Xna::Graphics::EffectContent> source =
                    importer.Import(S().contentRoot + "/Effects/blur2tap.fx", importerContext);
                if (!source->getEffectCodeProperty().has_value()) throw std::runtime_error("EffectImporter returned no source");
                Processors::EffectProcessor processor;
                processor.setDebugModeProperty(static_cast<Processors::EffectProcessorDebugMode>(debugMode_));
                processor.setDefinesProperty(useDefine_ ? "MUSEUM_EFFECT=1" : "");
                PipelineSupport::ProcessorContext processorContext;
                compiled = processor.Process(source, processorContext);
                input = std::to_string(source->getEffectCodeProperty()->size()) + " byte FX source -> " +
                        std::to_string(compiled->GetEffectCode().size()) + " byte FXB; DebugMode " +
                        std::to_string(static_cast<int>(processor.getDebugModeProperty())) + ", Defines " +
                        (processor.getDefinesProperty().empty() ? "(empty)" : processor.getDefinesProperty());
            } else {
                std::ifstream in(S().contentRoot + "/Effects/blur2tap.fxb", std::ios::binary);
                const std::vector<char> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                if (raw.empty()) throw std::runtime_error("Content/Effects/blur2tap.fxb is missing");
                const std::vector<SharpRuntime::bytecs> bytes(raw.begin(), raw.end());
                compiled = std::make_shared<Processors::CompiledEffectContent>(bytes);
                input = std::to_string(bytes.size()) + " byte FXB";
            }
            Compiler::ContentCompiler compiler;
            Compiler::CompileOptions options;
            options.assetName = "blur2tap";
            options.targetProfile = GraphicsProfile::HiDef;
            const auto result = compiler.Compile<Processors::CompiledEffectContent>(compiled, options);
            if (!manager_) manager_ = std::make_unique<PipelineSupport::MemoryContentManager>(&S().game->getServicesProperty());
            manager_->Unload();
            manager_->bytes = result.bytes;
            effect_ = manager_->Load<std::shared_ptr<Effect>>("blur2tap");
            status_ = input + " -> " + std::to_string(result.bytes.size()) + " byte XNB -> EffectReader";
#else
            std::ifstream in(S().contentRoot + "/Effects/blur2tap.fxb", std::ios::binary);
            const std::vector<char> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (raw.empty()) throw std::runtime_error("Content/Effects/blur2tap.fxb is missing");
            const std::vector<SharpRuntime::bytecs> bytes(raw.begin(), raw.end());
            effect_ = std::make_shared<Effect>(*S().device, bytes);
            status_ = std::to_string(bytes.size()) + " bytes loaded";
#endif
            Describe();
        } catch (const std::exception& e) {
            status_ = std::string("refused: ") + std::string(e.what()).substr(0, 150);
            Museum::Log("fx", "%s", e.what());
        }
    }

    void BuildVertexTexture()
    {
        vertexTexture_ = std::make_unique<Texture2D>(*S().device, 4, 1, false, SurfaceFormat::Vector4);
        const Vector4 heights[] = {Vector4(0.08f, 0, 0, 1), Vector4(0.94f, 0, 0, 1), Vector4(0.22f, 0, 0, 1), Vector4(0.78f, 0, 0, 1)};
        vertexTexture_->SetData(heights, 4);
        constexpr int bars = 48;
        vertexVertices_.reserve(static_cast<std::size_t>(bars * 6));
        for (int i = 0; i < bars; ++i) {
            const float x0 = -0.9f + 1.8f * static_cast<float>(i) / static_cast<float>(bars);
            const float x1 = -0.9f + 1.8f * static_cast<float>(i + 1) / static_cast<float>(bars);
            const float u = -0.25f + 1.5f * (static_cast<float>(i) + 0.5f) / static_cast<float>(bars);
            const VertexTextureVertex a{Vector3(x0, 0, 0), Vector2(u, 0)}, b{Vector3(x1, 0, 0), Vector2(u, 0)};
            const VertexTextureVertex c{Vector3(x1, 0, 0), Vector2(u, 1)}, d{Vector3(x0, 0, 0), Vector2(u, 1)};
            vertexVertices_.insert(vertexVertices_.end(), {a, b, c, a, c, d});
        }
    }

    void DrawVertexTextureGraph()
    {
        if (!compiledSupported_ || !vertexEffect_ || !vertexTexture_ || vertexVertices_.empty()) {
            vertexBinding_ = compiledSupported_ ? "vertex texture effect unavailable" : "not run: CompiledEffects is false";
            return;
        }
        GraphicsDevice& device = *S().device;
        TextureCollection& textures = device.getVertexTexturesProperty();
        SamplerStateCollection& samplers = device.getVertexSamplerStatesProperty();
        Texture* oldTexture = textures[2];
        const SamplerState oldSampler = samplers[2];
        const SamplerState selected = vertexWrap_ ? SamplerState::PointWrap : SamplerState::PointClamp;
        const auto restore = [&] {
            textures(2, oldTexture);
            samplers[2] = oldSampler;
        };
        try {
            vertexEffect_->getParametersProperty()["Amount"]->SetValue(vertexAmount_);
            device.setRasterizerStateProperty(RasterizerState::CullNone);
            device.setDepthStencilStateProperty(DepthStencilState::None);
            device.setBlendStateProperty(BlendState::Opaque);
            static const VertexDeclaration declaration(sizeof(VertexTextureVertex), {
                VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                VertexElement(12, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0)});
            for (auto& pass : vertexEffect_->getCurrentTechniqueProperty()->getPassesProperty()) {
                pass.Apply();
                // Deliberately after Apply(): the effect has a plain sampler declaration, so these
                // public device collections are the authoritative application override.
                textures(2, vertexTexture_.get());
                samplers[2] = selected;
                device.DrawUserPrimitives(PrimitiveType::TriangleList, vertexVertices_.data(), 0,
                                          static_cast<int>(vertexVertices_.size() / 3), declaration);
            }
            const bool textureAsSet = textures[2] == vertexTexture_.get();
            const bool samplerAsSet = samplers[2].getFilterProperty() == selected.getFilterProperty() &&
                                      samplers[2].getAddressUProperty() == selected.getAddressUProperty();
            vertexBinding_ = std::string("slot 2 ") + (textureAsSet ? "Vector4 texture" : "WRONG texture") + ", " +
                             (vertexWrap_ ? "PointWrap" : "PointClamp") + (samplerAsSet ? ", drawn" : ", WRONG sampler");
            if (lastLoggedWrap_ != static_cast<int>(vertexWrap_)) {
                Museum::Log("fx", "vertex sampler draw: %s", vertexBinding_.c_str());
                lastLoggedWrap_ = static_cast<int>(vertexWrap_);
            }
            restore();
        } catch (const std::exception& e) {
            restore();
            vertexBinding_ = std::string("draw failed: ") + std::string(e.what()).substr(0, 58);
            Museum::Log("fx", "vertex texture draw: %s", e.what());
        }
    }

    // Exact results of the two fixtures (Content/Effects/*.fx), run from RenderOffscreen so the screen's own target is free:
    // blur2tap returns tex(uv) * Weight + tex(uv + Offset) * Weight, so with Offset 0 a flat colour is scaled by 2 x Weight;
    // vertextexture's bars sample u from -0.25 to 1.25, so PointClamp and PointWrap must give different shapes.
    void RenderOffscreen(const DrawContext& ctx) override
    {
        ScreenExhibit::RenderOffscreen(ctx);
        if (!checkPending_) return;
        checkPending_ = false;
        const auto check = [this](const std::string& name, const std::string& expected, const std::string& got) {
            Museum::Log("check", "%s %s: %s (expected %s, got %s)", R().key.c_str(), name.c_str(), expected == got ? "PASS" : "FAIL", expected.c_str(), got.c_str());
        };
        if (!compiledSupported_) {
            Museum::Log("check", "%s compiled effects: SKIP (GraphicsCapability.CompiledEffects is false on this renderer)", R().key.c_str());
            return;
        }
        GraphicsDevice& d = ctx.device;
        if (effect_ != nullptr) {
            EffectParameterCollection& reflected = effect_->getParametersProperty();
            const EffectParameter* weight = reflected["Weight"];
            const EffectParameter* offset = reflected["Offset"];
            const EffectTechnique* blur = effect_->getTechniquesProperty()["Blur"];
            const std::string got = std::string(weight != nullptr && weight->getParameterClassProperty() == EffectParameterClass::Scalar ? "Weight scalar" : "no scalar Weight") + ", " +
                                    (offset != nullptr && offset->getParameterClassProperty() == EffectParameterClass::Vector && offset->getColumnCountProperty() == 2 ? "Offset float2" : "no float2 Offset") + ", " +
                                    (blur != nullptr ? "technique Blur with " + std::to_string(blur->getPassesProperty().getCountProperty()) + " pass" : std::string("no technique Blur"));
            check("blur2tap reflection", "Weight scalar, Offset float2, technique Blur with 1 pass", got);
            const Color flat(200, 100, 40, 255);
            Texture2D source(d, 4, 4, false, SurfaceFormat::Color);
            std::vector<Color> texels(16, flat);
            source.SetData(texels.data(), 16);
            RenderTarget2D target(d, 16, 16, false, SurfaceFormat::Color, DepthFormat::None);
            SpriteBatch batch(d);
            for (const float w : {0.5f, 0.25f}) {
                effect_->getParametersProperty()["Weight"]->SetValue(w);
                effect_->getParametersProperty()["Offset"]->SetValue(Vector2(0.0f, 0.0f));
                d.SetRenderTarget(&target);
                d.Clear(Color::Black);
                batch.Begin(SpriteSortMode::Immediate, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr, effect_.get());
                batch.Draw(source, Rectangle(0, 0, 16, 16), Color::White);
                batch.End();
                d.SetRenderTarget(nullptr);
                std::vector<Color> px(256);
                target.GetData(px.data(), 256);
                const Color c = px[8 * 16 + 8];
                const int er = static_cast<int>(200 * 2 * w), eg = static_cast<int>(100 * 2 * w), eb = static_cast<int>(40 * 2 * w);
                const bool near = std::abs(c.getRProperty() - er) <= 3 && std::abs(c.getGProperty() - eg) <= 3 && std::abs(c.getBProperty() - eb) <= 3;
                char buffer[96];
                std::snprintf(buffer, sizeof buffer, "%d,%d,%d", c.getRProperty(), c.getGProperty(), c.getBProperty());
                char want[96];
                std::snprintf(want, sizeof want, "%d,%d,%d +-3", er, eg, eb);
                check("blur2tap with Weight " + std::string(w == 0.5f ? "0.5" : "0.25") + ", Offset 0 on 200,100,40", want, near ? std::string(want) : std::string(buffer));
            }
            const float panelWeight = weight_;
            reflected["Weight"]->SetValue(panelWeight);   // the panel's own weight back
        }
        if (vertexEffect_ == nullptr || vertexTexture_ == nullptr) {
            Museum::Log("check", "%s vertextexture: SKIP (the renderer refused it: %s)", R().key.c_str(), vertexStatus_.c_str());
        } else {
            RenderTarget2D target(d, 64, 64, false, SurfaceFormat::Color, DepthFormat::None);
            std::vector<Color> clamp(64 * 64), wrap(64 * 64);
            const bool keep = vertexWrap_;
            for (int mode = 0; mode < 2; ++mode) {
                vertexWrap_ = mode == 1;
                d.SetRenderTarget(&target);
                d.setViewportProperty(Viewport(0, 0, 64, 64));
                d.Clear(Color::Black);
                DrawVertexTextureGraph();
                d.SetRenderTarget(nullptr);
                target.GetData((mode == 0 ? clamp : wrap).data(), 64 * 64);
            }
            vertexWrap_ = keep;
            if (vertexBinding_.rfind("draw failed", 0) == 0) {
                Museum::Log("check", "%s vertextexture: SKIP (the renderer refused the draw: %s)", R().key.c_str(), vertexBinding_.c_str());
                return;
            }
            int lit = 0, differ = 0;
            for (std::size_t i = 0; i < clamp.size(); ++i) {
                lit += clamp[i].getPackedValueProperty() != Color::Black.getPackedValueProperty() ? 1 : 0;
                differ += clamp[i].getPackedValueProperty() != wrap[i].getPackedValueProperty() ? 1 : 0;
            }
            check("vertextexture: VertexSamplerStates[2] PointClamp against PointWrap", "a drawn graph whose shape changes",
                  lit > 100 && differ > 20 ? std::string("a drawn graph whose shape changes") : std::to_string(lit) + " lit pixels, " + std::to_string(differ) + " differ");
        }
    }

    void Describe()
    {
        parameters_.clear();
        techniques_.clear();
        const EffectParameterCollection& ps = effect_->getParametersProperty();
        for (int i = 0; i < ps.getCountProperty(); ++i) {
            const EffectParameter& p = *ps[i];
            if (!parameters_.empty()) parameters_ += ", ";
            parameters_ += p.getNameProperty() + " (" + ClassName(p.getParameterClassProperty()) + " " + TypeName(p.getParameterTypeProperty()) + ")";
        }
        const EffectTechniqueCollection& ts = effect_->getTechniquesProperty();
        for (int i = 0; i < ts.getCountProperty(); ++i) {
            const EffectTechnique& t = *ts[i];
            if (!techniques_.empty()) techniques_ += ", ";
            techniques_ += t.getNameProperty() + " (" + std::to_string(t.getPassesProperty().getCountProperty()) + " pass)";
        }
    }

#ifdef MUSEUM_PIPELINE
    std::unique_ptr<PipelineSupport::MemoryContentManager> manager_;
#endif
    std::shared_ptr<Effect> effect_;
    std::unique_ptr<Effect> vertexEffect_;
    std::unique_ptr<Texture2D> vertexTexture_;
    std::vector<VertexTextureVertex> vertexVertices_;
    bool use_ = true, failed_ = false;
    bool compiledSupported_ = false, vertexWrap_ = false;
#ifdef MUSEUM_PIPELINE
    bool sourceEffect_ = false;
    int debugMode_ = 0;
    bool useDefine_ = false;
#endif
    float weight_ = 0.5f;
    float vertexAmount_ = 0.42f;
    Vector2 offset_{6.0f, 0.0f};
    int picture_ = 0;
    int lastLoggedWrap_ = -1;
    std::string capability_ = "-", status_ = "-", vertexStatus_ = "-", vertexBinding_ = "-", parameters_ = "-", techniques_ = "-", readBack_ = "-";
    bool checkPending_ = false;
};

}  // namespace

std::unique_ptr<Exhibit> MakeCompiledFxLab(const RoomDef& room, Services& services)
{
    return std::make_unique<CompiledFxLab>(room, services);
}
