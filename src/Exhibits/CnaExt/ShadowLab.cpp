// Shadow mapping with CNA's shadow-receiver extension. A portable caster pass draws the objects from the light into
// a render target through stock BasicEffect: the CPU writes light-space depth into VertexPositionColor and the
// orthographic projection makes raster interpolation exact. BasicEffect then receives the map and darkens what the
// casters hide, with a bias and a percentage-closer filter of 0, 1 or 2 texels.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "CNA/GraphicsCapability.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IShadowReceiverEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

class ShadowLab final : public ScreenExhibit {
public:
    ShadowLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 5.2f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Shadow mapping", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddToggle("Shadows (setShadowsEnabledEXT)", true, [this](bool v) { on_ = v; });
        p.AddSlider("Light direction (degrees)", 0.0f, 360.0f, angle_, [this](float v) { angle_ = v; animate_ = false; }, "%.0f", 1.0f);
        p.AddToggle("Move the light", true, [this](bool v) { animate_ = v; });
        p.AddSlider("Shadow bias", 0.0f, 0.03f, bias_, [this](float v) { bias_ = v; }, "%.4f");
        p.AddChoice("Filter radius (PCF)", {"0: one tap", "1: 3x3", "2: 5x5"}, 1, [this](int v) { radius_ = v; });
        p.AddChoice("Shadow map size (per cascade)", {"256", "512", "1024"}, 1, [this](int v) { sizeIndex_ = v; mapDirty_ = true; });
        p.AddChoice("Cascades (ShadowCascadeStateEXT)", {"One map", "Two cascades", "Three cascades"}, 0, [this](int v) { cascades_ = v == 0 ? 0 : v + 1; mapDirty_ = true; });
        p.AddToggle("Tint each cascade", false, [this](bool v) { tint_ = v; });
        p.AddReadout("ShadowSampling support", [this] { return std::string(supported_ ? "yes" : "NO on this renderer"); });
        p.AddReadout("Map format", [this] { return format_; });
        p.AddReadout("IShadowReceiverEXT (last draw)", [this] { return receiverNote_; });
        p.AddReadout("Cascade splits (m)", [this] { return splitText_; });
        AddInfoSign(b, "Shadow mapping", {"The shadow map holds light-space depth from a portable stock-effect caster. BasicEffect, which implements", "IShadowReceiverEXT, compares each surface's depth with the map: hidden means shadowed. PCF softens the edge."});
    }

    void OnScreenLoaded() override
    {
        GraphicsDevice& d = *S().device;
        supported_ = d.SupportsShadowSamplingEXT();
        if (supported_) {
            const int capacity = std::max({S().shapes->Sphere().VertexCount(), S().shapes->Cube().VertexCount(),
                                           S().shapes->Cylinder().VertexCount(), S().shapes->Cone().VertexCount()});
            casterVertices_ = std::make_unique<DynamicVertexBuffer>(d, VertexPositionColor::getVertexDeclarationStatic(),
                                                                    capacity, BufferUsage::WriteOnly);
            caster_ = std::make_unique<BasicEffect>(d);
            caster_->setLightingEnabledProperty(false);
            caster_->setTextureEnabledProperty(false);
            caster_->setVertexColorEnabledProperty(true);
            caster_->setFogEnabledProperty(false);
            caster_->setDiffuseColorProperty(Vector3(1, 1, 1));
            caster_->setWorldProperty(Matrix::getIdentityProperty());
            caster_->setViewProperty(Matrix::getIdentityProperty());
            caster_->setProjectionProperty(Matrix::getIdentityProperty());
        }
        lit_ = std::make_unique<BasicEffect>(d);
        lit_->EnableDefaultLighting();
        lit_->setTextureEnabledProperty(true);
        lit_->setTextureProperty(S().textures->White());
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        if (animate_) angle_ = std::fmod(angle_ + ctx.dt * 12.0f, 360.0f);
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (!ctx.offscreen && supported_ && caster_ && casterVertices_) RenderShadowMap(ctx.device, ctx.view);
        ScreenExhibit::RenderOffscreen(ctx);
    }

    void DrawExtra(const DrawContext& ctx) override
    {
        if (!lit_) return;
        GraphicsDevice& d = ctx.device;
        RenderStates::SetOpaque3D(d);
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        lit_->setViewProperty(ctx.view.view);
        lit_->setProjectionProperty(ctx.view.projection);
        const Vector3 toLight = LightDirection();
        DirectionalLight& l0 = lit_->getDirectionalLight0Property();
        l0.setEnabledProperty(true);
        l0.setDirectionProperty(-toLight);
        l0.setDiffuseColorProperty(Vector3(1.0f, 0.97f, 0.9f));
        lit_->getDirectionalLight1Property().setEnabledProperty(false);
        lit_->getDirectionalLight2Property().setEnabledProperty(false);
        lit_->setAmbientLightColorProperty(Vector3(0.25f, 0.27f, 0.32f));
        lit_->setSpecularColorProperty(Vector3(0, 0, 0));
        const bool shadows = supported_ && on_ && map_ != nullptr && mapReady_;
        lit_->setShadowMapEXT(shadows ? map_.get() : nullptr);
        lit_->setShadowsEnabledEXT(shadows);
        lit_->setLightViewProjectionEXT(lightVp_);
        lit_->setShadowCascadesEXT(cascades_ > 0 ? cascadeState_ : ShadowCascadeStateEXT{});
        lit_->setShadowDepthBiasEXT(bias_);
        lit_->setShadowFilterRadiusEXT(radius_);
        {
            // What any shadow-receiving effect reports, asked through the CNAEXT interface rather than the BasicEffect type.
            const IShadowReceiverEXT& receiver = *lit_;
            char note[96];
            std::snprintf(note, sizeof note, "%s, bias %.4f, filter radius %d", receiver.isShadowsEnabledEXT() ? "receiving" : "not receiving",
                          static_cast<double>(receiver.getShadowDepthBiasEXT()), receiver.getShadowFilterRadiusEXT());
            receiverNote_ = note;
        }
        for (const Item& item : Items()) {
            lit_->setDiffuseColorProperty(item.colour);
            lit_->setWorldProperty(item.world);
            EffectBank::Apply(*lit_);
            item.mesh->Draw(d);
        }
        lit_->setShadowMapEXT(nullptr);
        lit_->setShadowsEnabledEXT(false);
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
        sb.Draw(*S().whiteTexture, Rectangle(0, 0, 960, 540), Color(14, 18, 30, 255));
        if (map_ && mapReady_) sb.Draw(*map_, Rectangle(cascades_ == 0 ? 300 : 30, 40, cascades_ == 0 ? 460 : 900, cascades_ == 0 ? 460 : static_cast<int>(900.0f / static_cast<float>(cascades_))), Color::White);
        sb.End();
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        sb.DrawString(*S().font, "The shadow map (light-space depth, red channel; cascades side by side)", Vector2(24, 8), Kit::kGold, 0.0f, Vector2(0, 0), 0.8f, SpriteEffects::None, 0.0f);
        if (!supported_) sb.DrawString(*S().font, "This renderer does not support shadow sampling: no shadows are drawn.", Vector2(24, 500), Kit::Rgba(255, 190, 150), 0.0f, Vector2(0, 0), 0.7f, SpriteEffects::None, 0.0f);
        sb.End();
    }

