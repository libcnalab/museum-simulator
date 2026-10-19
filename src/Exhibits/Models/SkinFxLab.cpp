// SkinnedEffect laboratory: the classic XNA skinning effect driven by hand-built bone matrices. A tube is skinned to a
// chain of joints with VertexPositionNormalTextureSkinned vertices; SetBoneTransforms takes one matrix per joint and
// WeightsPerVertex decides how many of the four blend weights the shader reads.
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/PunctualLightEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTextureSkinned.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

constexpr int kJoints = 4;           // bones in the palette
constexpr float kSegment = 1.0f;     // metres between joints
constexpr int kRings = 24;           // rings along the tube
constexpr int kSides = 20;           // vertices around a ring
constexpr float kRadius = 0.32f;

class SkinFxLab final : public Exhibit {
public:
    SkinFxLab(const RoomDef& room, Services& services) : Exhibit(room, services), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        BuildStagePlinth(b, 0.0f, 0.0f, 0.4f);
        ControlPanel& p = AddPanel("SkinnedEffect", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddChoice("WeightsPerVertex", {"1 (rigid: nearest joint)", "2 (blend two joints)", "4 (blend four joints)"}, 1, [this](int v) { weights_ = v == 0 ? 1 : (v == 1 ? 2 : 4); });
        p.AddSlider("Bend per joint", -80.0f, 80.0f, bend_, [this](float v) { bend_ = v; animate_ = false; }, "%.0f deg", 1.0f);
        p.AddSlider("Twist per joint", -60.0f, 60.0f, twist_, [this](float v) { twist_ = v; animate_ = false; }, "%.0f deg", 1.0f);
        p.AddToggle("Animate", true, [this](bool v) { animate_ = v; });
        p.AddToggle("Directional lights (EnableDefaultLighting)", true, [this](bool v) { lighting_ = v; });
        p.AddChoice("Punctual light", {"None", "Point", "Spot"}, 0,
                    [this](int v) { punctual_ = v; });
        p.AddToggle("PreferPerPixelLighting", true, [this](bool v) { perPixel_ = v; });
        p.AddToggle("Fog", false, [this](bool v) { fog_ = v; });
        p.AddToggle("Show joints and weights", true, [this](bool v) { joints_ = v; });
        p.AddReadout("Palette", [] { return std::to_string(kJoints) + " matrices of " + std::to_string(SkinnedEffect::MaxBones) + " allowed"; });
        p.AddReadout("Vertex", [] { return std::string("VertexPositionNormalTextureSkinned, 52 bytes"); });
        p.AddReadout("Vertices / triangles", [this] { return std::to_string(vertexCount_) + " / " + std::to_string(indexCount_ / 3); });
        SignSpec what = Kit::Plate("SkinnedEffect (XNA 4.0)",
                                   {"Skinning moves each vertex by a weighted blend of bone matrices. SkinnedEffect is XNA's lit, textured, fogged",
                                    "skinning effect: SetBoneTransforms takes one matrix per joint (bind pose inverse times current pose), and",
                                    "WeightsPerVertex 1, 2 or 4 says how many of the vertex's blend weights the shader reads. Compare 1 and 4."});
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        GraphicsDevice& d = *S().device;
        try {
            effect_ = std::make_unique<SkinnedEffect>(d);
            BuildMesh(d);
            ok_ = true;
        } catch (const std::exception& e) {
            ok_ = false;
            Museum::Log("skinfx", "SkinnedEffect unavailable: %s", e.what());
            ReportError(std::string("SkinnedEffect or mesh: ") + e.what());
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        if (animate_) time_ += ctx.dt;
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!ok_ || !effect_ || !vb_) return;
        GraphicsDevice& d = ctx.device;
        const float bend = animate_ ? 55.0f * std::sin(time_ * 0.9f) : bend_;
        const float twist = animate_ ? 35.0f * std::sin(time_ * 0.55f + 1.0f) : twist_;
        if (animate_) { bend_ = bend; twist_ = twist; }
        const std::vector<Matrix> skin = Palette(bend, twist);

        const Matrix stand = Matrix::CreateTranslation(R().ToWorld(0.0f, 0.55f, 0.0f));
        RenderStates::SetOpaque3D(d);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        d.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
        SkinnedEffect& fx = *effect_;
        fx.setWorldProperty(stand);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setTextureProperty(S().textures->Get("brick"));
        fx.setDiffuseColorProperty(Vector3(1, 1, 1));
        fx.setAlphaProperty(1.0f);
        fx.setPreferPerPixelLightingProperty(perPixel_);
        // SkinnedEffect always lights (LightingEnabled cannot be false): "off" means no directional lights and a flat ambient.
        fx.EnableDefaultLighting();
        if (!lighting_) {
            fx.getDirectionalLight0Property().setEnabledProperty(false);
            fx.getDirectionalLight1Property().setEnabledProperty(false);
            fx.getDirectionalLight2Property().setEnabledProperty(false);
            fx.setAmbientLightColorProperty(Vector3(0.85f, 0.85f, 0.85f));
        }
        PunctualLightEXT lamp;
        if (punctual_ != 0) {
            lamp.Kind = punctual_ == 1 ? PunctualLightKindEXT::Point : PunctualLightKindEXT::Spot;
            lamp.Position = R().ToWorld(1.8f, 2.2f, 1.4f);
            lamp.Direction = Vector3::Normalize(R().ToWorld(0.0f, 1.9f, 0.0f) - lamp.Position);
            lamp.DiffuseColor = Vector3(4.0f, 2.4f, 1.1f);
            lamp.Range = 7.0f;
            lamp.InnerAngle = MathHelper::ToRadians(24.0f);
            lamp.OuterAngle = MathHelper::ToRadians(38.0f);
        }
        fx.setPunctualLightEXT(lamp);
        fx.setFogEnabledProperty(fog_);
        if (fog_) {
            fx.setFogColorProperty(Vector3(0.12f, 0.14f, 0.22f));
            fx.setFogStartProperty(Vector3::Distance(ctx.view.eye, R().ToWorld(0, 1.8f, 0)) - 0.5f);
            fx.setFogEndProperty(Vector3::Distance(ctx.view.eye, R().ToWorld(0, 1.8f, 0)) + 3.5f);
        }
        fx.setWeightsPerVertexProperty(weights_);
        fx.SetBoneTransforms(skin);
        fx.Apply();
        d.SetVertexBuffer(vb_.get());
        d.setIndicesProperty(ib_.get());
        d.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, vertexCount_, 0, indexCount_ / 3);

