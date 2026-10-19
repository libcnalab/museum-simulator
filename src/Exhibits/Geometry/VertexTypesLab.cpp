// Vertex types: what a vertex is made of. Each stock type (VertexPositionColor, VertexPositionTexture, VertexPositionColorTexture,
// VertexPositionNormalTexture) and one the museum declares itself (a struct plus a VertexDeclaration of VertexElements) draws the same
// quad through BasicEffect; the labels print the declaration CNA reads back from each type: stride, then every element with its
// offset, format and usage. A custom declaration is how a game stores data the stock types do not have.
#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionTexture.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/Gizmos.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

// The museum's own vertex: position, a packed colour, texture coordinates, a normal and one extra float (a "wobble" amount) that no
// stock type has. It is a plain struct; its declaration says where each member sits.
struct MuseumVertex {
    float position[3];
    std::uint8_t color[4];   // R, G, B, A: what VertexElementFormat::Color reads
    float uv[2];
    float normal[3];
    float wobble;
    static const VertexDeclaration& Declaration()
    {
        static const VertexDeclaration declaration(std::vector<VertexElement>{
            VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
            VertexElement(12, VertexElementFormat::Color, VertexElementUsage::Color, 0),
            VertexElement(16, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0),
            VertexElement(24, VertexElementFormat::Vector3, VertexElementUsage::Normal, 0),
            VertexElement(36, VertexElementFormat::Single, VertexElementUsage::TextureCoordinate, 1)});
        return declaration;
    }
};
static_assert(sizeof(MuseumVertex) == 40);

const char* FormatName(VertexElementFormat f)
{
    switch (f) {
        case VertexElementFormat::Single: return "Single";
        case VertexElementFormat::Vector2: return "Vector2";
        case VertexElementFormat::Vector3: return "Vector3";
        case VertexElementFormat::Vector4: return "Vector4";
        case VertexElementFormat::Color: return "Color";
        case VertexElementFormat::Byte4: return "Byte4";
        default: return "other";
    }
}
const char* UsageName(VertexElementUsage u)
{
    switch (u) {
        case VertexElementUsage::Position: return "Position";
        case VertexElementUsage::Color: return "Color";
        case VertexElementUsage::TextureCoordinate: return "TextureCoordinate";
        case VertexElementUsage::Normal: return "Normal";
        case VertexElementUsage::Binormal: return "Binormal";
        case VertexElementUsage::Tangent: return "Tangent";
        default: return "other";
    }
}

