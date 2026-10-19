// Procedural animation laboratory: motion from maths and hierarchies of matrices, not from stored clips.
// Eight rooms: a pendulum wave, a clock, meshing gears, a conveyor, proximity doors, a kinetic sculpture,
// a gyroscope and a walking figure. One class ("mode"); every pose is Matrix * Matrix * ... drawn with BasicEffect.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <string>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/MeshBuilder.hpp"
#include "Rendering/RenderStates.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Pendulum, Clock, Gears, Conveyor, Doors, Kinetic, Gyroscope, Walker };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"clock", Mode::Clock}, {"gears", Mode::Gears}, {"conveyor", Mode::Conveyor}, {"doors", Mode::Doors},
                                                         {"kinetic", Mode::Kinetic}, {"gyroscope", Mode::Gyroscope}, {"walker", Mode::Walker}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Pendulum;
}

float Ease(float t) { return MathHelper::SmoothStep(0.0f, 1.0f, std::clamp(t, 0.0f, 1.0f)); }

class AnimLab final : public Exhibit {
public:
    AnimLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Pendulum:
                p.AddSlider("Gravity", 2.0f, 20.0f, gravity_, [this](float v) { gravity_ = v; }, "%.1f");
                p.AddSlider("Damping", 0.0f, 0.3f, damping_, [this](float v) { damping_ = v; });
                p.AddToggle("Simulate (off = exact formula)", false, [this](bool v) { integrate_ = v; });
                p.AddButton("Restart together", [this] { phase_ = 0.0f; for (auto& s : state_) { s.angle = 0.6f; s.omega = 0.0f; } });
                p.AddHeading("Each pendulum has a slightly different length");
                break;
            case Mode::Clock:
                p.AddSlider("Time speed", 1.0f, 3600.0f, speed_, [this](float v) { speed_ = v; }, "%.0fx", 1.0f);
                p.AddReadout("Time", [this] { return TimeText(); });
                break;
            case Mode::Gears:
                p.AddSlider("Motor speed", -3.0f, 3.0f, speed_, [this](float v) { speed_ = v; });
                p.AddSlider("Driven gear teeth", 8.0f, 28.0f, static_cast<float>(teeth2_), [this](float v) { teeth2_ = static_cast<int>(v); gearDirty_ = true; }, "%.0f", 1.0f);
                p.AddReadout("Gear ratio", [this] { return Fmt(static_cast<float>(teeth2_) / 12.0f) + " : 1"; });
                p.AddReadout("Driven speed", [this] { return Fmt(-speed_ * 12.0f / static_cast<float>(teeth2_)) + " rad/s"; });
                break;
            case Mode::Conveyor:
                p.AddSlider("Belt speed", 0.0f, 4.0f, speed_, [this](float v) { speed_ = v; });
                p.AddToggle("Gate enabled", true, [this](bool v) { gate_ = v; });
                p.AddReadout("Items through the gate", [this] { return std::to_string(counted_); });
                break;
            case Mode::Doors:
                p.AddSlider("Trigger distance", 1.0f, 6.0f, trigger_, [this](float v) { trigger_ = v; }, "%.1f m");
                p.AddSlider("Open time", 0.2f, 3.0f, openTime_, [this](float v) { openTime_ = v; }, "%.1f s");
                p.AddReadout("Left door (Lerp)", [this] { return Fmt(doorA_); });
                p.AddReadout("Right door (SmoothStep)", [this] { return Fmt(Ease(doorB_)); });
                p.AddReadout("Distance to you", [this] { return Fmt(distance_) + " m"; });
                break;
            case Mode::Kinetic:
                p.AddSlider("Wave length", 0.1f, 1.0f, wave_, [this](float v) { wave_ = v; });
                p.AddSlider("Speed", 0.0f, 3.0f, speed_, [this](float v) { speed_ = v; });
                p.AddChoice("Pattern", {"Travelling wave", "Twist", "Lissajous"}, 0, [this](int v) { pattern_ = v; });
                break;
            case Mode::Gyroscope:
                p.AddSlider("Outer ring (X)", -3.0f, 3.0f, rates_[0], [this](float v) { rates_[0] = v; });
                p.AddSlider("Middle ring (Y)", -3.0f, 3.0f, rates_[1], [this](float v) { rates_[1] = v; });
                p.AddSlider("Inner ring (Z)", -3.0f, 3.0f, rates_[2], [this](float v) { rates_[2] = v; });
                p.AddHeading("Each ring is the child of the one outside it");
                break;
            case Mode::Walker:
                p.AddSlider("Walk speed", 0.0f, 3.0f, speed_, [this](float v) { speed_ = v; });
                p.AddSlider("Stride", 0.1f, 1.0f, stride_, [this](float v) { stride_ = v; });
                p.AddToggle("Show the joints", false, [this](bool v) { joints_ = v; });
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        for (int i = 0; i < 8; ++i) state_[static_cast<std::size_t>(i)] = {0.6f, 0.0f};
        MeshBuilder gear;
        BuildGearMesh(12, 0.62f);
        gearDirty_ = true;
        (void)gear;
        belt_ = 0.0f;
        for (int i = 0; i < 6; ++i) items_[static_cast<std::size_t>(i)] = static_cast<float>(i) * 1.1f;
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        const float dt = std::min(ctx.dt, 0.05f);
        time_ += dt;
        phase_ += dt;
        distance_ = Vector3::Distance(ctx.playerPosition, DoorCentre());
        if (mode_ == Mode::Pendulum && integrate_)
            for (int i = 0; i < 8; ++i) {
                Pend& s = state_[static_cast<std::size_t>(i)];
                const float len = Length(i);
                for (int k = 0; k < 4; ++k) {   // semi-implicit Euler, four substeps
                    s.omega += (-gravity_ / len * std::sin(s.angle) - damping_ * s.omega) * (dt / 4.0f);
                    s.angle += s.omega * (dt / 4.0f);
                }
            }
        if (mode_ == Mode::Conveyor) {
            belt_ = std::fmod(belt_ + speed_ * dt, 1.0f);
            for (float& x : items_) {
                x += speed_ * dt;
                if (x > 6.6f) x -= 6.6f * 1.0f;
                const float prev = x - speed_ * dt;
                if (gate_ && prev < 3.0f && x >= 3.0f) ++counted_;
            }
        }
        if (mode_ == Mode::Doors) {
            const bool open = distance_ < trigger_;
            const float step = dt / openTime_;
            doorA_ = std::clamp(doorA_ + (open ? step : -step), 0.0f, 1.0f);
            doorB_ = std::clamp(doorB_ + (open ? step : -step), 0.0f, 1.0f);
            doorA_ = MathHelper::Lerp(0.0f, 1.0f, doorA_);
        }
        if (mode_ == Mode::Clock) clockSeconds_ += dt * speed_;
        if (mode_ == Mode::Gears) gearAngle_ += dt * speed_;
        // The gait phase stays in (-pi, pi]: a phase that only grows loses float precision after a long visit and the stride jitters.
        if (mode_ == Mode::Walker) walk_ = MathHelper::WrapAngle(walk_ + dt * speed_ * 4.0f);
        if (mode_ == Mode::Gyroscope)
            for (int i = 0; i < 3; ++i) ring_[static_cast<std::size_t>(i)] += dt * rates_[static_cast<std::size_t>(i)];
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!fx_) fx_ = std::make_unique<BasicEffect>(*S().device);
        ctx.device.setRasterizerStateProperty(RasterizerState::CullClockwise);
        const Vector3 o = R().ToWorld(0.0f, 0.0f, 0.0f);
        switch (mode_) {
            case Mode::Pendulum: DrawPendulum(ctx, o); break;
            case Mode::Clock: DrawClock(ctx, o); break;
            case Mode::Gears: DrawGears(ctx, o); break;
            case Mode::Conveyor: DrawConveyor(ctx, o); break;
            case Mode::Doors: DrawDoors(ctx, o); break;
            case Mode::Kinetic: DrawKinetic(ctx, o); break;
            case Mode::Gyroscope: DrawGyro(ctx, o); break;
            case Mode::Walker: DrawWalker(ctx, o); break;
        }
        RenderStates::SetOpaque3D(ctx.device);
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    struct Pend { float angle, omega; };
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Pendulum wave", "A mechanical clock", "Meshing gears", "A conveyor belt", "Proximity doors", "Kinetic sculpture", "A gyroscope", "A walking figure"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Pendulum: return {"Eight pendulums whose lengths are chosen so their periods differ by a fixed step drift in and out of phase", "and trace waves. The angle comes either from the closed formula or from numerically integrating the equation."};
            case Mode::Clock: return {"The hands are a hierarchy of rotations: seconds, minutes and hours each turn about the same pivot by an", "angle derived from the time. Speed it up to watch the hour hand move; the digital time reads the same value."};
            case Mode::Gears: return {"Meshing gears turn in opposite directions and their speeds are in the inverse ratio of their tooth counts.", "The driven gear's angle is simply -motor * teeth1 / teeth2: ratios are matrix rotations."};
            case Mode::Conveyor: return {"The belt's texture offset scrolls while crates travel along it at the same speed, so they seem to ride it.", "A gate counts every crate whose position crosses a line between two frames: a swept test, not a position test."};
            case Mode::Doors: return {"Doors that open as you approach: each frame the target is open or closed and the door moves towards it.", "The left door moves linearly, the right with SmoothStep easing: same time, different feel."};
            case Mode::Kinetic: return {"A hundred rods with the same motion and a phase that depends on position make a wave: a kinetic sculpture.", "Change the wave length or the pattern: only the phase formula differs."};
            case Mode::Gyroscope: return {"Three rings, each rotating inside the previous one: the world matrix of the inner ring is its rotation", "times the middle ring's, times the outer ring's. Hierarchies compose by matrix multiplication."};
            default: return {"A figure animated only by sines: hips bob, thighs swing and shins follow with a phase lag. Every limb is a", "child of the one above it, so forward kinematics gives the foot positions without any stored animation."};
        }
    }

    BasicEffect& Lit(const DrawContext& ctx, const Matrix& world, const Vector3& colour, Texture2D* tex = nullptr)
    {
        BasicEffect& fx = *fx_;
        fx.setLightingEnabledProperty(true);
        fx.EnableDefaultLighting();
        fx.setTextureEnabledProperty(true);
        fx.setTextureProperty(tex != nullptr ? tex : S().textures->White());
        fx.setVertexColorEnabledProperty(false);
        fx.setPreferPerPixelLightingProperty(true);
        fx.setDiffuseColorProperty(colour);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setSpecularColorProperty(Vector3(0.3f, 0.3f, 0.3f));
        fx.setSpecularPowerProperty(30.0f);
        fx.setAlphaProperty(1.0f);
        fx.setFogEnabledProperty(false);
        fx.setPunctualLightEXT(PunctualLightEXT{});
        fx.setWorldProperty(world);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        return fx;
    }
    void Draw(const DrawContext& ctx, const LitMesh& mesh, const Matrix& world, const Vector3& colour, Texture2D* tex = nullptr)
    {
        BasicEffect& fx = Lit(ctx, world, colour, tex);
        EffectBank::Apply(fx);
        mesh.Draw(ctx.device);
    }
    // A bar from `bottom` to `top` of the given thickness, as a scaled cube.
    void Bar(const DrawContext& ctx, const Matrix& parent, const Vector3& from, const Vector3& to, float thick, const Vector3& colour)
    {
        const Vector3 d = to - from;
        const float len = d.Length();
        if (len < 1e-4f) return;
        Draw(ctx, S().shapes->Cylinder(), Matrix::CreateScale(thick, len, thick) * AlignYTo(d / len) * Matrix::CreateTranslation(from) * parent, colour);
    }

    // ---- pendulum wave ------------------------------------------------------------------------
    [[nodiscard]] float Length(int i) const
    {
        // Periods T_i = 2*pi*sqrt(L/g) differing by a constant step: L_i = g * (T0 / (2 pi) * k_i)^2.
        const float k = 1.0f + static_cast<float>(i) * 0.06f;
        return 2.4f / (k * k);
    }
    void DrawPendulum(const DrawContext& ctx, const Vector3& o)
    {
        const Vector3 top = o + Vector3(0, 4.2f, 0);
        Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(5.6f, 0.15f, 0.3f) * Matrix::CreateTranslation(top + Vector3(0, 0.1f, 0)), Vector3(0.45f, 0.4f, 0.4f));
        for (int i = 0; i < 8; ++i) {
            const float len = Length(i) * 1.1f;
            float angle;
            if (integrate_) angle = state_[static_cast<std::size_t>(i)].angle;
            else {
                const float w = std::sqrt(gravity_ / Length(i));
                angle = 0.6f * std::cos(w * phase_) * std::exp(-damping_ * phase_ * 0.5f);
            }
            const Vector3 pivot = top + Vector3(-2.45f + static_cast<float>(i) * 0.7f, 0, 0);
            const Vector3 bob = pivot + Vector3(0, -std::cos(angle) * len, std::sin(angle) * len);
            gizmos_.Line(pivot, bob, Color(210, 210, 220, 255));
            const Vector3 col = i % 2 ? Vector3(0.95f, 0.55f, 0.2f) : Vector3(0.3f, 0.7f, 0.95f);
            gizmos_.SolidSphere(ctx, bob, 0.17f, col);
        }
        (void)o;
    }

    // ---- clock ---------------------------------------------------------------------------------
    [[nodiscard]] float ClockSeconds() const
    {
        const std::time_t t = std::time(nullptr);
        std::tm tmv{};
#if defined(_WIN32)
        localtime_s(&tmv, &t);   // the C runtime's argument order differs from POSIX localtime_r
#else
        localtime_r(&t, &tmv);
#endif
        return static_cast<float>(tmv.tm_hour * 3600 + tmv.tm_min * 60 + tmv.tm_sec) + clockSeconds_;
    }
    [[nodiscard]] std::string TimeText() const
    {
        const int s = static_cast<int>(ClockSeconds()) % 86400;
        char buf[16];
        std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
        return buf;
    }
    void DrawClock(const DrawContext& ctx, const Vector3& o)
    {
        const Vector3 c = o + Vector3(0, 2.4f, 0);
        const Matrix face = Matrix::CreateTranslation(c);
        Draw(ctx, S().shapes->Cylinder(), Matrix::CreateScale(3.4f, 0.2f, 3.4f) * Matrix::CreateRotationX(MathHelper::PiOver2) * face, Vector3(0.9f, 0.88f, 0.8f));
        for (int i = 0; i < 12; ++i) {
            const float a = static_cast<float>(i) / 12.0f * MathHelper::TwoPi;
            Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(0.1f, i % 3 == 0 ? 0.35f : 0.2f, 0.1f) * Matrix::CreateTranslation(0, 1.45f, 0.14f) * Matrix::CreateRotationZ(-a) * face, Vector3(0.1f, 0.1f, 0.12f));
        }
        const float s = ClockSeconds();
        const float angS = -(std::fmod(s, 60.0f) / 60.0f) * MathHelper::TwoPi;
        const float angM = -(std::fmod(s, 3600.0f) / 3600.0f) * MathHelper::TwoPi;
        const float angH = -(std::fmod(s, 43200.0f) / 43200.0f) * MathHelper::TwoPi;
        Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(0.14f, 0.95f, 0.07f) * Matrix::CreateTranslation(0, 0.475f, 0.22f) * Matrix::CreateRotationZ(angH) * face, Vector3(0.1f, 0.1f, 0.12f));
        Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(0.1f, 1.35f, 0.06f) * Matrix::CreateTranslation(0, 0.675f, 0.3f) * Matrix::CreateRotationZ(angM) * face, Vector3(0.15f, 0.15f, 0.2f));
        Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(0.04f, 1.5f, 0.04f) * Matrix::CreateTranslation(0, 0.65f, 0.36f) * Matrix::CreateRotationZ(angS) * face, Vector3(0.9f, 0.15f, 0.1f));
        Draw(ctx, S().shapes->Sphere(), Matrix::CreateScale(0.2f) * Matrix::CreateTranslation(0, 0, 0.38f) * face, Vector3(0.7f, 0.65f, 0.3f));
    }

    // ---- gears ---------------------------------------------------------------------------------
    void BuildGearMesh(int teeth, float radius)
    {
        MeshBuilder mb;
        mb.AddCylinder(Vector3(0, -0.12f, 0), radius, 0.24f, 32, 1.0f, Vector3(1, 1, 1), true);
        for (int i = 0; i < teeth; ++i) {
            const float a = static_cast<float>(i) / static_cast<float>(teeth) * MathHelper::TwoPi;
            const float tw = radius * MathHelper::TwoPi / static_cast<float>(teeth) * 0.4f;
            const Vector3 centre(std::cos(a) * (radius + 0.06f), 0, std::sin(a) * (radius + 0.06f));
            // A tooth: a small box aligned with the radius (axis-aligned boxes only, so approximate with a rotated cone).
            mb.AddCone(Vector3(centre.X - std::cos(a) * 0.08f, -0.12f, centre.Z - std::sin(a) * 0.08f), tw, tw * 0.5f, 0.24f, 6, Vector3(1, 1, 1));
        }
        gearMeshes_[teeth] = std::make_unique<LitMesh>(*S().device, mb);
    }
    [[nodiscard]] const LitMesh& GearMesh(int teeth)
    {
        if (!gearMeshes_.count(teeth)) BuildGearMesh(teeth, 0.62f * static_cast<float>(teeth) / 12.0f);
        return *gearMeshes_[teeth];
    }
    void DrawGears(const DrawContext& ctx, const Vector3& o)
    {
        const float r1 = 0.62f, r2 = r1 * static_cast<float>(teeth2_) / 12.0f;
        const Vector3 c1 = o + Off(-r2 * 0.5f - 0.1f, 2.2f, 0.0f), c2 = c1 + Vector3(r1 + r2 + 0.12f, 0, 0);
        const float a1 = gearAngle_, a2 = -gearAngle_ * 12.0f / static_cast<float>(teeth2_) + MathHelper::Pi / static_cast<float>(teeth2_);
        // Gears lie in the XY plane: rotate the Y-axis meshes onto Z.
        const Matrix flat = Matrix::CreateRotationX(MathHelper::PiOver2);
        Draw(ctx, GearMesh(12), Matrix::CreateRotationY(a1) * flat * Matrix::CreateTranslation(c1), Vector3(0.85f, 0.65f, 0.2f), S().textures->Get("metal"));
        Draw(ctx, GearMesh(teeth2_), Matrix::CreateRotationY(a2) * flat * Matrix::CreateTranslation(c2), Vector3(0.6f, 0.7f, 0.85f), S().textures->Get("metal"));
        Bar(ctx, Matrix::getIdentityProperty(), c1 - Vector3(0, 0, 0.5f), c1 + Vector3(0, 0, 0.5f), 0.12f, Vector3(0.3f, 0.3f, 0.35f));
        gizmos_.Label(c1 + Vector3(0, r1 + 0.45f, 0), "12 teeth (motor)", Color(255, 230, 150, 255), 0.5f);
        gizmos_.Label(c2 + Vector3(0, r2 + 0.45f, 0), std::to_string(teeth2_) + " teeth", Color(255, 230, 150, 255), 0.5f);
    }

    // ---- conveyor --------------------------------------------------------------------------------
    void DrawConveyor(const DrawContext& ctx, const Vector3& o)
    {
        const Vector3 start = o + Off(-3.3f, 0.7f, 0.0f);
        // The belt is a long quad whose texture coordinates scroll.
        BasicEffect& fx = ctx.fx.World();
        fx.setTextureProperty(S().textures->Get("darkgrid"));
        fx.setTextureEnabledProperty(true);
        fx.setVertexColorEnabledProperty(true);
        fx.setDiffuseColorProperty(Vector3(1, 1, 1));
        fx.setWorldProperty(Matrix::CreateTranslation(start));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        ctx.device.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
        ctx.device.setRasterizerStateProperty(RasterizerState::CullNone);
        EffectBank::Apply(fx);
        const float u0 = -belt_ * 1.0f, u1 = u0 + 6.6f;
        const VertexPositionColorTexture q[6] = {{Vector3(0, 0, 0.5f), Color::White, Vector2(u0, 1)}, {Vector3(6.6f, 0, 0.5f), Color::White, Vector2(u1, 1)}, {Vector3(6.6f, 0, -0.5f), Color::White, Vector2(u1, 0)},
                                                 {Vector3(0, 0, 0.5f), Color::White, Vector2(u0, 1)}, {Vector3(6.6f, 0, -0.5f), Color::White, Vector2(u1, 0)}, {Vector3(0, 0, -0.5f), Color::White, Vector2(u0, 0)}};
        ctx.device.DrawUserPrimitives(PrimitiveType::TriangleList, q, 0, 2);
        ctx.device.setRasterizerStateProperty(RasterizerState::CullClockwise);
        Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(6.8f, 0.5f, 0.1f) * Matrix::CreateTranslation(start + Vector3(3.3f, -0.26f, 0.55f)), Vector3(0.4f, 0.4f, 0.45f));
        Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(6.8f, 0.5f, 0.1f) * Matrix::CreateTranslation(start + Vector3(3.3f, -0.26f, -0.55f)), Vector3(0.4f, 0.4f, 0.45f));
        for (std::size_t i = 0; i < items_.size(); ++i) {
            const float x = items_[i];
            if (x < 0 || x > 6.6f) continue;
            const Vector3 col = i % 2 ? Vector3(0.85f, 0.5f, 0.2f) : Vector3(0.3f, 0.65f, 0.9f);
            Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(0.55f, 0.4f, 0.55f) * Matrix::CreateTranslation(start + Vector3(x, 0.22f, 0)), col, S().textures->Get("wood"));
        }
        const Vector3 gate = start + Vector3(3.0f, 0, 0);
        gizmos_.Box(BoundingBox(gate + Vector3(-0.02f, 0.0f, -0.7f), gate + Vector3(0.02f, 1.1f, 0.7f)), gate_ ? Color(110, 255, 140, 255) : Color(120, 120, 130, 255));
        gizmos_.Label(gate + Vector3(0, 1.3f, 0), "counted: " + std::to_string(counted_), Color(150, 255, 170, 255), 0.55f);
    }

    // ---- doors -----------------------------------------------------------------------------------
    [[nodiscard]] Vector3 DoorCentre() const { return R().ToWorld(0.0f, 1.2f, -2.0f); }
    void DrawDoors(const DrawContext& ctx, const Vector3& o)
    {
        const Vector3 base = o + Off(0, 0, -2.0f);
        Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(6.6f, 0.3f, 0.5f) * Matrix::CreateTranslation(base + Vector3(0, 2.75f, 0)), Vector3(0.35f, 0.38f, 0.45f));
        for (int pass = 0; pass < 2; ++pass) {
            const float open = pass == 0 ? doorA_ : Ease(doorB_);
            const float zOff = pass == 0 ? 0.12f : -0.12f;
            Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(1.5f, 2.6f, 0.1f) * Matrix::CreateTranslation(base + Off(-0.8f - open * 1.4f, 1.3f, zOff)), Vector3(0.8f, 0.85f, 0.95f), S().textures->Get("metal"));
            Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(1.5f, 2.6f, 0.1f) * Matrix::CreateTranslation(base + Off(0.8f + open * 1.4f, 1.3f, zOff)), Vector3(0.8f, 0.85f, 0.95f), S().textures->Get("metal"));
            if (pass == 0) gizmos_.Label(base + Off(-3.0f, 2.2f, 0), "Lerp", Color(255, 230, 150, 255), 0.5f);
        }
        gizmos_.Label(base + Off(3.0f, 2.2f, 0), "SmoothStep (front pair)", Color(255, 230, 150, 255), 0.5f);
        gizmos_.Box(BoundingBox(base + Off(-trigger_, 0, -trigger_), base + Off(trigger_, 0.05f, trigger_)), distance_ < trigger_ ? Color(110, 255, 140, 255) : Color(200, 200, 210, 255));
    }

    // ---- kinetic ---------------------------------------------------------------------------------
    void DrawKinetic(const DrawContext& ctx, const Vector3& o)
    {
        const int n = 60;
        for (int i = 0; i < n; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(n - 1);
            const float x = (t - 0.5f) * 8.0f;
            float angle = 0.0f, lift = 0.0f;
            const float ph = time_ * speed_;
            if (pattern_ == 0) {
                angle = std::sin(ph - t * MathHelper::TwoPi / wave_) * 0.9f;
                lift = std::sin(ph * 1.2f - t * MathHelper::TwoPi / wave_) * 0.4f;
            } else if (pattern_ == 1) {
                angle = ph + t * MathHelper::TwoPi / wave_;
            } else {
                angle = std::sin(ph * 1.3f + t * 11.0f * wave_) * 1.3f;
                lift = std::cos(ph * 0.9f + t * 7.0f) * 0.5f;
            }
            const Vector3 pivot = o + Off(x, 2.4f + lift, 0);
            const Vector3 col(0.4f + 0.6f * t, 0.5f + 0.3f * std::sin(t * 9.0f), 1.0f - 0.7f * t);
            Draw(ctx, S().shapes->Cylinder(), Matrix::CreateScale(0.07f, 1.8f, 0.07f) * Matrix::CreateTranslation(0, -0.9f, 0) * Matrix::CreateRotationZ(angle) * Matrix::CreateTranslation(pivot), col);
            Draw(ctx, S().shapes->Sphere(), Matrix::CreateScale(0.2f) * Matrix::CreateTranslation(0, -1.8f, 0) * Matrix::CreateRotationZ(angle) * Matrix::CreateTranslation(pivot), col);
        }
    }

    // ---- gyroscope -------------------------------------------------------------------------------
    void DrawGyro(const DrawContext& ctx, const Vector3& o)
    {
        const Matrix root = Matrix::CreateTranslation(o + Vector3(0, 2.3f, 0));
        const Matrix outer = Matrix::CreateRotationX(ring_[0]) * root;
        const Matrix middle = Matrix::CreateRotationY(ring_[1]) * outer;
        const Matrix inner = Matrix::CreateRotationZ(ring_[2]) * middle;
        const auto ring = [&](const Matrix& m, float radius, const Vector3& colour, int axis) {
            const Matrix flat = axis == 0 ? Matrix::CreateRotationZ(MathHelper::PiOver2) : (axis == 1 ? Matrix::getIdentityProperty() : Matrix::CreateRotationX(MathHelper::PiOver2));
            Draw(ctx, S().shapes->Cylinder(), Matrix::CreateScale(radius * 2.0f, 0.12f, radius * 2.0f) * Matrix::CreateTranslation(0, -0.06f, 0) * flat * m, colour);
        };
        ring(outer, 1.7f, Vector3(0.9f, 0.4f, 0.3f), 0);
        ring(middle, 1.35f, Vector3(0.4f, 0.85f, 0.4f), 1);
        ring(inner, 1.0f, Vector3(0.4f, 0.55f, 0.95f), 2);
        Draw(ctx, S().shapes->Sphere(), Matrix::CreateScale(0.7f) * inner, Vector3(0.9f, 0.8f, 0.3f), S().textures->Get("marble"));
        gizmos_.Axes(inner, 0.9f);
        gizmos_.Label(o + Vector3(0, 4.5f, 0), "outer X  >  middle Y  >  inner Z", Color(255, 230, 150, 255), 0.5f);
    }

    // ---- walker ----------------------------------------------------------------------------------
    void DrawWalker(const DrawContext& ctx, const Vector3& o)
    {
        const float w = walk_;
        const float bob = std::abs(std::sin(w)) * 0.08f;
        const Matrix body = Matrix::CreateRotationY(time_ * 0.35f * (speed_ > 0.01f ? 1.0f : 0.0f)) * Matrix::CreateTranslation(o + Vector3(0, 1.2f + bob, 0));
        const Matrix ring = Matrix::CreateTranslation(o);
        // The figure walks on a circle: body orbits, legs swing relative to it.
        const Matrix orbit = Matrix::CreateTranslation(1.8f, 0, 0) * Matrix::CreateRotationY(-time_ * 0.35f * speed_) * ring;
        const Matrix hips = Matrix::CreateTranslation(0, 1.05f + bob, 0) * Matrix::CreateRotationY(MathHelper::PiOver2) * orbit;
        (void)body;
        Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(0.4f, 0.7f, 0.25f) * Matrix::CreateTranslation(0, 0.45f, 0) * hips, Vector3(0.3f, 0.5f, 0.85f));
        Draw(ctx, S().shapes->Sphere(), Matrix::CreateScale(0.3f) * Matrix::CreateTranslation(0, 1.0f, 0) * hips, Vector3(0.95f, 0.8f, 0.65f));
        for (int side = 0; side < 2; ++side) {
            const float ph = w + (side ? MathHelper::Pi : 0.0f);
            const float thigh = std::sin(ph) * stride_;
            const float shin = std::max(0.0f, -std::sin(ph - 0.6f)) * stride_ * 1.4f;
            const Matrix hip = Matrix::CreateTranslation(0, 0, side ? 0.12f : -0.12f) * hips;
            const Matrix hipJoint = Matrix::CreateRotationZ(thigh) * hip;
            Draw(ctx, S().shapes->Cylinder(), Matrix::CreateScale(0.14f, 0.5f, 0.14f) * Matrix::CreateTranslation(0, -0.5f, 0) * hipJoint, Vector3(0.25f, 0.3f, 0.45f));
            const Matrix knee = Matrix::CreateRotationZ(shin) * Matrix::CreateTranslation(0, -0.5f, 0) * hipJoint;
            Draw(ctx, S().shapes->Cylinder(), Matrix::CreateScale(0.12f, 0.5f, 0.12f) * Matrix::CreateTranslation(0, -0.5f, 0) * knee, Vector3(0.3f, 0.35f, 0.55f));
            Draw(ctx, S().shapes->Cube(), Matrix::CreateScale(0.3f, 0.1f, 0.14f) * Matrix::CreateTranslation(0.08f, -1.02f, 0) * knee, Vector3(0.2f, 0.2f, 0.25f));
            if (joints_) {
                gizmos_.SolidSphere(ctx, Vector3::Transform(Vector3::Zero, hipJoint), 0.07f, Vector3(1, 0.4f, 0.3f));
                gizmos_.SolidSphere(ctx, Vector3::Transform(Vector3::Zero, knee), 0.07f, Vector3(0.4f, 1, 0.4f));
            }
        }
        gizmos_.Grid(Vector3(o.X, R().centre.Y + 0.03f, o.Z), 2.4f, 0.4f, Color(80, 90, 120, 255));
    }

    Mode mode_;
    Gizmos gizmos_;
    std::unique_ptr<BasicEffect> fx_;
    std::map<int, std::unique_ptr<LitMesh>> gearMeshes_;
    std::array<Pend, 8> state_{};
    std::array<float, 6> items_{};
    std::array<float, 3> ring_{}, rates_{1.2f, -0.8f, 1.9f};
    float time_ = 0.0f, phase_ = 0.0f, gravity_ = 9.8f, damping_ = 0.02f, speed_ = 1.0f, clockSeconds_ = 0.0f, gearAngle_ = 0.0f, belt_ = 0.0f;
    float trigger_ = 3.5f, openTime_ = 1.0f, doorA_ = 0.0f, doorB_ = 0.0f, distance_ = 99.0f, wave_ = 0.5f, walk_ = 0.0f, stride_ = 0.5f;
    bool integrate_ = false, gate_ = true, gearDirty_ = true, joints_ = false;
    int teeth2_ = 18, pattern_ = 0, counted_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeAnimLab(const RoomDef& room, Services& services)
{
    return std::make_unique<AnimLab>(room, services);
}