        if (joints_) DrawJoints(stand, bend, twist);
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] static Vector3 JointRest(int i) { return Vector3(0.0f, static_cast<float>(i) * kSegment, 0.0f); }

    // Global joint transforms for the current pose (row vectors: a child is its local transform times its parent's).
    [[nodiscard]] std::array<Matrix, kJoints> JointPoses(float bendDeg, float twistDeg) const
    {
        std::array<Matrix, kJoints> g;
        for (int i = 0; i < kJoints; ++i) {
            const Matrix local = (i == 0 ? Matrix::getIdentityProperty()
                                         : Matrix::CreateRotationY(MathHelper::ToRadians(twistDeg)) * Matrix::CreateRotationZ(MathHelper::ToRadians(bendDeg))) *
                                 Matrix::CreateTranslation(i == 0 ? JointRest(0) : Vector3(0.0f, kSegment, 0.0f));
            g[static_cast<std::size_t>(i)] = i == 0 ? local : local * g[static_cast<std::size_t>(i - 1)];
        }
        return g;
    }

    // What SetBoneTransforms wants: each joint's inverse bind pose followed by its current pose.
    [[nodiscard]] std::vector<Matrix> Palette(float bendDeg, float twistDeg) const
    {
        const auto g = JointPoses(bendDeg, twistDeg);
        std::vector<Matrix> skin(kJoints);
        for (int i = 0; i < kJoints; ++i)
            skin[static_cast<std::size_t>(i)] = Matrix::CreateTranslation(-JointRest(i)) * g[static_cast<std::size_t>(i)];
        return skin;
    }

    void DrawJoints(const Matrix& stand, float bend, float twist)
    {
        const auto g = JointPoses(bend, twist);
        Vector3 previous;
        for (int i = 0; i < kJoints; ++i) {
            const Vector3 p = Vector3::Transform(Vector3::Zero, g[static_cast<std::size_t>(i)] * stand);
            gizmos_.Sphere(BoundingSphere(p, 0.07f), Color(255, 230, 90, 255), 16);
            if (i > 0) gizmos_.Line(previous, p, Color(255, 230, 90, 255));
            gizmos_.Label(p + Off(0.55f, 0.0f, 0.0f), "joint " + std::to_string(i), Color(255, 235, 170, 255), 0.4f);
            previous = p;
        }
    }

    void BuildMesh(GraphicsDevice& d)
    {
        std::vector<VertexPositionNormalTextureSkinned> v;
        std::vector<std::uint16_t> idx;
        const float height = static_cast<float>(kJoints - 1) * kSegment;
        for (int r = 0; r <= kRings; ++r) {
            const float y = height * static_cast<float>(r) / static_cast<float>(kRings);
            // Blend weights from the distance to each joint, the four largest first (the shader reads the first N of them).
            std::array<std::pair<float, int>, kJoints> w;
            float sum = 0.0f;
            for (int j = 0; j < kJoints; ++j) {
                const float dist = std::abs(y - static_cast<float>(j) * kSegment) / kSegment;
                const float k = std::max(0.0f, 1.0f - dist / 1.2f);
                w[static_cast<std::size_t>(j)] = {k * k, j};
                sum += k * k;
            }
            std::sort(w.begin(), w.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
            for (auto& e : w) e.first /= std::max(sum, 1e-4f);
            const Vector4 weights(w[0].first, w[1].first, w[2].first, w[3].first);
            const std::array<std::uint8_t, 4> bones{static_cast<std::uint8_t>(w[0].second), static_cast<std::uint8_t>(w[1].second),
                                                     static_cast<std::uint8_t>(w[2].second), static_cast<std::uint8_t>(w[3].second)};
            for (int s = 0; s <= kSides; ++s) {
                const float a = MathHelper::TwoPi * static_cast<float>(s) / static_cast<float>(kSides);
                const Vector3 n(std::cos(a), 0.0f, std::sin(a));
                v.emplace_back(Vector3(n.X * kRadius, y, n.Z * kRadius), n, Vector2(static_cast<float>(s) / kSides * 2.0f, y * 0.6f), weights, bones);
            }
        }
        for (int r = 0; r < kRings; ++r)
            for (int s = 0; s < kSides; ++s) {
                const auto i0 = static_cast<std::uint16_t>(r * (kSides + 1) + s), i1 = static_cast<std::uint16_t>(i0 + 1);
                const auto i2 = static_cast<std::uint16_t>(i0 + kSides + 1), i3 = static_cast<std::uint16_t>(i2 + 1);
                idx.insert(idx.end(), {i0, i2, i1, i1, i2, i3});
            }
        vertexCount_ = static_cast<int>(v.size());
        indexCount_ = static_cast<int>(idx.size());
        vb_ = std::make_unique<VertexBuffer>(d, VertexPositionNormalTextureSkinned::getVertexDeclarationStatic(), vertexCount_, BufferUsage::WriteOnly);
        vb_->SetData(v.data(), vertexCount_);
        ib_ = std::make_unique<IndexBuffer>(d, IndexElementSize::SixteenBits, indexCount_, BufferUsage::WriteOnly);
        ib_->SetData(idx.data(), indexCount_);
    }

    Gizmos gizmos_;
    std::unique_ptr<SkinnedEffect> effect_;
    std::unique_ptr<VertexBuffer> vb_;
    std::unique_ptr<IndexBuffer> ib_;
    int vertexCount_ = 0, indexCount_ = 0, weights_ = 2, punctual_ = 0;
    float time_ = 0.0f, bend_ = 0.0f, twist_ = 0.0f;
    bool ok_ = false, animate_ = true, lighting_ = true, perPixel_ = true, fog_ = false, joints_ = true;
};

}  // namespace

std::unique_ptr<Exhibit> MakeSkinFxLab(const RoomDef& room, Services& services)
{
    return std::make_unique<SkinFxLab>(room, services);
}