private:
    struct Item {
        const LitMesh* mesh;
        Matrix world;
        Vector3 colour;
    };
    [[nodiscard]] std::vector<Item> Items() const
    {
        const Vector3 c = R().centre;
        const float spin = time_ * 0.5f;
        return {
            {&S().shapes->Plane(), Matrix::CreateScale(14.0f, 1.0f, 14.0f) * Matrix::CreateTranslation(c + Vector3(0, 0.02f, 0)), Vector3(0.7f, 0.75f, 0.8f)},
            {&S().shapes->Cube(), Matrix::CreateScale(1.0f) * Matrix::CreateRotationY(spin) * Matrix::CreateTranslation(c + Off(-1.8f, 0.5f, -0.6f)), Vector3(0.9f, 0.4f, 0.3f)},
            {&S().shapes->Sphere(), Matrix::CreateScale(1.1f) * Matrix::CreateTranslation(c + Off(0.1f, 0.9f + 0.25f * std::sin(time_), 0.9f)), Vector3(0.35f, 0.6f, 0.95f)},
            {&S().shapes->Cylinder(), Matrix::CreateScale(0.5f, 3.0f, 0.5f) * Matrix::CreateTranslation(c + Off(-5.0f, 0.0f, -5.5f)), Vector3(0.8f, 0.8f, 0.5f)},
            {&S().shapes->Cylinder(), Matrix::CreateScale(0.5f, 3.0f, 0.5f) * Matrix::CreateTranslation(c + Off(-5.0f, 0.0f, 0.0f)), Vector3(0.8f, 0.8f, 0.5f)},
            {&S().shapes->Cylinder(), Matrix::CreateScale(0.5f, 3.0f, 0.5f) * Matrix::CreateTranslation(c + Off(-5.0f, 0.0f, 5.5f)), Vector3(0.8f, 0.8f, 0.5f)},
            {&S().shapes->Cylinder(), Matrix::CreateScale(0.5f, 3.0f, 0.5f) * Matrix::CreateTranslation(c + Off(5.0f, 0.0f, -5.5f)), Vector3(0.8f, 0.8f, 0.5f)},
            {&S().shapes->Cylinder(), Matrix::CreateScale(0.5f, 3.0f, 0.5f) * Matrix::CreateTranslation(c + Off(5.0f, 0.0f, 5.5f)), Vector3(0.8f, 0.8f, 0.5f)},
            {&S().shapes->Cone(), Matrix::CreateScale(1.0f, 1.8f, 1.0f) * Matrix::CreateTranslation(c + Off(1.9f, 0.0f, -0.9f)), Vector3(0.4f, 0.85f, 0.45f)},
        };
    }
    [[nodiscard]] Vector3 LightDirection() const   // unit vector from the scene towards the light
    {
        const float a = MathHelper::ToRadians(angle_);
        return Vector3::Normalize(Vector3(std::cos(a) * 0.6f, 0.8f, std::sin(a) * 0.6f));
    }
    [[nodiscard]] Matrix LightViewProjection() const
    {
        const Vector3 target = R().centre + Vector3(0, 0.5f, 0);
        const Vector3 eye = target + LightDirection() * 14.0f;
        return Matrix::CreateLookAt(eye, target, Vector3::Up) * Matrix::CreateOrthographic(17.0f, 17.0f, 1.0f, 36.0f);
    }

    /** A light-space orthographic matrix that encloses the slice of the camera frustum between two view distances. */
    Matrix CascadeMatrix(const SceneView& view, float nearD, float farD) const
    {
        const Matrix proj = view.projection;
        const float tanY = 1.0f / proj.M22, tanX = 1.0f / proj.M11;
        const Matrix invView = Matrix::Invert(view.view);
        Vector3 corners[8];
        int k = 0;
        for (const float d : {nearD, farD})
            for (const float sx : {-1.0f, 1.0f})
                for (const float sy : {-1.0f, 1.0f}) corners[k++] = Vector3::Transform(Vector3(sx * tanX * d, sy * tanY * d, -d), invView);
        Vector3 centre(0, 0, 0);
        for (const Vector3& c : corners) centre = centre + c;
        centre = centre / 8.0f;
        float radius = 0.0f;
        for (const Vector3& c : corners) radius = std::max(radius, Vector3::Distance(c, centre));
        radius = std::ceil(radius * 4.0f) / 4.0f;   // a steady size, so the map does not shimmer as the camera turns
        const Vector3 eye = centre + LightDirection() * (radius + 12.0f);
        return Matrix::CreateLookAt(eye, centre, Vector3::Up) * Matrix::CreateOrthographic(radius * 2.0f, radius * 2.0f, 1.0f, radius * 2.0f + 30.0f);
    }

    void RenderShadowMap(GraphicsDevice& d, const SceneView& cameraView)
    {
        static const int sizes[] = {256, 512, 1024};
        const int count = std::max(1, cascades_);
        const int size = sizes[sizeIndex_];
        if (mapDirty_ || !map_) {
            mapReady_ = false;
            RenderStates::UnbindTextures(d);
            const bool floatTarget = d.SupportsCapability(CNA::GraphicsCapability::FloatRenderTargets);
            try {
                map_ = std::make_unique<RenderTarget2D>(d, size * count, size, false, floatTarget ? SurfaceFormat::Single : SurfaceFormat::Color, DepthFormat::Depth24);
                format_ = floatTarget ? "Single (32-bit float)" : "Color (8 bits: use a larger bias)";
            } catch (const std::exception& e) {
                Museum::Log("shadow", "shadow map target: %s", e.what());
                map_.reset();
                return;
            }
            mapDirty_ = false;
        }
        RenderStates::UnbindTextures(d);
        d.SetRenderTarget(map_.get());
        d.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color(255, 255, 255, 255), 1.0f, 0);
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::Default);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        cascadeState_ = ShadowCascadeStateEXT{};
        splitText_ = "-";
        std::vector<Matrix> matrices;
        if (cascades_ == 0) {
            matrices.push_back(LightViewProjection());
            lightVp_ = matrices[0];
        } else {
            static const float splitsByCount[4][3] = {{0, 0, 0}, {0, 0, 0}, {5.0f, 24.0f, 0.0f}, {4.0f, 9.0f, 26.0f}};
            const float* splits = splitsByCount[count];
            cascadeState_.Count = count;
            cascadeState_.CameraView = cameraView.view;
            cascadeState_.BlendBand = 1.0f;
            cascadeState_.DebugTint = tint_;
            splitText_.clear();
            float nearD = 0.3f;
            for (int i = 0; i < count; ++i) {
                const Matrix lvp = CascadeMatrix(cameraView, nearD, splits[i]);
                matrices.push_back(lvp);
                // World to atlas: the light's clip space remapped to this cascade's slice of the atlas (x), 0..1 (y, depth).
                const float slice = 1.0f / static_cast<float>(count);
                const Matrix remap(0.5f * slice, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 0.5f, 0, 0.5f * slice + slice * static_cast<float>(i), 0.5f, 0.5f, 1.0f);
                cascadeState_.WorldToAtlas[i] = lvp * remap;
                cascadeState_.SplitDistance[i] = splits[i];
                char buf[32];
                std::snprintf(buf, sizeof buf, "%s%.0f", i ? ", " : "", static_cast<double>(splits[i]));
                splitText_ += buf;
                nearD = splits[i] * 0.92f;
            }
            lightVp_ = matrices[0];
        }
        for (int c = 0; c < count; ++c) {
            d.setViewportProperty(Viewport(c * size, 0, size, size));
            for (const Item& item : Items()) {
                if (item.mesh == &S().shapes->Plane()) continue;   // the floor only receives
                const Matrix mvp = item.world * matrices[static_cast<std::size_t>(c)];
                casterColours_.clear();
                casterColours_.reserve(static_cast<std::size_t>(item.mesh->VertexCount()));
                for (const Vector3& position : item.mesh->Positions()) {
                    const Vector4 clip = Vector4::Transform(Vector4(position, 1.0f), mvp);
                    const float depth = std::clamp(clip.Z / clip.W * 0.5f + 0.5f, 0.0f, 1.0f);
                    const int channel = static_cast<int>(depth * 255.0f + 0.5f);
                    casterColours_.emplace_back(position, Kit::Rgba(channel, channel, channel));
                }
                casterVertices_->SetData(casterColours_.data(), 0, static_cast<int>(casterColours_.size()), SetDataOptions::Discard);
                caster_->setWorldProperty(mvp);
                EffectBank::Apply(*caster_);
                d.SetVertexBuffer(casterVertices_.get());
                d.setIndicesProperty(item.mesh->Indices());
                d.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, item.mesh->VertexCount(), 0,
                                        item.mesh->TriangleCount());
            }
        }
        d.SetRenderTarget(nullptr);
        mapReady_ = true;
    }

    std::unique_ptr<BasicEffect> caster_;
    std::unique_ptr<DynamicVertexBuffer> casterVertices_;
    std::vector<VertexPositionColor> casterColours_;
    std::unique_ptr<BasicEffect> lit_;
    std::unique_ptr<RenderTarget2D> map_;
    std::string format_ = "-", receiverNote_ = "-";
    Matrix lightVp_ = Matrix::getIdentityProperty();
    ShadowCascadeStateEXT cascadeState_;
    std::string splitText_ = "-";
    int cascades_ = 0;
    bool tint_ = false;
    float time_ = 0.0f, angle_ = 40.0f, bias_ = 0.006f;
    int radius_ = 1, sizeIndex_ = 1;
    bool on_ = true, animate_ = true, supported_ = true, mapDirty_ = true, mapReady_ = false;
};

}  // namespace

std::unique_ptr<Exhibit> MakeShadowLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ShadowLab>(room, services);
}
