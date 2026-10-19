// SkinnedModelEXT laboratory: CNA's container for a game's own skinned character: a skeleton, parts and named animation clips.
// The museum builds a seven-bone tentacle in code (no model file), gives it three keyframed clips, and ComputeBoneTransformsEXT turns
// a clip and a time into the matrices SkinnedEffect wants. A second model with the same skeleton, a cap, is worn and taken off
// with AttachPartEXT and RemovePartEXT.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedModelEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTextureSkinned.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Rendering/Gizmos.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "System/TimeSpan.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

constexpr int kBones = 7;
constexpr float kSegment = 0.45f;
constexpr int kRings = 28;
constexpr int kSides = 16;
constexpr float kDuration = 2.0f;

class SkinExtLab final : public Exhibit {
public:
    SkinExtLab(const RoomDef& room, Services& services) : Exhibit(room, services), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        plinth_ = BuildStagePlinth(b, 0.0f, 0.0f, 0.4f);
        ControlPanel& p = AddPanel("SkinnedModelEXT", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddChoice("Clip (Clips[name])", {"Wave", "Curl", "Twist"}, 0, [this](int v) { clip_ = v; });
        p.AddToggle("Play", true, [this](bool v) { play_ = v; });
        p.AddSlider("Position (when stopped)", 0.0f, kDuration, 0.0f, [this](float v) { position_ = v; play_ = false; }, "%.2f s");
        p.AddSlider("Speed", 0.0f, 3.0f, speed_, [this](float v) { speed_ = v; });
        p.AddToggle("Loop (wrap the position)", true, [this](bool v) { loop_ = v; });
        p.AddToggle("Show the skeleton", true, [this](bool v) { skeleton_ = v; });
        p.AddButton("AttachPartEXT: wear the cap", [this] { Wear(); });
        p.AddButton("RemovePartEXT(\"cap\")", [this] { TakeOff(); });
        p.AddReadout("BoneCount", [this] { return std::to_string(model_ ? model_->BoneCount : 0); });
        p.AddReadout("Parts", [this] { return PartNames(); });
        p.AddReadout("Clips", [this] { return model_ ? std::to_string(model_->Clips.size()) + " (" + ClipNames() + ")" : std::string("-"); });
        p.AddReadout("ComputeBoneTransformsEXT", [this] { return computeText_; });
        SignSpec what = Kit::Plate("SkinnedModelEXT (CNAEXT)",
                                   {"A skinned character you bring yourself: a skeleton (parents, bind pose, inverse bind pose), parts with their own buffers and named",
                                    "animation clips of per-bone keyframes. ComputeBoneTransformsEXT samples a clip (slerp for rotation) and composes the hierarchy into the matrices",
                                    "SkinnedEffect.SetBoneTransforms takes. The cap is a second model on the same skeleton: AttachPartEXT moves its part onto this one."});
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        try {
            effect_ = std::make_unique<SkinnedEffect>(*S().device);
            model_ = BuildTentacle(*S().device);
            ok_ = true;
        } catch (const std::exception& e) {
            ok_ = false;
            Museum::Log("skinext", "unavailable: %s", e.what());
            ReportError(std::string("SkinnedEffect or tentacle model: ") + e.what());
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby || !ok_) return;
        if (play_) position_ = std::fmod(position_ + ctx.dt * speed_, kDuration);
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!ok_ || !model_ || !effect_) return;
        try {
            static const char* names[] = {"Wave", "Curl", "Twist"};
            std::vector<Matrix> skin;
            const auto t0 = std::chrono::steady_clock::now();
            model_->ComputeBoneTransformsEXT(names[clip_], System::TimeSpan::FromSeconds(static_cast<double>(position_)), loop_, skin);
            const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
            char buf[96];
            std::snprintf(buf, sizeof buf, "%d matrices in %.0f us", static_cast<int>(skin.size()), us);
            computeText_ = buf;

            GraphicsDevice& d = ctx.device;
            const Matrix stand = Matrix::CreateTranslation(plinth_ + Vector3(0.0f, 0.02f, 0.0f));
            RenderStates::SetOpaque3D(d);
            d.setRasterizerStateProperty(RasterizerState::CullNone);
            d.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
            SkinnedEffect& fx = *effect_;
            fx.setWorldProperty(stand);
            fx.setViewProperty(ctx.view.view);
            fx.setProjectionProperty(ctx.view.projection);
            fx.setDiffuseColorProperty(Vector3(1, 1, 1));
            fx.setAlphaProperty(1.0f);
            fx.setPreferPerPixelLightingProperty(true);
            fx.EnableDefaultLighting();
            fx.setWeightsPerVertexProperty(2);
            fx.SetBoneTransforms(skin);
            for (const auto& part : model_->Parts) {
                fx.setTextureProperty(S().textures->Get(part.Name == "cap" ? "metal" : "tiles"));
                fx.Apply();
                d.SetVertexBuffer(part.Part->getVertexBufferProperty());
                d.setIndicesProperty(part.Part->getIndexBufferProperty());
                d.DrawIndexedPrimitives(PrimitiveType::TriangleList, part.Part->getVertexOffsetProperty(), 0, part.Part->getNumVerticesProperty(), part.Part->getStartIndexProperty(), part.Part->getPrimitiveCountProperty());
            }
            if (skeleton_) DrawSkeleton(skin, stand);
            gizmos_.Flush(ctx, true);
        } catch (const std::exception& e) {
            if (!reported_) {
                Museum::Log("skinext", "draw: %s", e.what());
                ReportError(std::string("draw: ") + e.what());   // per frame: reported_ keeps it to the first failure
            }
            reported_ = true;
        }
    }

