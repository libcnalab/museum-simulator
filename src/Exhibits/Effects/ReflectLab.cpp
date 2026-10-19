// Effect reflection: what an Effect tells a program about itself. Parameters (name, class, type, size, semantic,
// annotations), techniques and passes, the IEffectMatrices / IEffectLights / IEffectFog interfaces that let tools
// configure any stock effect the same way, and Effect.Clone.
#include <algorithm>
#include <array>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DualTextureEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectAnnotation.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectAnnotationCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameter.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameterClass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameterCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameterType.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechniqueCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EnvironmentMapEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/IEffectFog.hpp"
#include "Microsoft/Xna/Framework/Graphics/IEffectLights.hpp"
#include "Microsoft/Xna/Framework/Graphics/IEffectMatrices.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteEffect.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const char* ClassName(EffectParameterClass c)
{
    switch (c) {
        case EffectParameterClass::Scalar: return "Scalar";
        case EffectParameterClass::Vector: return "Vector";
        case EffectParameterClass::Matrix: return "Matrix";
        case EffectParameterClass::Object: return "Object";
        case EffectParameterClass::Struct: return "Struct";
    }
    return "?";
}

const char* TypeName(EffectParameterType t)
{
    switch (t) {
        case EffectParameterType::Void: return "Void";
        case EffectParameterType::Bool: return "Bool";
        case EffectParameterType::Int32: return "Int32";
        case EffectParameterType::Single: return "Single";
        case EffectParameterType::String: return "String";
        case EffectParameterType::Texture: return "Texture";
        case EffectParameterType::Texture1D: return "Texture1D";
        case EffectParameterType::Texture2D: return "Texture2D";
        case EffectParameterType::Texture3D: return "Texture3D";
        case EffectParameterType::TextureCube: return "TextureCube";
    }
    return "?";
}

const char* kEffects[] = {"BasicEffect", "AlphaTestEffect", "DualTextureEffect", "EnvironmentMapEffect", "SkinnedEffect", "SpriteEffect"};

class ReflectLab final : public ScreenExhibit {
public:
    ReflectLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("The Effect object model", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddChoice("Effect", {kEffects[0], kEffects[1], kEffects[2], kEffects[3], kEffects[4], kEffects[5]}, 0, [this](int v) { Select(v); });
        p.AddSlider("Scroll the parameter list", 0.0f, 1.0f, scroll_, [this](float v) { scroll_ = v; });
        p.AddButton("Configure through the interfaces", [this] { ConfigureThroughInterfaces(); });
        p.AddButton("Clone the effect", [this] { CloneIt(); });
        p.AddReadout("Interfaces", [this] { return interfaces_; });
        p.AddReadout("Via interfaces", [this] { return configured_; });
        p.AddReadout("Clone", [this] { return cloned_; });
        AddInfoSign(b, "What an Effect can tell you",
                    {"Every Effect lists its Parameters (name, class, type, rows x columns, semantic, annotations), its Techniques and the",
                     "Passes of each. The stock effects also implement IEffectMatrices, IEffectLights and IEffectFog, so one routine can set",
                     "World/View/Projection, lights and fog on any of them. Clone() copies an effect with its current values."});
    }

    void OnScreenLoaded() override { Select(0); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr || !effect_) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.62f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(std::string(kEffects[index_]) + ": " + summary_, 24, 12, Kit::kGold, 0.85f);
        rect(20, 64, 920, 2, Kit::Rgba(90, 100, 130));
        const float cols[] = {30, 290, 380, 465, 560, 790};
        const char* heads[] = {"Parameter", "Class", "Type", "Size", "GetValue*()", "Semantic / notes"};
        for (int i = 0; i < 6; ++i) text(heads[i], cols[i], 72, Kit::kDim, 0.55f);
        const int visibleRows = 12;
        const int total = static_cast<int>(rows_.size());
        const int first = std::clamp(static_cast<int>(scroll_ * static_cast<float>(std::max(0, total - visibleRows)) + 0.5f), 0, std::max(0, total - visibleRows));
        for (int r = 0; r < visibleRows && first + r < total; ++r) {
            const auto& row = rows_[static_cast<std::size_t>(first + r)];
            const float y = 100.0f + static_cast<float>(r) * 29.0f;
            if (r % 2 == 0) rect(20, y - 3, 920, 28, Kit::Rgba(36, 44, 66));
            for (int i = 0; i < 6; ++i) text(row[static_cast<std::size_t>(i)], cols[i], y, i == 0 ? Kit::kText : Kit::kXna, 0.58f);
        }
        text(std::to_string(total) + " parameters, showing " + std::to_string(first + 1) + " to " + std::to_string(std::min(total, first + visibleRows)), 30, 462, Kit::kDim, 0.55f);
        text(techniques_, 30, 492, Kit::kText, 0.58f);
        sb.End();
    }

