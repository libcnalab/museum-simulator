// Camera laboratory: each room shows a small scene through a different kind of camera on a live screen,
// with controls for the camera's parameters and a gizmo of the camera inside the scene.
#include <algorithm>
#include <cmath>
#include <string>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/MiniView.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Orbit, Chase, TopDown, Security, Fov, Clip, Transition, Split, Mirror };

Mode ParseMode(const std::string& s)
{
    if (s == "chase") return Mode::Chase;
    if (s == "topdown") return Mode::TopDown;
    if (s == "security") return Mode::Security;
    if (s == "fov") return Mode::Fov;
    if (s == "clip") return Mode::Clip;
    if (s == "transition") return Mode::Transition;
    if (s == "split") return Mode::Split;
    if (s == "mirror") return Mode::Mirror;
    return Mode::Orbit;
}

struct Pose {
    Vector3 eye, target;
};

class CameraLab final : public Exhibit {
public:
    CameraLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Orbit:
                p.AddSlider("Distance", 2.5f, 9.0f, dist_, [this](float v) { dist_ = v; });
                p.AddSlider("Elevation", 5.0f, 80.0f, elev_, [this](float v) { elev_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Orbit speed", -2.0f, 2.0f, speed_, [this](float v) { speed_ = v; });
                p.AddHeading("The camera circles its target");
                p.AddReadout("Eye", [this] { return Fmt(Current().eye - Stage()); });
                break;
            case Mode::Chase:
                p.AddSlider("Follow distance", 1.0f, 5.0f, dist_, [this](float v) { dist_ = v; });
                p.AddSlider("Height", 0.5f, 3.0f, height_, [this](float v) { height_ = v; });
                p.AddSlider("Spring stiffness", 0.5f, 12.0f, stiff_, [this](float v) { stiff_ = v; }, "%.1f");
                p.AddHeading("Low stiffness lags behind; high sticks to the car");
                p.AddReadout("Lag (eye to ideal)", [this] { return Fmt(Vector3::Distance(chaseEye_, Ideal())) + " m"; });
                break;
            case Mode::TopDown:
                p.AddToggle("Orthographic", true, [this](bool v) { ortho_ = v; });
                p.AddSlider("Height", 4.0f, 14.0f, dist_, [this](float v) { dist_ = v; });
                p.AddSlider("View size (ortho)", 3.0f, 12.0f, size_, [this](float v) { size_ = v; });
                p.AddHeading("Maps and minimaps use this camera");
                break;
            case Mode::Security:
                p.AddSlider("Sweep range", 0.0f, 3.5f, range_, [this](float v) { range_ = v; });
                p.AddSlider("Sweep speed", 0.1f, 2.0f, speed_, [this](float v) { speed_ = v; });
                p.AddSlider("Zoom (FOV)", 15.0f, 90.0f, fov_, [this](float v) { fov_ = v; }, "%.0f deg", 1.0f);
                p.AddHeading("A fixed camera that only pans");
                break;
            case Mode::Fov:
                p.AddSlider("Left camera FOV", 15.0f, 120.0f, fov_, [this](float v) { fov_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Right camera FOV", 15.0f, 120.0f, fov2_, [this](float v) { fov2_ = v; }, "%.0f deg", 1.0f);
                p.AddHeading("Same position, different field of view");
                p.AddReadout("Left  M22 = 1/tan(fov/2)", [this] { return Fmt(1.0f / std::tan(MathHelper::ToRadians(fov_) * 0.5f)); });
                p.AddReadout("Right M22", [this] { return Fmt(1.0f / std::tan(MathHelper::ToRadians(fov2_) * 0.5f)); });
                break;
            case Mode::Clip:
                p.AddSlider("Near plane", 0.05f, 6.0f, near_, [this](float v) { near_ = v; }, "%.2f");
                p.AddSlider("Far plane", 4.0f, 60.0f, far_, [this](float v) { far_ = v; }, "%.1f");
                p.AddHeading("Anything outside [near, far] is clipped");
                p.AddReadout("Far / near ratio", [this] { return Fmt(std::max(far_, near_ + 0.5f) / near_); });
                p.AddReadout("Depth buffer", [] { return std::string("24-bit; high ratios z-fight"); });
                break;
            case Mode::Transition:
                p.AddChoice("Easing", {"Lerp", "SmoothStep"}, 1, [this](int v) { smooth_ = v == 1; });
                p.AddSlider("Duration", 0.3f, 4.0f, duration_, [this](float v) { duration_ = v; }, "%.1f s");
                p.AddButton("Go to view 1: wide", [this] { GoTo(0); });
                p.AddButton("Go to view 2: close-up", [this] { GoTo(1); });
                p.AddButton("Go to view 3: high", [this] { GoTo(2); });
                p.AddButton("Go to view 4: low", [this] { GoTo(3); });
                p.AddHeading("Position: Vector3.Lerp/SmoothStep.  Direction: Quaternion.Slerp");
                break;
            case Mode::Split:
                p.AddSlider("Left camera orbit", -2.0f, 2.0f, speed_, [this](float v) { speed_ = v; });
                p.AddSlider("Divider position", 0.2f, 0.8f, split_, [this](float v) { split_ = v; });
                p.AddHeading("Two Viewports in one render target");
                p.AddReadout("Left viewport", [this] { return std::to_string(static_cast<int>(split_ * 960.0f)) + " x 360"; });
                p.AddReadout("Right viewport", [this] { return std::to_string(960 - static_cast<int>(split_ * 960.0f)) + " x 360"; });
                break;
            case Mode::Mirror:
                p.AddToggle("Flip horizontally (mirror)", true, [this](bool v) { flip_ = v; });
                p.AddSlider("Rear view FOV", 40.0f, 110.0f, fov_, [this](float v) { fov_ = v; }, "%.0f deg", 1.0f);
                p.AddHeading("A camera at your eyes, looking backwards");
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        const int w = mode_ == Mode::Split ? 960 : 640, h = mode_ == Mode::Split ? 360 : 360;
        a_ = std::make_unique<MiniView>(S(), w, h);
        if (mode_ == Mode::Fov) b_ = std::make_unique<MiniView>(S(), 480, 300), a_ = std::make_unique<MiniView>(S(), 480, 300);
        chaseEye_ = Ideal();
        from_ = to_ = Pose{Stage() + Vector3(0, 3, 6), Stage()};
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        player_ = ctx.playerPosition;
        forward_ = ctx.playerForward;
        if (mode_ == Mode::Chase) {
            const float k = 1.0f - std::exp(-stiff_ * ctx.dt);
            chaseEye_ = Vector3::Lerp(chaseEye_, Ideal(), k);   // move the fraction k of the remaining way each frame
        }
        if (mode_ == Mode::Transition && moving_) {
            progress_ = std::min(1.0f, progress_ + ctx.dt / duration_);
            if (progress_ >= 1.0f) moving_ = false;
        }
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (!a_) return;
        const auto scene = [this](const DrawContext& sub) { DrawScene(sub, true); };
        const float aspect = a_->Aspect();
        switch (mode_) {
            case Mode::Fov: {
                const Pose p = Fixed();
                a_->Render(ctx, Matrix::CreateLookAt(p.eye, p.target, Vector3::Up), Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(fov_), aspect, 0.1f, 60.0f), p.eye, scene);
                b_->Render(ctx, Matrix::CreateLookAt(p.eye, p.target, Vector3::Up), Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(fov2_), aspect, 0.1f, 60.0f), p.eye, scene);
                break;
            }
            case Mode::Split: RenderSplit(ctx); break;
            case Mode::Mirror: {
                const Vector3 back = -Vector3::Normalize(forward_ + Vector3(1e-4f, 0, 0));
                const Vector3 eye = player_;
                a_->Render(ctx, Matrix::CreateLookAt(eye, eye + back, Vector3::Up), Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(fov_), aspect, 0.15f, 60.0f), eye, [this](const DrawContext& sub) {
                    DrawWorld(sub);
                });
                break;
            }
            default: {
                const Pose p = Current();
                Matrix view = Matrix::CreateLookAt(p.eye, p.target, mode_ == Mode::TopDown ? Off(0, 0, -1) : Vector3::Up);
                Matrix proj;
                if (mode_ == Mode::TopDown && ortho_) proj = Matrix::CreateOrthographic(size_, size_ / aspect, 0.5f, 40.0f);
                else if (mode_ == Mode::Clip) proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(60.0f), aspect, near_, std::max(far_, near_ + 0.5f));
                else proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(mode_ == Mode::Security ? fov_ : 60.0f), aspect, 0.1f, 80.0f);
                a_->Render(ctx, view, proj, p.eye, scene);
            }
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        DrawScene(ctx, false);
        // The live screens.
        const Vector3 centre = R().ToWorld(0.0f, 1.9f, -5.2f);
        if (mode_ == Mode::Fov) {
            a_->DrawInRoom(ctx, R().ToWorld(-2.0f, 1.9f, -5.2f), R().frameYaw, 3.5f);
            b_->DrawInRoom(ctx, R().ToWorld(2.0f, 1.9f, -5.2f), R().frameYaw, 3.5f);
            gizmos_.Label(R().ToWorld(-2.0f, 3.9f, -5.2f), "FOV " + std::to_string(static_cast<int>(fov_)), Color(255, 230, 120, 255));
            gizmos_.Label(R().ToWorld(2.0f, 3.9f, -5.2f), "FOV " + std::to_string(static_cast<int>(fov2_)), Color(255, 230, 120, 255));
        } else if (mode_ == Mode::Mirror && a_ && a_->HasContent()) {
            RenderStates::SetOpaque3D(ctx.device);
            ctx.device.setRasterizerStateProperty(RasterizerState::CullNone);
            const Vector3 r = Off(1.0f * (flip_ ? -1.0f : 1.0f) * 4.0f, 0, 0), up(0, 4.0f / a_->Aspect(), 0);
            const Vector3 o = centre - r * 0.5f - up * 0.5f;
            QuadDraw::Textured(ctx, a_->Texture(), o, r, up);
        } else if (a_) {
            a_->DrawInRoom(ctx, centre, R().frameYaw, mode_ == Mode::Split ? 5.0f : 4.4f);
        }
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        switch (mode_) {
            case Mode::Orbit: return "Orbit camera";
            case Mode::Chase: return "Chase camera";
            case Mode::TopDown: return "Top-down camera";
            case Mode::Security: return "Fixed security camera";
            case Mode::Fov: return "Field of view";
            case Mode::Clip: return "Near and far planes";
            case Mode::Transition: return "Camera transitions";
            case Mode::Split: return "Split screen with Viewports";
            default: return "Rear-view mirror";
        }
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Orbit: return {"An orbit camera keeps a target in the middle and moves on a sphere around it:", "CreateLookAt(eye, target, up) is rebuilt every frame from distance, elevation and an angle."};
            case Mode::Chase: return {"A chase camera aims for a spot behind the subject and eases towards it, so it lags in turns.", "The spring is exponential smoothing: eye += (ideal - eye) * (1 - e^(-k dt))."};
            case Mode::TopDown: return {"Looking straight down gives a map. Orthographic keeps distances true; perspective shows depth.", "CreateOrthographic takes the visible width and height in metres."};
            case Mode::Security: return {"A fixed camera cannot move, only pan and zoom. Narrow the field of view to zoom in.", "The gizmo in the scene is the camera's own BoundingFrustum."};
            case Mode::Fov: return {"Two cameras in the same place with different fields of view: a wide angle shows more and bends", "perspective at the edges, a narrow one compresses depth like a telephoto lens."};
            case Mode::Clip: return {"The near plane cuts away what is too close, the far plane what is too far. Pushing near down and far", "up (large ratio) wastes depth-buffer precision: watch the two overlapping quads shimmer."};
            case Mode::Transition: return {"Moving a camera between poses: positions interpolate with Vector3.Lerp or SmoothStep, and the", "direction with Quaternion.Slerp so it turns at a steady rate."};
            case Mode::Split: return {"Split-screen sets GraphicsDevice.Viewport to a part of the target and draws once per player.", "Here both views are drawn into one RenderTarget2D."};
            default: return {"A rear-view mirror is a camera at your eyes looking backwards, drawn mirrored. Turn around", "and watch the screen show what is behind you."};
        }
    }

    [[nodiscard]] Vector3 Stage() const { return R().ToWorld(1.5f, 0.02f, 1.0f); }
    [[nodiscard]] float Angle() const { return time_ * (mode_ == Mode::Chase ? 0.6f : speed_); }
    [[nodiscard]] Vector3 Subject() const { return Stage() + Vector3(std::cos(time_ * 0.6f) * 2.2f, 0.25f, std::sin(time_ * 0.6f) * 2.2f); }
    [[nodiscard]] Vector3 Heading() const { return Vector3(-std::sin(time_ * 0.6f), 0, std::cos(time_ * 0.6f)); }
    [[nodiscard]] Vector3 Ideal() const { return Subject() - Heading() * dist_ + Vector3(0, height_, 0); }
    [[nodiscard]] Pose Fixed() const { return {Stage() + Vector3(-2.6f, 1.8f, 3.2f), Stage() + Vector3(0, 0.4f, 0)}; }
    [[nodiscard]] Pose Current() const
    {
        const Vector3 st = Stage() + Vector3(0, 0.5f, 0);
        switch (mode_) {
            case Mode::Orbit: {
                const float e = MathHelper::ToRadians(elev_), a = Angle();
                return {st + Vector3(std::cos(a) * std::cos(e), std::sin(e), std::sin(a) * std::cos(e)) * dist_, st};
            }
            case Mode::Chase: return {chaseEye_, Subject() + Vector3(0, 0.2f, 0)};
            case Mode::TopDown: return {Stage() + Vector3(0, dist_, 0.001f), Stage()};
            case Mode::Security: return {Stage() + Vector3(-3.0f, 2.4f, 3.2f), st + Vector3(std::sin(time_ * speed_) * range_, 0, 0)};
            case Mode::Clip: return {Stage() + Vector3(0.0f, 1.6f, 6.0f), Stage() + Vector3(0, 0.3f, 0)};
            case Mode::Transition: {
                // The position eases with the Vector3 call the control names; the turn uses the same eased fraction.
                const float t = smooth_ ? MathHelper::SmoothStep(0, 1, progress_) : progress_;
                const Vector3 eye = smooth_ ? Vector3::SmoothStep(from_.eye, to_.eye, progress_) : Vector3::Lerp(from_.eye, to_.eye, progress_);
                const Quaternion qa = Look(from_.eye, from_.target), qb = Look(to_.eye, to_.target);
                const Vector3 fwd = Vector3::Transform(Vector3::Forward, Quaternion::Slerp(qa, qb, t));
                return {eye, eye + fwd * 4.0f};
            }
            default: return Fixed();
        }
    }
    static Quaternion Look(const Vector3& eye, const Vector3& target)
    {
        const Vector3 f = Vector3::Normalize(target - eye);
        return Quaternion::CreateFromRotationMatrix(Matrix::CreateWorld(Vector3::Zero, f, Vector3::Up));
    }
    void GoTo(int view)
    {
        const Vector3 st = Stage();
        const Pose views[4] = {{st + Vector3(0, 3.0f, 7.0f), st}, {st + Vector3(1.5f, 1.0f, 2.8f), st + Vector3(0, 0.4f, 0)},
                               {st + Vector3(0.5f, 7.0f, 1.5f), st}, {st + Vector3(-3.0f, 0.4f, 3.0f), st + Vector3(0, 0.6f, 0)}};
        from_ = Current();
        to_ = views[view];
        progress_ = 0.0f;
        moving_ = true;
    }

    void RenderSplit(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        const int w = 960, h = 360, lw = static_cast<int>(split_ * static_cast<float>(w));
        const SceneView sv(Matrix::getIdentityProperty(), Matrix::getIdentityProperty(), Vector3::Zero);
        device.SetRenderTarget(a_->Target());
        device.Clear(Color(30, 36, 50, 255), 1.0f);
        const Viewport full = device.getViewportProperty();
        const auto pass = [&](int x, int vw, const Pose& p) {
            device.setViewportProperty(Viewport(x, 0, vw, h));
            const Matrix view = Matrix::CreateLookAt(p.eye, p.target, Vector3::Up);
            const Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(60.0f), static_cast<float>(vw) / static_cast<float>(h), 0.1f, 60.0f);
            const SceneView sub(view, proj, p.eye);
            DrawContext c{device, ctx.fx, sub, ctx.time, true};
            ctx.fx.SetCamera(view, proj);
            RenderStates::SetOpaque3D(device);
            device.Clear(ClearOptions::DepthBuffer, Color(0, 0, 0, 0), 1.0f, 0);
            DrawWorld(c);
        };
        const Vector3 st = Stage() + Vector3(0, 0.5f, 0);
        const float a = time_ * speed_;
        pass(0, lw, {st + Vector3(std::cos(a) * 5.0f, 2.5f, std::sin(a) * 5.0f), st});
        pass(lw, w - lw, {Subject() - Heading() * 2.2f + Vector3(0, 1.1f, 0), Subject()});
        device.setViewportProperty(full);
        device.SetRenderTarget(nullptr);
        a_->MarkRendered();
    }

    void DrawWorld(const DrawContext& ctx)
    {
        DrawDiorama(ctx, S(), Stage(), time_, 2.0f);
        // The moving car.
        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPunctualLightEXT(PunctualLightEXT{});
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setSpecularColorProperty(Vector3(0.3f, 0.3f, 0.3f));
        fx.setDiffuseColorProperty(Vector3(0.95f, 0.75f, 0.2f));
        fx.setEmissiveColorProperty(Vector3(0.05f, 0.04f, 0.0f));
        const float yaw = std::atan2(Heading().X, Heading().Z);
        fx.setWorldProperty(Matrix::CreateScale(0.45f, 0.3f, 0.8f) * Matrix::CreateRotationY(yaw) * Matrix::CreateTranslation(Subject()));
        EffectBank::Apply(fx);
        S().shapes->Cube().Draw(ctx.device);
        if (mode_ == Mode::Clip) DrawClipTargets(ctx);
    }

    void DrawClipTargets(const DrawContext& ctx)
    {
        // Two nearly coplanar quads far from the eye: with a huge far/near ratio they z-fight.
        BasicEffect& fx = ctx.fx.Flat();
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        QuadDraw::Coloured(ctx, Stage() + Vector3(-3.0f, 0.02f, -6.0f), Vector3(1.6f, 0, 0), Vector3(0, 0, 1.6f), Color(230, 70, 70, 255));
        QuadDraw::Coloured(ctx, Stage() + Vector3(-3.0f, 0.0205f, -6.0f), Vector3(1.6f, 0, 0), Vector3(0, 0, 1.6f), Color(70, 90, 230, 255));
        // A row of posts at increasing distance shows which ones the clip planes remove.
        BasicEffect& lit = ctx.fx.BeginLit();
        lit.EnableDefaultLighting();
        lit.setPunctualLightEXT(PunctualLightEXT{});
        lit.setViewProperty(ctx.view.view);
        lit.setProjectionProperty(ctx.view.projection);
        lit.setDiffuseColorProperty(Vector3(0.6f, 0.9f, 0.6f));
        for (int i = 0; i < 12; ++i) {
            lit.setWorldProperty(Matrix::CreateScale(0.3f, 1.2f, 0.3f) * Matrix::CreateTranslation(Stage() + Vector3(1.2f, 0.6f, 5.0f - static_cast<float>(i) * 2.2f)));
            EffectBank::Apply(lit);
            S().shapes->Cube().Draw(ctx.device);
        }
    }

    void DrawScene(const DrawContext& ctx, bool inside)
    {
        if (inside) {
            DrawWorld(ctx);
            return;
        }
        // In the room: the diorama, plus the camera gizmo.
        DrawWorld(ctx);
        if (mode_ == Mode::Mirror) return;
        const Pose p = mode_ == Mode::Fov ? Fixed() : Current();
        gizmos_.SolidSphere(ctx, p.eye, 0.1f, kRed);
        gizmos_.Line(p.eye, p.target, Color(255, 120, 100, 255));
        const float aspect = 16.0f / 9.0f;
        Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(mode_ == Mode::Fov ? fov_ : (mode_ == Mode::Security ? fov_ : 60.0f)), aspect, 0.3f, mode_ == Mode::Clip ? near_ + (far_ - near_) * 0.2f : 3.0f);
        if (mode_ == Mode::Clip) proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(60.0f), aspect, near_, std::max(far_, near_ + 0.5f));
        gizmos_.Frustum(BoundingFrustum(Matrix::CreateLookAt(p.eye, p.target, mode_ == Mode::TopDown ? Off(0, 0, -1) : Vector3::Up) * proj), Color(255, 200, 80, 255));
        gizmos_.Label(p.eye + Vector3(0, 0.25f, 0), "camera", ToColor(kRed * 1.3f), 0.5f);
    }

    Mode mode_;
    Gizmos gizmos_;
    std::unique_ptr<MiniView> a_, b_;
    float time_ = 0.0f;
    float dist_ = 5.0f, elev_ = 30.0f, speed_ = 0.5f, height_ = 1.4f, stiff_ = 3.0f, size_ = 8.0f, range_ = 2.0f, fov_ = 55.0f, fov2_ = 100.0f;
    float near_ = 0.5f, far_ = 24.0f, duration_ = 1.5f, split_ = 0.5f;
    bool ortho_ = true, smooth_ = true, flip_ = true, moving_ = false;
    float progress_ = 1.0f;
    Vector3 chaseEye_, player_, forward_{0, 0, -1};
    Pose from_, to_;
};

}  // namespace

std::unique_ptr<Exhibit> MakeCameraLab(const RoomDef& room, Services& services)
{
    return std::make_unique<CameraLab>(room, services);
}