class VertexTypesLab final : public Exhibit {
public:
    VertexTypesLab(const RoomDef& room, Services& services) : Exhibit(room, services), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Vertex types", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddToggle("Light the textured quads", true, [this](bool v) { lit_ = v; });
        p.AddToggle("Spin the quads", true, [this](bool v) { spin_ = v; });
        p.AddReadout("Declarations read from", [] { return std::string("VertexDeclaration.GetVertexElements"); });
        SignSpec what = Kit::Plate("Vertex types and declarations",
                                   {"A vertex is whatever the GPU is told it is: a VertexDeclaration lists the elements (offset, format, usage) inside a stride. The stock types cover the common cases;",
                                    "the fifth quad uses a struct the museum declares itself, with an extra Single in a second texture-coordinate slot. BasicEffect reads the elements it knows and ignores the rest."});
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        GraphicsDevice& d = *S().device;
        try {
            effect_ = std::make_unique<BasicEffect>(d);
            const Vector3 n(0, 0, 1);
            const float h = 0.5f;
            const Vector3 c[4] = {{-h, h, 0}, {h, h, 0}, {h, -h, 0}, {-h, -h, 0}};
            const Vector2 t[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            const Color col[4] = {Color(255, 80, 70, 255), Color(90, 220, 110, 255), Color(80, 140, 255, 255), Color(255, 220, 80, 255)};
            const std::uint16_t order[6] = {0, 1, 2, 0, 2, 3};
            std::vector<VertexPositionColor> a;
            std::vector<VertexPositionTexture> bb;
            std::vector<VertexPositionColorTexture> cc;
            std::vector<VertexPositionNormalTexture> dd;
            std::vector<MuseumVertex> ee;
            for (std::uint16_t i : order) {
                a.emplace_back(c[i], col[i]);
                bb.emplace_back(c[i], t[i]);
                cc.emplace_back(c[i], col[i], t[i]);
                dd.emplace_back(c[i], n, t[i]);
                ee.push_back({{c[i].X, c[i].Y, c[i].Z}, {col[i].getRProperty(), col[i].getGProperty(), col[i].getBProperty(), col[i].getAProperty()}, {t[i].X, t[i].Y}, {n.X, n.Y, n.Z}, static_cast<float>(i) * 0.25f});
            }
            Make(vbColor_, d, VertexPositionColor::getVertexDeclarationStatic(), a);
            Make(vbTexture_, d, VertexPositionTexture::getVertexDeclarationStatic(), bb);
            Make(vbColorTexture_, d, VertexPositionColorTexture::getVertexDeclarationStatic(), cc);
            Make(vbNormalTexture_, d, VertexPositionNormalTexture::getVertexDeclarationStatic(), dd);
            Make(vbCustom_, d, MuseumVertex::Declaration(), ee);
            ok_ = true;
        } catch (const std::exception& e) {
            Museum::Log("vertextypes", "unavailable: %s", e.what());
            ReportError(std::string("vertex buffers: ") + e.what());
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (ctx.nearby && spin_) angle_ += ctx.dt * 0.6f;
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!ok_) return;
        GraphicsDevice& d = ctx.device;
        RenderStates::SetOpaque3D(d);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        d.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
        BasicEffect& fx = *effect_;
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        struct Item {
            VertexBuffer* vb;
            const VertexDeclaration* declaration;
            const char* name;
            bool color, texture, light;
        };
        const Item items[5] = {
            {vbColor_.get(), &VertexPositionColor::getVertexDeclarationStatic(), "VertexPositionColor", true, false, false},
            {vbTexture_.get(), &VertexPositionTexture::getVertexDeclarationStatic(), "VertexPositionTexture", false, true, false},
            {vbColorTexture_.get(), &VertexPositionColorTexture::getVertexDeclarationStatic(), "VertexPositionColorTexture", true, true, false},
            {vbNormalTexture_.get(), &VertexPositionNormalTexture::getVertexDeclarationStatic(), "VertexPositionNormalTexture", false, true, true},
            {vbCustom_.get(), &MuseumVertex::Declaration(), "MuseumVertex (declared here)", true, true, false},
        };
        for (int i = 0; i < 5; ++i) {
            const Item& it = items[i];
            const Vector3 centre = R().ToWorld((static_cast<float>(i) - 2.0f) * 2.4f, 1.9f, -1.2f);
            fx.setWorldProperty(Matrix::CreateRotationY(angle_ + static_cast<float>(i)) * Matrix::CreateTranslation(centre));
            fx.setVertexColorEnabledProperty(it.color);
            fx.setTextureEnabledProperty(it.texture);
            fx.setTextureProperty(S().textures->Get("uvgrid"));
            fx.setLightingEnabledProperty(it.light && lit_);
            if (it.light && lit_) fx.EnableDefaultLighting();
            fx.setDiffuseColorProperty(Vector3(1, 1, 1));
            EffectBank::Apply(fx);
            d.SetVertexBuffer(it.vb);
            d.DrawPrimitives(PrimitiveType::TriangleList, 0, 2);
            // The declaration, as CNA reports it.
            const Vector3 label = R().ToWorld((static_cast<float>(i) - 2.0f) * 2.4f, 1.0f, -1.2f);
            gizmos_.Label(label + Off(0.0f, 1.35f, 0.0f), it.name, Color(255, 226, 150, 255), 0.5f);
            gizmos_.Label(label + Off(0.0f, 1.18f, 0.0f), "stride " + std::to_string(it.declaration->getVertexStrideProperty()) + " bytes", Color(190, 200, 230, 255), 0.4f);
            float y = 0.0f;
            for (const VertexElement& e : it.declaration->GetVertexElements()) {
                gizmos_.Label(label + Off(0.0f, y, 0.0f), std::string("+") + std::to_string(e.getOffsetProperty()) + "  " + FormatName(e.getVertexElementFormatProperty()) + "  " + UsageName(e.getVertexElementUsageProperty()) + std::to_string(e.getUsageIndexProperty()), Color(170, 255, 190, 255), 0.36f);
                y -= 0.14f;
            }
        }
        gizmos_.Flush(ctx);
    }

    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    template <typename T>
    static void Make(std::unique_ptr<VertexBuffer>& vb, GraphicsDevice& d, const VertexDeclaration& declaration, const std::vector<T>& data)
    {
        vb = std::make_unique<VertexBuffer>(d, declaration, static_cast<int>(data.size()), BufferUsage::WriteOnly);
        if constexpr (std::is_trivially_copyable_v<T>) vb->SetData(data.data(), static_cast<int>(data.size()));
        else vb->SetData(data.data(), static_cast<int>(data.size()));
    }

    Gizmos gizmos_;
    std::unique_ptr<BasicEffect> effect_;
    std::unique_ptr<VertexBuffer> vbColor_, vbTexture_, vbColorTexture_, vbNormalTexture_, vbCustom_;
    bool ok_ = false, lit_ = true, spin_ = true;
    float angle_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeVertexTypesLab(const RoomDef& room, Services& services)
{
    return std::make_unique<VertexTypesLab>(room, services);
}
