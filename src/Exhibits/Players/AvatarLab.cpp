// Avatar laboratory: CNA's implementation of XNA's avatar API (AvatarDescription, AvatarRenderer, AvatarAnimation) on the
// original avatars compiled into every CNA release. The avatar stands on a plinth and is drawn by AvatarRenderer into the
// museum's own scene: the renderer brings its own shaders, so the room only supplies the world, view and projection.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Rendering/Gizmos.hpp"
#include "Rendering/RenderStates.hpp"
#include "World/RoomBuilder.hpp"

#ifdef MUSEUM_NETWORKING
#include "Microsoft/Xna/Framework/GamerServices/AvatarAnimation.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarAnimationPreset.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBodyType.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBone.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarDescription.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarExpression.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarEye.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarEyebrow.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarMouth.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarRenderer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarRendererState.hpp"
#include "Microsoft/Xna/Framework/GamerServices/Gamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamerCollection.hpp"
#include "System/TimeSpan.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

#ifdef MUSEUM_NETWORKING
using namespace Microsoft::Xna::Framework::GamerServices;

struct Preset {
    AvatarAnimationPreset value;
    const char* name;
};
// Every avatar has the same 31 standard animations (all of them are listed); the female and male idles and emotes only suit their own body type, as in XNA.
constexpr Preset kPresets[] = {
    {AvatarAnimationPreset::Stand0, "Stand0"}, {AvatarAnimationPreset::Stand1, "Stand1"}, {AvatarAnimationPreset::Stand2, "Stand2"}, {AvatarAnimationPreset::Stand3, "Stand3"},
    {AvatarAnimationPreset::Stand4, "Stand4"}, {AvatarAnimationPreset::Stand5, "Stand5"}, {AvatarAnimationPreset::Stand6, "Stand6"}, {AvatarAnimationPreset::Stand7, "Stand7"},
    {AvatarAnimationPreset::Clap, "Clap"}, {AvatarAnimationPreset::Wave, "Wave"}, {AvatarAnimationPreset::Celebrate, "Celebrate"},
    {AvatarAnimationPreset::FemaleIdleLookAround, "FemaleIdleLookAround"}, {AvatarAnimationPreset::FemaleLaugh, "FemaleLaugh"}, {AvatarAnimationPreset::FemaleYawn, "FemaleYawn"},
    {AvatarAnimationPreset::MaleIdleLookAround, "MaleIdleLookAround"}, {AvatarAnimationPreset::MaleLaugh, "MaleLaugh"}, {AvatarAnimationPreset::MaleYawn, "MaleYawn"},
    {AvatarAnimationPreset::FemaleIdleCheckNails, "FemaleIdleCheckNails"}, {AvatarAnimationPreset::FemaleIdleShiftWeight, "FemaleIdleShiftWeight"}, {AvatarAnimationPreset::FemaleIdleFixShoe, "FemaleIdleFixShoe"},
    {AvatarAnimationPreset::FemaleAngry, "FemaleAngry"}, {AvatarAnimationPreset::FemaleConfused, "FemaleConfused"}, {AvatarAnimationPreset::FemaleCry, "FemaleCry"}, {AvatarAnimationPreset::FemaleShocked, "FemaleShocked"},
    {AvatarAnimationPreset::MaleIdleStretch, "MaleIdleStretch"}, {AvatarAnimationPreset::MaleIdleShiftWeight, "MaleIdleShiftWeight"}, {AvatarAnimationPreset::MaleIdleCheckHand, "MaleIdleCheckHand"},
    {AvatarAnimationPreset::MaleAngry, "MaleAngry"}, {AvatarAnimationPreset::MaleConfused, "MaleConfused"}, {AvatarAnimationPreset::MaleCry, "MaleCry"}, {AvatarAnimationPreset::MaleSurprised, "MaleSurprised"},
};
struct Bone {
    AvatarBone value;
    const char* name;
};
constexpr Bone kBones[] = {{AvatarBone::PropRight, "PropRight"}, {AvatarBone::PropLeft, "PropLeft"}, {AvatarBone::Head, "Head"},
                           {AvatarBone::WristRight, "WristRight"}, {AvatarBone::WristLeft, "WristLeft"}, {AvatarBone::Neck, "Neck"},
                           {AvatarBone::BackUpper, "BackUpper"}, {AvatarBone::ToeLeft, "ToeLeft"}, {AvatarBone::Root, "Root"},
                           {AvatarBone::BackLower, "BackLower"}, {AvatarBone::HipLeft, "HipLeft"}, {AvatarBone::HipRight, "HipRight"},
                           {AvatarBone::KneeLeft, "KneeLeft"}, {AvatarBone::KneeRight, "KneeRight"}, {AvatarBone::AnkleLeft, "AnkleLeft"},
                           {AvatarBone::CollarLeft, "CollarLeft"}, {AvatarBone::AnkleRight, "AnkleRight"}, {AvatarBone::CollarRight, "CollarRight"},
                           {AvatarBone::ShoulderLeft, "ShoulderLeft"}, {AvatarBone::ShoulderRight, "ShoulderRight"},
                           {AvatarBone::ToeRight, "ToeRight"}, {AvatarBone::ElbowLeft, "ElbowLeft"}, {AvatarBone::ElbowRight, "ElbowRight"},
                           {AvatarBone::FingerIndexLeft, "FingerIndexLeft"}, {AvatarBone::FingerMiddleLeft, "FingerMiddleLeft"},
                           {AvatarBone::FingerRingLeft, "FingerRingLeft"}, {AvatarBone::FingerSmallLeft, "FingerSmallLeft"},
                           {AvatarBone::SpecialLeft, "SpecialLeft"}, {AvatarBone::FingerThumbLeft, "FingerThumbLeft"},
                           {AvatarBone::FingerIndexRight, "FingerIndexRight"}, {AvatarBone::FingerMiddleRight, "FingerMiddleRight"},
                           {AvatarBone::FingerRingRight, "FingerRingRight"}, {AvatarBone::FingerSmallRight, "FingerSmallRight"},
                           {AvatarBone::SpecialRight, "SpecialRight"}, {AvatarBone::FingerThumbRight, "FingerThumbRight"},
                           {AvatarBone::FingerIndex2Left, "FingerIndex2Left"}, {AvatarBone::FingerMiddle2Left, "FingerMiddle2Left"},
                           {AvatarBone::FingerRing2Left, "FingerRing2Left"}, {AvatarBone::FingerSmall2Left, "FingerSmall2Left"},
                           {AvatarBone::FingerThumb2Left, "FingerThumb2Left"}, {AvatarBone::FingerIndex2Right, "FingerIndex2Right"},
                           {AvatarBone::FingerMiddle2Right, "FingerMiddle2Right"}, {AvatarBone::FingerRing2Right, "FingerRing2Right"},
                           {AvatarBone::FingerSmall2Right, "FingerSmall2Right"}, {AvatarBone::FingerThumb2Right, "FingerThumb2Right"},
                           {AvatarBone::FingerIndex3Left, "FingerIndex3Left"}, {AvatarBone::FingerMiddle3Left, "FingerMiddle3Left"},
                           {AvatarBone::FingerRing3Left, "FingerRing3Left"}, {AvatarBone::FingerSmall3Left, "FingerSmall3Left"},
                           {AvatarBone::FingerThumb3Left, "FingerThumb3Left"}, {AvatarBone::FingerIndex3Right, "FingerIndex3Right"},
                           {AvatarBone::FingerMiddle3Right, "FingerMiddle3Right"}, {AvatarBone::FingerRing3Right, "FingerRing3Right"},
                           {AvatarBone::FingerSmall3Right, "FingerSmall3Right"}, {AvatarBone::FingerThumb3Right, "FingerThumb3Right"}};
constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
#endif

class AvatarLab final : public Exhibit {
public:
    AvatarLab(const RoomDef& room, Services& services) : Exhibit(room, services), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        plinth_ = BuildStagePlinth(b, 0.0f, 0.0f, 0.4f);
        ControlPanel& p = AddPanel("Avatar", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_NETWORKING
        std::vector<std::string> presetNames;
        for (const Preset& preset : kPresets) presetNames.emplace_back(preset.name);
        p.AddChoice("AvatarAnimation preset (all 31)", presetNames,
                    9, [this](int v) { StartPreset(v); });
        p.AddToggle("Loop the animation", true, [this](bool v) { loop_ = v; });
        p.AddChoice("Body type", {"Female", "Male"}, 1, [this](int v) { bodyType_ = v == 0 ? AvatarBodyType::Female : AvatarBodyType::Male; NewAvatar(); });
        p.AddButton("AvatarDescription.CreateRandom", [this] { NewAvatar(); });
        p.AddButton("The Visitor's own (BeginGetFromGamer)", [this] { FromGamer(); });
        p.AddToggle("Override the face (AvatarExpression)", false, [this](bool v) { faceOverride_ = v; });
        p.AddChoice("AvatarEye", {"Neutral", "Sad", "Angry", "Confused", "Laughing", "Shocked", "Happy", "Yawning", "Sleeping", "LookUp", "LookDown", "LookLeft", "LookRight", "Blink"}, 6, [this](int v) { eye_ = v; });
        p.AddChoice("AvatarMouth", {"Neutral", "Sad", "Angry", "Confused", "Laughing", "Shocked", "Happy", "PhoneticO", "PhoneticAi", "PhoneticEe", "PhoneticFv", "PhoneticW", "PhoneticL", "PhoneticDth"}, 6, [this](int v) { mouth_ = v; });
        p.AddChoice("AvatarEyebrow", {"Neutral", "Sad", "Angry", "Confused", "Raised"}, 4, [this](int v) { eyebrow_ = v; });
        std::vector<std::string> boneNames{"none"};
        for (const Bone& bone : kBones) boneNames.emplace_back(bone.name);
        p.AddChoice("Mark a bone (AvatarBone, all 55)", boneNames, 1,
                    [this](int v) { bone_ = v - 1; });
        p.AddToggle("Draw the skeleton (ParentBones)", false, [this](bool v) { skeleton_ = v; });
        p.AddSlider("Turntable speed", 0.0f, 2.0f, spin_, [this](float v) { spin_ = v; });
        p.AddReadout("AvatarRenderer.State", [this] { return state_; });
        p.AddReadout("Body / height", [this] { return summary_; });
        p.AddReadout("Description", [this] { return std::string("1021 bytes, ") + (description_ && description_->getIsValidProperty() ? "valid" : "invalid"); });
        p.AddReadout("Animation", [this] { return animationText_; });
#else
        p.AddReadout("GamerServices", [] { return std::string("not built into this museum"); });
#endif
        SignSpec what = Kit::Plate("Avatars", {"CNA implements XNA's avatar API on original avatars: an AvatarDescription (1021 bytes) says who it is, an AvatarAnimation",
                                               "poses the 71-bone skeleton, and AvatarRenderer draws it with the world, view and projection you give it. Nothing here is a model file."});
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
#ifdef MUSEUM_NETWORKING
        NewAvatar();
        StartPreset(9);
#endif
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
#ifdef MUSEUM_NETWORKING
        if (animation_) {
            animation_->Update(System::TimeSpan::FromSeconds(static_cast<double>(ctx.dt)), loop_);
            char buf[96];
            std::snprintf(buf, sizeof buf, "%.1f of %.1f s", animation_->getCurrentPositionProperty().getTotalSecondsProperty(), animation_->getLengthProperty().getTotalSecondsProperty());
            animationText_ = buf;
        }
        if (renderer_) {
            switch (renderer_->getStateProperty()) {
                case AvatarRendererState::Loading: state_ = "Loading"; break;
                case AvatarRendererState::Ready: state_ = "Ready"; break;
                default: state_ = "Unavailable"; break;
            }
        }
        turn_ += ctx.dt * spin_ * 0.8f;
#endif
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
#ifdef MUSEUM_NETWORKING
        if (!renderer_ || !animation_) return;
        // Left-handed XNA avatars face +Z; the turntable turns them around the plinth, and the visitor looks from where the door is.
        renderer_->setWorldProperty(Matrix::CreateRotationY(turn_ + R().frameYaw + MathHelper::Pi) * Matrix::CreateTranslation(plinth_));
        renderer_->setViewProperty(ctx.view.view);
        renderer_->setProjectionProperty(ctx.view.projection);
        if (!faceOverride_) {
            renderer_->Draw(animation_.get());
        } else {
            AvatarExpression face;
            face.setLeftEyeProperty(static_cast<AvatarEye>(eye_));
            face.setRightEyeProperty(static_cast<AvatarEye>(eye_));
            face.setMouthProperty(static_cast<AvatarMouth>(mouth_));
            face.setLeftEyebrowProperty(static_cast<AvatarEyebrow>(eyebrow_));
            face.setRightEyebrowProperty(static_cast<AvatarEyebrow>(eyebrow_));
            const auto bones = animation_->getBoneTransformsProperty();
            renderer_->Draw(std::vector<Matrix>(bones.begin(), bones.end()), face);
        }
        RenderStates::SetOpaque3D(ctx.device);   // the renderer set its own states
        if ((bone_ >= 0 || skeleton_) && renderer_->getStateProperty() == AvatarRendererState::Ready) {
            // The animation gives each bone relative to its parent (71 of them): a bone's place in the avatar is its own
            // transform times its parent's, up to the root. The renderer names each bone's parent.
            const auto local = animation_->getBoneTransformsProperty();
            const auto parents = renderer_->getParentBonesProperty();
            const auto bind = renderer_->getBindPoseProperty();
            const int count = std::min({local.getCountProperty(), parents.getCountProperty(), bind.getCountProperty()});
            std::vector<Matrix> model(static_cast<std::size_t>(count));
            const Matrix place = Matrix::CreateRotationY(turn_ + R().frameYaw + MathHelper::Pi) * Matrix::CreateTranslation(plinth_);
            for (int i = 0; i < count; ++i) {
                const int parent = parents[i];
                // The animation turns each bone; where the bone sits relative to its parent is the bind pose's.
                Matrix m = local[i];
                m.setTranslationProperty(static_cast<Matrix>(bind[i]).getTranslationProperty());
                model[static_cast<std::size_t>(i)] = parent >= 0 && parent < i ? m * model[static_cast<std::size_t>(parent)] : m;
            }
            if (skeleton_) {
                for (int i = 0; i < count; ++i) {
                    const int parent = parents[i];
                    if (parent >= 0 && parent < count)
                        gizmos_.Line((model[static_cast<std::size_t>(parent)] * place).getTranslationProperty(), (model[static_cast<std::size_t>(i)] * place).getTranslationProperty(), Color(120, 255, 160, 255));
                }
            }
            const std::size_t index = bone_ >= 0 ? static_cast<std::size_t>(kBones[bone_].value) : 0;
            if (bone_ >= 0 && static_cast<int>(index) < count) {
                const Matrix world = model[index] * place;
                gizmos_.Axes(world, 0.22f);
                gizmos_.SolidSphere(ctx, world.getTranslationProperty(), 0.035f, Vector3(1.0f, 0.8f, 0.2f));
            }
            gizmos_.Flush(ctx, true);
        }
#else
        (void)ctx;
#endif
    }

private:
#ifdef MUSEUM_NETWORKING
    void NewAvatar()
    {
        try {
            description_ = std::make_unique<AvatarDescription>(AvatarDescription::CreateRandom(bodyType_));
            MakeRenderer();
        } catch (const std::exception& e) {
            Museum::Log("avatar", "CreateRandom: %s", e.what());
            ReportError(std::string("CreateRandom: ") + e.what());   // local generation: nothing here depends on a service
        }
    }
    void FromGamer()
    {
        try {
            auto* gamers = Gamer::getSignedInGamersProperty();
            if (gamers == nullptr || gamers->getCountProperty() == 0) return;
            System::IAsyncResult* r = AvatarDescription::BeginGetFromGamer((*gamers)[0], {}, std::any{});
            description_ = std::make_unique<AvatarDescription>(AvatarDescription::EndGetFromGamer(r));
            delete r;
            if (description_->getBodyTypeProperty() != bodyType_) StartPreset(9);
            MakeRenderer();
        } catch (const std::exception& e) {
            Museum::Log("avatar", "BeginGetFromGamer: %s", e.what());
            summary_ = std::string("BeginGetFromGamer: ") + std::string(e.what()).substr(0, 48);
        }
    }
    void MakeRenderer()
    {
        renderer_ = std::make_unique<AvatarRenderer>(description_.get(), true);
        renderer_->setLightDirectionProperty(Vector3(-0.4f, -0.5f, -0.75f));
        renderer_->setLightColorProperty(Vector3(0.75f, 0.72f, 0.68f));
        renderer_->setAmbientLightColorProperty(Vector3(0.42f, 0.42f, 0.46f));
        char buf[96];
        std::snprintf(buf, sizeof buf, "%s, %.2f m", description_->getBodyTypeProperty() == AvatarBodyType::Male ? "male" : "female", static_cast<double>(description_->getHeightProperty()));
        summary_ = buf;
    }
    void StartPreset(int index)
    {
        animation_ = std::make_unique<AvatarAnimation>(kPresets[std::clamp(index, 0, kPresetCount - 1)].value);
    }

    std::unique_ptr<AvatarDescription> description_;
    std::unique_ptr<AvatarRenderer> renderer_;
    std::unique_ptr<AvatarAnimation> animation_;
    AvatarBodyType bodyType_ = AvatarBodyType::Male;
    bool faceOverride_ = false;
    int eye_ = 6, mouth_ = 6, eyebrow_ = 4;
    bool loop_ = true;
    float spin_ = 0.3f, turn_ = 0.0f;
    int bone_ = 0;
    bool skeleton_ = false;
    std::string state_ = "-", summary_ = "-", animationText_ = "-";
#endif
    Gizmos gizmos_;
    Vector3 plinth_{0, 0, 0};
};

}  // namespace

std::unique_ptr<Exhibit> MakeAvatarLab(const RoomDef& room, Services& services)
{
    return std::make_unique<AvatarLab>(room, services);
}