private:
    [[nodiscard]] static Vector3 BoneRest(int i) { return Vector3(0.0f, static_cast<float>(i) * kSegment, 0.0f); }

    // Where each bone is now: the skinning matrix undoes the bind pose, so the global pose is bind * skin.
    void DrawSkeleton(const std::vector<Matrix>& skin, const Matrix& stand)
    {
        Vector3 previous;
        for (int i = 0; i < kBones && i < static_cast<int>(skin.size()); ++i) {
            const Vector3 p = Vector3::Transform(BoneRest(i), skin[static_cast<std::size_t>(i)] * stand);
            gizmos_.Sphere(BoundingSphere(p, 0.05f), Color(255, 230, 90, 255), 12);
            if (i > 0) gizmos_.Line(previous, p, Color(255, 230, 90, 255));
            previous = p;
        }
    }

    [[nodiscard]] std::string PartNames() const
    {
        if (!model_) return "-";
        std::string s;
        for (const auto& p : model_->Parts) s += (s.empty() ? "" : ", ") + p.Name;
        return s.empty() ? "(none)" : s;
    }
    [[nodiscard]] std::string ClipNames() const
    {
        std::string s;
        for (const char* n : {"Wave", "Curl", "Twist"}) s += (s.empty() ? "" : ", ") + std::string(n);
        return s;
    }

    static void Skeleton(SkinnedModelEXT& m)
    {
        m.BoneCount = kBones;
        m.ParentBoneIndices.assign(kBones, -1);
        m.BindPoseLocal.assign(kBones, Matrix::getIdentityProperty());
        m.InverseBindPoseGlobal.assign(kBones, Matrix::getIdentityProperty());
        for (int i = 0; i < kBones; ++i) {
            m.ParentBoneIndices[static_cast<std::size_t>(i)] = i - 1;
            m.BindPoseLocal[static_cast<std::size_t>(i)] = Matrix::CreateTranslation(i == 0 ? Vector3::Zero : Vector3(0.0f, kSegment, 0.0f));
            m.InverseBindPoseGlobal[static_cast<std::size_t>(i)] = Matrix::CreateTranslation(-BoneRest(i));
        }
    }

    // A clip is a rotation per bone every eighth of the duration; the last key repeats the first so that it loops.
    static AnimationClipEXT MakeClip(int which)
    {
        AnimationClipEXT clip;
        clip.Duration = System::TimeSpan::FromSeconds(static_cast<double>(kDuration));
        for (int bone = 0; bone < kBones; ++bone) {
            BoneTrackEXT track;
            track.BoneIndex = bone;
            for (int k = 0; k <= 8; ++k) {
                const float t = kDuration * static_cast<float>(k) / 8.0f;
                const float phase = MathHelper::TwoPi * t / kDuration;
                Quaternion q = Quaternion::Identity;
                if (bone > 0) {
                    switch (which) {
                        case 0: q = Quaternion::CreateFromAxisAngle(Vector3::UnitZ, MathHelper::ToRadians(26.0f * std::sin(phase - 0.75f * static_cast<float>(bone)))); break;
                        case 1: q = Quaternion::CreateFromAxisAngle(Vector3::UnitZ, MathHelper::ToRadians(36.0f * (0.65f + 0.35f * std::sin(phase)))); break;
                        default:
                            q = Quaternion::CreateFromAxisAngle(Vector3::UnitY, MathHelper::ToRadians(30.0f * std::sin(phase - 0.4f * static_cast<float>(bone)))) *
                                Quaternion::CreateFromAxisAngle(Vector3::UnitZ, MathHelper::ToRadians(12.0f));
                    }
                }
                track.Keys.push_back(KeyframeEXT{System::TimeSpan::FromSeconds(static_cast<double>(t)), bone == 0 ? Vector3::Zero : Vector3(0.0f, kSegment, 0.0f), q, Vector3(1.0f, 1.0f, 1.0f)});
            }
            clip.Tracks.push_back(std::move(track));
        }
        return clip;
    }

    // A part: vertices weighted to the bones, in its own buffers (SkinnedModelEXT owns them afterwards).
    static void AddPart(SkinnedModelEXT& m, GraphicsDevice& d, const std::string& name, const std::vector<VertexPositionNormalTextureSkinned>& v, const std::vector<std::uint16_t>& idx)
    {
        auto vb = std::make_unique<VertexBuffer>(d, VertexPositionNormalTextureSkinned::getVertexDeclarationStatic(), static_cast<int>(v.size()), BufferUsage::WriteOnly);
        vb->SetData(v.data(), static_cast<int>(v.size()));
        auto ib = std::make_unique<IndexBuffer>(d, IndexElementSize::SixteenBits, static_cast<int>(idx.size()), BufferUsage::WriteOnly);
        ib->SetData(idx.data(), static_cast<int>(idx.size()));
        auto part = std::make_unique<ModelMeshPart>(vb.get(), ib.get(), static_cast<int>(v.size()), static_cast<int>(idx.size() / 3), 0, 0);
        m.AddPartEXT(name, std::move(vb), std::move(ib), std::move(part));
    }

    static std::unique_ptr<SkinnedModelEXT> BuildTentacle(GraphicsDevice& d)
    {
        auto m = std::make_unique<SkinnedModelEXT>();
        Skeleton(*m);
        m->Clips["Wave"] = MakeClip(0);
        m->Clips["Curl"] = MakeClip(1);
        m->Clips["Twist"] = MakeClip(2);
        std::vector<VertexPositionNormalTextureSkinned> v;
        std::vector<std::uint16_t> idx;
        const float height = static_cast<float>(kBones - 1) * kSegment;
        for (int r = 0; r <= kRings; ++r) {
            const float y = height * static_cast<float>(r) / static_cast<float>(kRings);
            const int lower = std::min(static_cast<int>(y / kSegment), kBones - 2);
            const float f = (y - static_cast<float>(lower) * kSegment) / kSegment;   // 0 at the lower bone, 1 at the upper
            const Vector4 weights(1.0f - f, f, 0.0f, 0.0f);
            const std::array<std::uint8_t, 4> bones{static_cast<std::uint8_t>(lower), static_cast<std::uint8_t>(lower + 1), 0, 0};
            const float radius = 0.3f - 0.17f * y / height;
            for (int s = 0; s <= kSides; ++s) {
                const float a = MathHelper::TwoPi * static_cast<float>(s) / static_cast<float>(kSides);
                const Vector3 n(std::cos(a), 0.0f, std::sin(a));
                v.emplace_back(Vector3(n.X * radius, y, n.Z * radius), n, Vector2(static_cast<float>(s) / kSides * 2.0f, y * 0.8f), weights, bones);
            }
        }
        for (int r = 0; r < kRings; ++r)
            for (int s = 0; s < kSides; ++s) {
                const auto i0 = static_cast<std::uint16_t>(r * (kSides + 1) + s), i1 = static_cast<std::uint16_t>(i0 + 1);
                const auto i2 = static_cast<std::uint16_t>(i0 + kSides + 1), i3 = static_cast<std::uint16_t>(i2 + 1);
                idx.insert(idx.end(), {i0, i2, i1, i1, i2, i3});
            }
        AddPart(*m, d, "tube", v, idx);
        return m;
    }

    // The cap: a ball on the last bone. Built as its own model on the same skeleton, as a converted piece of clothing would be.
    static SkinnedModelEXT BuildCap(GraphicsDevice& d)
    {
        SkinnedModelEXT cap;
        Skeleton(cap);
        std::vector<VertexPositionNormalTextureSkinned> v;
        std::vector<std::uint16_t> idx;
        const float top = static_cast<float>(kBones - 1) * kSegment;
        constexpr int stacks = 10, slices = 16;
        const float radius = 0.2f;
        for (int i = 0; i <= stacks; ++i) {
            const float phi = MathHelper::Pi * static_cast<float>(i) / stacks;
            for (int j = 0; j <= slices; ++j) {
                const float th = MathHelper::TwoPi * static_cast<float>(j) / slices;
                const Vector3 n(std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th));
                v.emplace_back(Vector3(0.0f, top + radius * 0.6f, 0.0f) + n * radius, n, Vector2(static_cast<float>(j) / slices, static_cast<float>(i) / stacks), Vector4(1, 0, 0, 0), std::array<std::uint8_t, 4>{static_cast<std::uint8_t>(kBones - 1), 0, 0, 0});
            }
        }
        for (int i = 0; i < stacks; ++i)
            for (int j = 0; j < slices; ++j) {
                const auto i0 = static_cast<std::uint16_t>(i * (slices + 1) + j), i1 = static_cast<std::uint16_t>(i0 + 1);
                const auto i2 = static_cast<std::uint16_t>(i0 + slices + 1), i3 = static_cast<std::uint16_t>(i2 + 1);
                idx.insert(idx.end(), {i0, i2, i1, i1, i2, i3});
            }
        AddPart(cap, d, "cap", v, idx);
        return cap;
    }

    void Wear()
    {
        if (!model_) return;
        try {
            SkinnedModelEXT cap = BuildCap(*S().device);
            model_->AttachPartEXT(std::move(cap));
        } catch (const std::exception& e) {
            Museum::Log("skinext", "AttachPartEXT: %s", e.what());
            ReportError(std::string("AttachPartEXT: ") + e.what());
        }
    }
    void TakeOff()
    {
        if (!model_) return;
        try {
            model_->RemovePartEXT("cap");
        } catch (const std::exception& e) {
            Museum::Log("skinext", "RemovePartEXT: %s", e.what());
            ReportError(std::string("RemovePartEXT: ") + e.what());
        }
    }

    Gizmos gizmos_;
    std::unique_ptr<SkinnedEffect> effect_;
    std::unique_ptr<SkinnedModelEXT> model_;
    Vector3 plinth_{0, 0, 0};
    int clip_ = 0;
    float position_ = 0.0f, speed_ = 1.0f;
    bool ok_ = false, play_ = true, loop_ = true, skeleton_ = true, reported_ = false;
    std::string computeText_ = "-";
};

}  // namespace

std::unique_ptr<Exhibit> MakeSkinExtLab(const RoomDef& room, Services& services)
{
    return std::make_unique<SkinExtLab>(room, services);
}