private:
    // CNA's stock SkinnedEffect exposes its 72 Bones matrices through the array getters, but its
    // hand-built reflection metadata predates the compiled-effect Elements collection and records
    // the array length in RowCount instead. Keep the room honest on both representations: compiled
    // effects use Elements.Count, while the stock Bones parameter is the sole matrix whose row
    // count is larger than a matrix can be.
    static int ArrayCount(const EffectParameter& p)
    {
        const int reflected = p.getElementsProperty().getCountProperty();
        if (reflected > 0) return reflected;
        if (p.getParameterTypeProperty() == EffectParameterType::Single &&
            p.getParameterClassProperty() == EffectParameterClass::Matrix &&
            p.getRowCountProperty() > 4 && p.getColumnCountProperty() == 4)
            return p.getRowCountProperty();
        return 0;
    }

    void Select(int i)
    {
        index_ = std::clamp(i, 0, 5);
        GraphicsDevice& d = *S().device;
        effect_.reset();
        clone_.reset();
        rows_.clear();
        cloned_ = "not cloned yet";
        configured_ = "-";
        try {
            switch (index_) {
                case 0: effect_ = std::make_unique<BasicEffect>(d); break;
                case 1: effect_ = std::make_unique<AlphaTestEffect>(d); break;
                case 2: effect_ = std::make_unique<DualTextureEffect>(d); break;
                case 3: effect_ = std::make_unique<EnvironmentMapEffect>(d); break;
                case 4: effect_ = std::make_unique<SkinnedEffect>(d); break;
                default: effect_ = std::make_unique<SpriteEffect>(d); break;
            }
        } catch (const std::exception& e) {
            summary_ = std::string("could not be created: ") + e.what();
            Museum::Log("reflect", "%s: %s", kEffects[index_], e.what());
            return;
        }
        Reflect();
    }

    // The parameter's current value through the GetValue overload its class, type and shape call for.
    static std::string ValueText(const EffectParameter& p)
    {
        char b[96];
        try {
            const int rows = p.getRowCountProperty(), cols = p.getColumnCountProperty();
            // An array parameter has its own getters, one per element type (and a transposed form for matrices).
            const int elementCount = ArrayCount(p);
            if (elementCount > 0 && p.getParameterTypeProperty() == EffectParameterType::Single) {
                if (p.getParameterClassProperty() == EffectParameterClass::Matrix && cols == 4) {
                    const Matrix first = p.GetValueMatrixArray(elementCount)[0], transposed = p.GetValueMatrixTransposeArray(elementCount)[0];
                    std::snprintf(b, sizeof b, "M[%d]: M41 %.2f / T.M14 %.2f", elementCount, static_cast<double>(first.M41), static_cast<double>(transposed.M14));
                    return b;
                }
                if (p.getParameterClassProperty() == EffectParameterClass::Scalar) { std::snprintf(b, sizeof b, "float[%d]: [0] = %.2f", elementCount, static_cast<double>(p.GetValueSingleArray(elementCount)[0])); return b; }
                if (p.getParameterClassProperty() == EffectParameterClass::Vector && cols == 2) { const Vector2 v = p.GetValueVector2Array(elementCount)[0]; std::snprintf(b, sizeof b, "Vector2[%d]: [0] = (%.2f, %.2f)", elementCount, static_cast<double>(v.X), static_cast<double>(v.Y)); return b; }
                if (p.getParameterClassProperty() == EffectParameterClass::Vector && cols == 3) { const Vector3 v = p.GetValueVector3Array(elementCount)[0]; std::snprintf(b, sizeof b, "Vector3[%d]: [0] = (%.2f, %.2f, %.2f)", elementCount, static_cast<double>(v.X), static_cast<double>(v.Y), static_cast<double>(v.Z)); return b; }
                if (p.getParameterClassProperty() == EffectParameterClass::Vector && cols == 4) { const Vector4 v = p.GetValueVector4Array(elementCount)[0]; std::snprintf(b, sizeof b, "Vector4[%d]: [0] = (%.2f, %.2f, %.2f, %.2f)", elementCount, static_cast<double>(v.X), static_cast<double>(v.Y), static_cast<double>(v.Z), static_cast<double>(v.W)); return b; }
            }
            switch (p.getParameterTypeProperty()) {
                case EffectParameterType::Bool: return p.GetValueBoolean() ? "true" : "false";
                case EffectParameterType::Int32: return std::to_string(p.GetValueInt32());
                case EffectParameterType::String: return p.GetValueString();
                case EffectParameterType::Single:
                    if (p.getParameterClassProperty() == EffectParameterClass::Scalar) { std::snprintf(b, sizeof b, "%.2f", static_cast<double>(p.GetValueSingle())); return b; }
                    if (p.getParameterClassProperty() == EffectParameterClass::Matrix && rows == 4 && cols == 4) {
                        const Matrix m = p.GetValueMatrix();
                        const Matrix t = p.GetValueMatrixTranspose();
                        std::snprintf(b, sizeof b, "M11 %.2f M44 %.2f / T.M41 %.2f", static_cast<double>(m.M11), static_cast<double>(m.M44), static_cast<double>(t.M41));
                        return b;
                    }
                    if (p.getParameterClassProperty() == EffectParameterClass::Vector && cols == 2) { const Vector2 v = p.GetValueVector2(); std::snprintf(b, sizeof b, "(%.2f, %.2f)", static_cast<double>(v.X), static_cast<double>(v.Y)); return b; }
                    if (p.getParameterClassProperty() == EffectParameterClass::Vector && cols == 3) { const Vector3 v = p.GetValueVector3(); std::snprintf(b, sizeof b, "(%.2f, %.2f, %.2f)", static_cast<double>(v.X), static_cast<double>(v.Y), static_cast<double>(v.Z)); return b; }
                    if (p.getParameterClassProperty() == EffectParameterClass::Vector && cols == 4) { const Vector4 v = p.GetValueVector4(); std::snprintf(b, sizeof b, "(%.2f, %.2f, %.2f, %.2f)", static_cast<double>(v.X), static_cast<double>(v.Y), static_cast<double>(v.Z), static_cast<double>(v.W)); return b; }
                    return "-";
                case EffectParameterType::Texture2D: return p.GetValueTexture2D() != nullptr ? "a Texture2D" : "no texture bound";
                case EffectParameterType::TextureCube: return p.GetValueTextureCube() != nullptr ? "a TextureCube" : "no texture bound";
                default: return "-";
            }
        } catch (const std::exception& e) {
            return std::string("throws: ") + std::string(e.what()).substr(0, 20);
        }
    }

    void Reflect()
    {
        Effect& fx = *effect_;
        EffectParameterCollection& params = fx.getParametersProperty();
        for (int i = 0; i < params.getCountProperty(); ++i) {
            EffectParameter* p = params[i];
            if (p == nullptr) continue;
            char size[24];
            const int arrayCount = ArrayCount(*p);
            if (arrayCount > 0 && p->getParameterClassProperty() == EffectParameterClass::Matrix)
                std::snprintf(size, sizeof size, "4 x %d [%d]", p->getColumnCountProperty(), arrayCount);
            else
                std::snprintf(size, sizeof size, "%d x %d", p->getRowCountProperty(), p->getColumnCountProperty());
            std::string extra = p->getSemanticProperty();
            const int ann = p->getAnnotationsProperty().getCountProperty();
            if (ann > 0) extra += (extra.empty() ? "" : ", ") + std::to_string(ann) + " annotations";
            if (arrayCount > 0) extra += (extra.empty() ? "" : ", ") + std::to_string(arrayCount) + " elements";
            rows_.push_back({p->getNameProperty(), ClassName(p->getParameterClassProperty()), TypeName(p->getParameterTypeProperty()), size,
                             ValueText(*p), extra.empty() ? "-" : extra});
        }
        EffectTechniqueCollection& techs = fx.getTechniquesProperty();
        std::string t = std::to_string(techs.getCountProperty()) + " technique(s): ";
        for (int i = 0; i < techs.getCountProperty(); ++i) {
            EffectTechnique* tech = techs[i];
            if (tech == nullptr) continue;
            EffectPassCollection& passes = tech->getPassesProperty();
            std::string names;
            for (int k = 0; k < passes.getCountProperty(); ++k)
                if (const EffectPass* pass = passes[k]) names += (k ? " " : "") + pass->getNameProperty();
            t += (i ? ", " : "") + tech->getNameProperty() + " (" + std::to_string(passes.getCountProperty()) + " pass" + (passes.getCountProperty() == 1 ? "" : "es") +
                 (names.empty() ? "" : ": " + names) + ")";
        }
        const EffectTechnique* cur = fx.getCurrentTechniqueProperty();
        techniques_ = t + (cur != nullptr ? "; current: " + cur->getNameProperty() : "");
        summary_ = std::to_string(params.getCountProperty()) + " parameters";
        const bool m = dynamic_cast<IEffectMatrices*>(effect_.get()) != nullptr, l = dynamic_cast<IEffectLights*>(effect_.get()) != nullptr,
                   f = dynamic_cast<IEffectFog*>(effect_.get()) != nullptr;
        interfaces_ = std::string(m ? "Matrices " : "") + (l ? "Lights " : "") + (f ? "Fog" : "");
        if (interfaces_.empty()) interfaces_ = "none (a plain Effect)";
    }

    // The same code for every effect: this is what the interfaces are for.
    void ConfigureThroughInterfaces()
    {
        if (!effect_) return;
        std::string done;
        try {
            if (auto* m = dynamic_cast<IEffectMatrices*>(effect_.get())) {
                const Matrix w = Matrix::CreateTranslation(1.0f, 2.0f, 3.0f);
                m->setWorldProperty(w);
                m->setViewProperty(Matrix::CreateLookAt(Vector3(0, 0, 5), Vector3::Zero, Vector3::Up));
                m->setProjectionProperty(Matrix::CreatePerspectiveFieldOfView(1.0f, 1.6f, 0.1f, 50.0f));
                done += m->getWorldProperty() == w ? "matrices read back OK. " : "matrices MISMATCH. ";
            }
            if (auto* l = dynamic_cast<IEffectLights*>(effect_.get())) {
                l->EnableDefaultLighting();
                l->setAmbientLightColorProperty(Vector3(0.2f, 0.25f, 0.3f));
                done += l->getLightingEnabledProperty() && l->getDirectionalLight0Property().getEnabledProperty() ? "lights on. " : "lights not enabled. ";
            }
            if (auto* f = dynamic_cast<IEffectFog*>(effect_.get())) {
                f->setFogEnabledProperty(true);
                f->setFogStartProperty(5.0f);
                f->setFogEndProperty(40.0f);
                done += f->getFogEnabledProperty() && f->getFogEndProperty() == 40.0f ? "fog set." : "fog not set.";
            }
        } catch (const std::exception& e) {
            done += std::string("exception: ") + e.what();
        }
        configured_ = done.empty() ? "nothing to set: no interfaces" : done;
    }

    void CloneIt()
    {
        if (!effect_) return;
        try {
            // Change something first, so the clone visibly starts from the current values.
            if (auto* l = dynamic_cast<IEffectLights*>(effect_.get())) l->setAmbientLightColorProperty(Vector3(0.9f, 0.1f, 0.1f));
            clone_.reset(effect_->Clone());
            std::string r = "copy has " + std::to_string(clone_->getParametersProperty().getCountProperty()) + " parameters";
            if (auto* a = dynamic_cast<IEffectLights*>(effect_.get())) {
                if (auto* b = dynamic_cast<IEffectLights*>(clone_.get())) {
                    b->setAmbientLightColorProperty(Vector3(0.1f, 0.9f, 0.1f));
                    r += a->getAmbientLightColorProperty().X > 0.5f && b->getAmbientLightColorProperty().Y > 0.5f ? "; changing it left the original alone" : "; STATE SHARED";
                }
            }
            cloned_ = r;
        } catch (const std::exception& e) {
            cloned_ = std::string("Clone threw: ") + e.what();
        }
    }

    std::unique_ptr<Effect> effect_, clone_;
    std::vector<std::array<std::string, 6>> rows_;
    std::string summary_ = "-", techniques_ = "-", interfaces_ = "-", configured_ = "-", cloned_ = "-";
    int index_ = 0;
    float scroll_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeReflectLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ReflectLab>(room, services);
}
