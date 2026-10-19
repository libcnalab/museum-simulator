// Math, checked: every XNA by-reference overload of the math types ("..., out T result" in C#, a reference parameter here)
// is computed next to its value-returning twin from the same inputs and compared bit for bit, and a few contracts at the
// edges are checked against XNA 4.0's own behaviour. Each check logs
//     [museum][check] math-checks <name>: PASS|FAIL (expected ..., got ...)
// so a run's log is the witness. Inputs are fixed numbers, never random, so a FAIL is reproducible.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Plane.hpp"
#include "Microsoft/Xna/Framework/Point.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

// Bitwise equality of the values' own fields (not padding): the two overloads must produce the identical floats.
bool Bits(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }
bool Same(float a, float b) { return Bits(a, b); }
bool Same(bool a, bool b) { return a == b; }
bool Same(ContainmentType a, ContainmentType b) { return a == b; }
bool Same(PlaneIntersectionType a, PlaneIntersectionType b) { return a == b; }
bool Same(const std::optional<float>& a, const std::optional<float>& b) { return a.has_value() == b.has_value() && (!a || Bits(*a, *b)); }
bool Same(const Vector2& a, const Vector2& b) { return Bits(a.X, b.X) && Bits(a.Y, b.Y); }
bool Same(const Vector3& a, const Vector3& b) { return Bits(a.X, b.X) && Bits(a.Y, b.Y) && Bits(a.Z, b.Z); }
bool Same(const Vector4& a, const Vector4& b) { return Bits(a.X, b.X) && Bits(a.Y, b.Y) && Bits(a.Z, b.Z) && Bits(a.W, b.W); }
bool Same(const Quaternion& a, const Quaternion& b) { return Bits(a.X, b.X) && Bits(a.Y, b.Y) && Bits(a.Z, b.Z) && Bits(a.W, b.W); }
bool Same(const Plane& a, const Plane& b) { return Same(a.Normal, b.Normal) && Bits(a.D, b.D); }
bool Same(const Rectangle& a, const Rectangle& b) { return a.X == b.X && a.Y == b.Y && a.Width == b.Width && a.Height == b.Height; }
bool Same(const BoundingBox& a, const BoundingBox& b) { return Same(a.Min, b.Min) && Same(a.Max, b.Max); }
bool Same(const BoundingSphere& a, const BoundingSphere& b) { return Same(a.Center, b.Center) && Bits(a.Radius, b.Radius); }
bool Same(const Matrix& a, const Matrix& b)
{
    const float x[16] = {a.M11, a.M12, a.M13, a.M14, a.M21, a.M22, a.M23, a.M24, a.M31, a.M32, a.M33, a.M34, a.M41, a.M42, a.M43, a.M44};
    const float y[16] = {b.M11, b.M12, b.M13, b.M14, b.M21, b.M22, b.M23, b.M24, b.M31, b.M32, b.M33, b.M34, b.M41, b.M42, b.M43, b.M44};
    for (int i = 0; i < 16; ++i)
        if (!Bits(x[i], y[i])) return false;
    return true;
}

struct MathCheck {
    std::string name;
    bool pass = false;
    std::string detail;
};

class MathChecksLab final : public ScreenExhibit {
public:
    MathChecksLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Math, checked", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddButton("Run every check again", [this] { Run(); });
        p.AddReadout("Passed / failed", [this] { return std::to_string(passed_) + " / " + std::to_string(failed_); });
        AddInfoSign(b, "Two ways to call the same maths",
                    {"XNA gives most math functions twice: one returns the result, one writes it through an out parameter (fast on the Xbox).",
                     "Here every by-reference overload runs next to its twin on the same numbers; both must give the identical bits."});
    }

    void OnScreenLoaded() override { Run(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.6f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        text("Math, checked: " + std::to_string(passed_) + " passed, " + std::to_string(failed_) + " failed", 24, 12, Kit::kGold, 0.9f);
        float y = 70.0f;
        int shown = 0;
        for (const MathCheck& c : checks_)
            if (!c.pass && shown < 10) {
                text("FAIL " + c.name + "  " + c.detail.substr(0, 60), 24, y, Kit::Rgba(255, 120, 110), 0.55f);
                y += 34.0f;
                ++shown;
            }
        if (shown == 0) text("Every by-reference overload agreed with its value twin, and every edge contract held.", 24, y, Kit::Rgba(120, 230, 140), 0.65f);
        sb.End();
    }

private:
    template <typename T>
    void Twin(const char* name, const T& value, const T& out)
    {
        Add(std::string(name), Same(value, out), "value and by-reference results identical", Same(value, out) ? "identical" : "different");
    }

    void Add(const std::string& name, bool pass, const std::string& expected, const std::string& got)
    {
        checks_.push_back({name, pass, got});
        (pass ? passed_ : failed_)++;
        Museum::Log("check", "%s %s: %s (expected %s, got %s)", R().key.c_str(), name.c_str(), pass ? "PASS" : "FAIL", expected.c_str(), got.c_str());
    }

    void Run()
    {
        checks_.clear();
        passed_ = failed_ = 0;
        try {
            Vectors();
            Matrices();
            Quaternions();
            Volumes();
            Edges();
        } catch (const std::exception& e) {
            Add("the check run itself", false, "no exception", e.what());
        }
    }

    void Vectors()
    {
        const Vector2 a2(1.5f, -2.25f), b2(0.75f, 3.5f), c2(-4.0f, 0.125f), d2(2.0f, 2.0f), n2 = Vector2::Normalize(Vector2(1, 1));
        const Vector3 a3(1.5f, -2.25f, 0.5f), b3(0.75f, 3.5f, -1.25f), c3(-4.0f, 0.125f, 2.0f), d3(2.0f, 2.0f, -2.0f), n3 = Vector3::Normalize(Vector3(1, 2, 3));
        const Vector4 a4(1.5f, -2.25f, 0.5f, 3.0f), b4(0.75f, 3.5f, -1.25f, -0.5f), c4(-4.0f, 0.125f, 2.0f, 1.0f), d4(2.0f, 2.0f, -2.0f, 2.0f);
        const Matrix m = Matrix::CreateFromYawPitchRoll(0.3f, -0.2f, 0.7f) * Matrix::CreateTranslation(1.0f, -2.0f, 3.0f);
        const Quaternion q = Quaternion::CreateFromYawPitchRoll(0.4f, 0.1f, -0.3f);
        {
            Vector2 o;
            Vector2::Add(a2, b2, o); Twin("Vector2.Add(ref)", Vector2::Add(a2, b2), o);
            Vector2::Barycentric(a2, b2, c2, 0.3f, 0.2f, o); Twin("Vector2.Barycentric(ref)", Vector2::Barycentric(a2, b2, c2, 0.3f, 0.2f), o);
            Vector2::CatmullRom(a2, b2, c2, d2, 0.4f, o); Twin("Vector2.CatmullRom(ref)", Vector2::CatmullRom(a2, b2, c2, d2, 0.4f), o);
            Vector2::Clamp(c2, a2, d2, o); Twin("Vector2.Clamp(ref)", Vector2::Clamp(c2, a2, d2), o);
            float f = 0;
            Vector2::Distance(a2, b2, f); Twin("Vector2.Distance(ref)", Vector2::Distance(a2, b2), f);
            Vector2::DistanceSquared(a2, b2, f); Twin("Vector2.DistanceSquared(ref)", Vector2::DistanceSquared(a2, b2), f);
            Vector2::Divide(a2, b2, o); Twin("Vector2.Divide(vector, ref)", Vector2::Divide(a2, b2), o);
            Vector2::Divide(a2, 3.0f, o); Twin("Vector2.Divide(scalar, ref)", Vector2::Divide(a2, 3.0f), o);
            Vector2::Dot(a2, b2, f); Twin("Vector2.Dot(ref)", Vector2::Dot(a2, b2), f);
            Vector2::Hermite(a2, b2, c2, d2, 0.6f, o); Twin("Vector2.Hermite(ref)", Vector2::Hermite(a2, b2, c2, d2, 0.6f), o);
            Vector2::Lerp(a2, b2, 0.35f, o); Twin("Vector2.Lerp(ref)", Vector2::Lerp(a2, b2, 0.35f), o);
            Vector2::Max(a2, b2, o); Twin("Vector2.Max(ref)", Vector2::Max(a2, b2), o);
            Vector2::Min(a2, b2, o); Twin("Vector2.Min(ref)", Vector2::Min(a2, b2), o);
            Vector2::Multiply(a2, b2, o); Twin("Vector2.Multiply(vector, ref)", Vector2::Multiply(a2, b2), o);
            Vector2::Multiply(a2, -1.5f, o); Twin("Vector2.Multiply(scalar, ref)", Vector2::Multiply(a2, -1.5f), o);
            Vector2::Negate(a2, o); Twin("Vector2.Negate(ref)", Vector2::Negate(a2), o);
            Vector2::Normalize(a2, o); Twin("Vector2.Normalize(ref)", Vector2::Normalize(a2), o);
            Vector2::Reflect(a2, n2, o); Twin("Vector2.Reflect(ref)", Vector2::Reflect(a2, n2), o);
            Vector2::SmoothStep(a2, b2, 0.25f, o); Twin("Vector2.SmoothStep(ref)", Vector2::SmoothStep(a2, b2, 0.25f), o);
            Vector2::Subtract(a2, b2, o); Twin("Vector2.Subtract(ref)", Vector2::Subtract(a2, b2), o);
            Vector2::Transform(a2, m, o); Twin("Vector2.Transform(matrix, ref)", Vector2::Transform(a2, m), o);
            Vector2::Transform(a2, q, o); Twin("Vector2.Transform(quaternion, ref)", Vector2::Transform(a2, q), o);
            Vector2::TransformNormal(a2, m, o); Twin("Vector2.TransformNormal(ref)", Vector2::TransformNormal(a2, m), o);
        }
        {
            Vector3 o;
            float f = 0;
            Vector3::Add(a3, b3, o); Twin("Vector3.Add(ref)", Vector3::Add(a3, b3), o);
            Vector3::Barycentric(a3, b3, c3, 0.3f, 0.2f, o); Twin("Vector3.Barycentric(ref)", Vector3::Barycentric(a3, b3, c3, 0.3f, 0.2f), o);
            Vector3::CatmullRom(a3, b3, c3, d3, 0.4f, o); Twin("Vector3.CatmullRom(ref)", Vector3::CatmullRom(a3, b3, c3, d3, 0.4f), o);
            Vector3::Clamp(c3, a3, d3, o); Twin("Vector3.Clamp(ref)", Vector3::Clamp(c3, a3, d3), o);
            Vector3::Cross(a3, b3, o); Twin("Vector3.Cross(ref)", Vector3::Cross(a3, b3), o);
            Vector3::Distance(a3, b3, f); Twin("Vector3.Distance(ref)", Vector3::Distance(a3, b3), f);
            Vector3::DistanceSquared(a3, b3, f); Twin("Vector3.DistanceSquared(ref)", Vector3::DistanceSquared(a3, b3), f);
            Vector3::Divide(a3, b3, o); Twin("Vector3.Divide(vector, ref)", Vector3::Divide(a3, b3), o);
            Vector3::Divide(a3, 3.0f, o); Twin("Vector3.Divide(scalar, ref)", Vector3::Divide(a3, 3.0f), o);
            Vector3::Dot(a3, b3, f); Twin("Vector3.Dot(ref)", Vector3::Dot(a3, b3), f);
            Vector3::Hermite(a3, b3, c3, d3, 0.6f, o); Twin("Vector3.Hermite(ref)", Vector3::Hermite(a3, b3, c3, d3, 0.6f), o);
            Vector3::Lerp(a3, b3, 0.35f, o); Twin("Vector3.Lerp(ref)", Vector3::Lerp(a3, b3, 0.35f), o);
            Vector3::Max(a3, b3, o); Twin("Vector3.Max(ref)", Vector3::Max(a3, b3), o);
            Vector3::Min(a3, b3, o); Twin("Vector3.Min(ref)", Vector3::Min(a3, b3), o);
            Vector3::Multiply(a3, b3, o); Twin("Vector3.Multiply(vector, ref)", Vector3::Multiply(a3, b3), o);
            Vector3::Multiply(a3, -1.5f, o); Twin("Vector3.Multiply(scalar, ref)", Vector3::Multiply(a3, -1.5f), o);
            Vector3::Negate(a3, o); Twin("Vector3.Negate(ref)", Vector3::Negate(a3), o);
            Vector3::Normalize(a3, o); Twin("Vector3.Normalize(ref)", Vector3::Normalize(a3), o);
            Vector3::Reflect(a3, n3, o); Twin("Vector3.Reflect(ref)", Vector3::Reflect(a3, n3), o);
            Vector3::SmoothStep(a3, b3, 0.25f, o); Twin("Vector3.SmoothStep(ref)", Vector3::SmoothStep(a3, b3, 0.25f), o);
            Vector3::Subtract(a3, b3, o); Twin("Vector3.Subtract(ref)", Vector3::Subtract(a3, b3), o);
            Vector3::Transform(a3, m, o); Twin("Vector3.Transform(matrix, ref)", Vector3::Transform(a3, m), o);
            Vector3::Transform(a3, q, o); Twin("Vector3.Transform(quaternion, ref)", Vector3::Transform(a3, q), o);
            Vector3::TransformNormal(a3, m, o); Twin("Vector3.TransformNormal(ref)", Vector3::TransformNormal(a3, m), o);
        }
        {
            Vector4 o;
            float f = 0;
            Vector4::Add(a4, b4, o); Twin("Vector4.Add(ref)", Vector4::Add(a4, b4), o);
            Vector4::Barycentric(a4, b4, c4, 0.3f, 0.2f, o); Twin("Vector4.Barycentric(ref)", Vector4::Barycentric(a4, b4, c4, 0.3f, 0.2f), o);
            Vector4::CatmullRom(a4, b4, c4, d4, 0.4f, o); Twin("Vector4.CatmullRom(ref)", Vector4::CatmullRom(a4, b4, c4, d4, 0.4f), o);
            Vector4::Clamp(c4, a4, d4, o); Twin("Vector4.Clamp(ref)", Vector4::Clamp(c4, a4, d4), o);
            Vector4::Distance(a4, b4, f); Twin("Vector4.Distance(ref)", Vector4::Distance(a4, b4), f);
            Vector4::DistanceSquared(a4, b4, f); Twin("Vector4.DistanceSquared(ref)", Vector4::DistanceSquared(a4, b4), f);
            Vector4::Divide(a4, b4, o); Twin("Vector4.Divide(vector, ref)", Vector4::Divide(a4, b4), o);
            Vector4::Divide(a4, 3.0f, o); Twin("Vector4.Divide(scalar, ref)", Vector4::Divide(a4, 3.0f), o);
            Vector4::Dot(a4, b4, f); Twin("Vector4.Dot(ref)", Vector4::Dot(a4, b4), f);
            Vector4::Hermite(a4, b4, c4, d4, 0.6f, o); Twin("Vector4.Hermite(ref)", Vector4::Hermite(a4, b4, c4, d4, 0.6f), o);
            Vector4::Lerp(a4, b4, 0.35f, o); Twin("Vector4.Lerp(ref)", Vector4::Lerp(a4, b4, 0.35f), o);
            Vector4::Max(a4, b4, o); Twin("Vector4.Max(ref)", Vector4::Max(a4, b4), o);
            Vector4::Min(a4, b4, o); Twin("Vector4.Min(ref)", Vector4::Min(a4, b4), o);
            Vector4::Multiply(a4, b4, o); Twin("Vector4.Multiply(vector, ref)", Vector4::Multiply(a4, b4), o);
            Vector4::Multiply(a4, -1.5f, o); Twin("Vector4.Multiply(scalar, ref)", Vector4::Multiply(a4, -1.5f), o);
            Vector4::Negate(a4, o); Twin("Vector4.Negate(ref)", Vector4::Negate(a4), o);
            Vector4::Normalize(a4, o); Twin("Vector4.Normalize(ref)", Vector4::Normalize(a4), o);
            Vector4::SmoothStep(a4, b4, 0.25f, o); Twin("Vector4.SmoothStep(ref)", Vector4::SmoothStep(a4, b4, 0.25f), o);
            Vector4::Subtract(a4, b4, o); Twin("Vector4.Subtract(ref)", Vector4::Subtract(a4, b4), o);
            Vector4::Transform(a2, m, o); Twin("Vector4.Transform(Vector2, matrix, ref)", Vector4::Transform(a2, m), o);
            Vector4::Transform(a3, m, o); Twin("Vector4.Transform(Vector3, matrix, ref)", Vector4::Transform(a3, m), o);
            Vector4::Transform(a4, m, o); Twin("Vector4.Transform(Vector4, matrix, ref)", Vector4::Transform(a4, m), o);
            Vector4::Transform(a2, q, o); Twin("Vector4.Transform(Vector2, quaternion, ref)", Vector4::Transform(a2, q), o);
            Vector4::Transform(a3, q, o); Twin("Vector4.Transform(Vector3, quaternion, ref)", Vector4::Transform(a3, q), o);
            Vector4::Transform(a4, q, o); Twin("Vector4.Transform(Vector4, quaternion, ref)", Vector4::Transform(a4, q), o);
        }
    }

    void Matrices()
    {
        const Matrix a = Matrix::CreateFromYawPitchRoll(0.3f, -0.2f, 0.7f) * Matrix::CreateTranslation(1.0f, -2.0f, 3.0f);
        const Matrix b = Matrix::CreateScale(1.5f, 0.5f, 2.0f) * Matrix::CreateRotationZ(0.4f);
        const Vector3 p(1.0f, 2.0f, 3.0f), t(-2.0f, 0.5f, 4.0f), up = Vector3::Up, axis = Vector3::Normalize(Vector3(1, 1, 0));
        const Quaternion q = Quaternion::CreateFromYawPitchRoll(0.4f, 0.1f, -0.3f);
        const Plane plane(Vector3::Normalize(Vector3(0.2f, 1.0f, 0.1f)), -0.5f);
        Matrix o;
        Matrix::Add(a, b, o); Twin("Matrix.Add(ref)", Matrix::Add(a, b), o);
        Matrix::CreateBillboard(p, t, up, std::optional<Vector3>(Vector3::Forward), o); Twin("Matrix.CreateBillboard(ref)", Matrix::CreateBillboard(p, t, up, std::optional<Vector3>(Vector3::Forward)), o);
        Matrix::CreateConstrainedBillboard(p, t, up, std::optional<Vector3>(Vector3::Forward), std::optional<Vector3>(Vector3::Forward), o);
        Twin("Matrix.CreateConstrainedBillboard(ref)", Matrix::CreateConstrainedBillboard(p, t, up, std::optional<Vector3>(Vector3::Forward), std::optional<Vector3>(Vector3::Forward)), o);
        Matrix::CreateFromAxisAngle(axis, 0.8f, o); Twin("Matrix.CreateFromAxisAngle(ref)", Matrix::CreateFromAxisAngle(axis, 0.8f), o);
        Matrix::CreateFromQuaternion(q, o); Twin("Matrix.CreateFromQuaternion(ref)", Matrix::CreateFromQuaternion(q), o);
        Matrix::CreateFromYawPitchRoll(0.3f, -0.2f, 0.7f, o); Twin("Matrix.CreateFromYawPitchRoll(ref)", Matrix::CreateFromYawPitchRoll(0.3f, -0.2f, 0.7f), o);
        Matrix::CreateLookAt(p, t, up, o); Twin("Matrix.CreateLookAt(ref)", Matrix::CreateLookAt(p, t, up), o);
        Matrix::CreateOrthographic(8.0f, 6.0f, 0.1f, 50.0f, o); Twin("Matrix.CreateOrthographic(ref)", Matrix::CreateOrthographic(8.0f, 6.0f, 0.1f, 50.0f), o);
        Matrix::CreateOrthographicOffCenter(-3.0f, 5.0f, -2.0f, 4.0f, 0.1f, 50.0f, o); Twin("Matrix.CreateOrthographicOffCenter(ref)", Matrix::CreateOrthographicOffCenter(-3.0f, 5.0f, -2.0f, 4.0f, 0.1f, 50.0f), o);
        Matrix::CreatePerspective(1.6f, 0.9f, 0.5f, 80.0f, o); Twin("Matrix.CreatePerspective(ref)", Matrix::CreatePerspective(1.6f, 0.9f, 0.5f, 80.0f), o);
        Matrix::CreatePerspectiveFieldOfView(0.9f, 1.7f, 0.5f, 80.0f, o); Twin("Matrix.CreatePerspectiveFieldOfView(ref)", Matrix::CreatePerspectiveFieldOfView(0.9f, 1.7f, 0.5f, 80.0f), o);
        Matrix::CreatePerspectiveOffCenter(-1.0f, 2.0f, -0.5f, 1.0f, 0.5f, 80.0f, o); Twin("Matrix.CreatePerspectiveOffCenter(ref)", Matrix::CreatePerspectiveOffCenter(-1.0f, 2.0f, -0.5f, 1.0f, 0.5f, 80.0f), o);
        Matrix::CreateRotationX(0.6f, o); Twin("Matrix.CreateRotationX(ref)", Matrix::CreateRotationX(0.6f), o);
        Matrix::CreateRotationY(0.6f, o); Twin("Matrix.CreateRotationY(ref)", Matrix::CreateRotationY(0.6f), o);
        Matrix::CreateRotationZ(0.6f, o); Twin("Matrix.CreateRotationZ(ref)", Matrix::CreateRotationZ(0.6f), o);
        Matrix::CreateScale(1.75f, o); Twin("Matrix.CreateScale(uniform, ref)", Matrix::CreateScale(1.75f), o);
        Matrix::CreateScale(1.5f, 0.5f, 2.0f, o); Twin("Matrix.CreateScale(x, y, z, ref)", Matrix::CreateScale(1.5f, 0.5f, 2.0f), o);
        Matrix::CreateScale(p, o); Twin("Matrix.CreateScale(Vector3, ref)", Matrix::CreateScale(p), o);
        Matrix::CreateShadow(Vector3::Normalize(Vector3(-1, -2, -1)), plane, o); Twin("Matrix.CreateShadow(ref)", Matrix::CreateShadow(Vector3::Normalize(Vector3(-1, -2, -1)), plane), o);
        Matrix::CreateTranslation(p, o); Twin("Matrix.CreateTranslation(Vector3, ref)", Matrix::CreateTranslation(p), o);
        Matrix::CreateTranslation(1.0f, 2.0f, 3.0f, o); Twin("Matrix.CreateTranslation(x, y, z, ref)", Matrix::CreateTranslation(1.0f, 2.0f, 3.0f), o);
        Matrix::CreateReflection(plane, o); Twin("Matrix.CreateReflection(ref)", Matrix::CreateReflection(plane), o);
        Matrix::CreateWorld(p, Vector3::Forward, up, o); Twin("Matrix.CreateWorld(ref)", Matrix::CreateWorld(p, Vector3::Forward, up), o);
        Matrix::Divide(a, b, o); Twin("Matrix.Divide(matrix, ref)", Matrix::Divide(a, b), o);
        Matrix::Divide(a, 4.0f, o); Twin("Matrix.Divide(scalar, ref)", Matrix::Divide(a, 4.0f), o);
        Matrix::Invert(a, o); Twin("Matrix.Invert(ref)", Matrix::Invert(a), o);
        Matrix::Lerp(a, b, 0.3f, o); Twin("Matrix.Lerp(ref)", Matrix::Lerp(a, b, 0.3f), o);
        Matrix::Multiply(a, b, o); Twin("Matrix.Multiply(matrix, ref)", Matrix::Multiply(a, b), o);
        Matrix::Multiply(a, -2.5f, o); Twin("Matrix.Multiply(scalar, ref)", Matrix::Multiply(a, -2.5f), o);
        Matrix::Negate(a, o); Twin("Matrix.Negate(ref)", Matrix::Negate(a), o);
        Matrix::Subtract(a, b, o); Twin("Matrix.Subtract(ref)", Matrix::Subtract(a, b), o);
        Matrix::Transpose(a, o); Twin("Matrix.Transpose(ref)", Matrix::Transpose(a), o);
        Matrix::Transform(a, q, o); Twin("Matrix.Transform(quaternion, ref)", Matrix::Transform(a, q), o);
    }

    void Quaternions()
    {
        const Quaternion a = Quaternion::CreateFromYawPitchRoll(0.4f, 0.1f, -0.3f), b = Quaternion::CreateFromAxisAngle(Vector3::Normalize(Vector3(1, 2, 3)), 1.1f);
        const Matrix m = Matrix::CreateFromYawPitchRoll(0.3f, -0.2f, 0.7f);
        Quaternion o;
        float f = 0;
        Quaternion::Add(a, b, o); Twin("Quaternion.Add(ref)", Quaternion::Add(a, b), o);
        Quaternion::Concatenate(a, b, o); Twin("Quaternion.Concatenate(ref)", Quaternion::Concatenate(a, b), o);
        Quaternion::Conjugate(a, o); Twin("Quaternion.Conjugate(ref)", Quaternion::Conjugate(a), o);
        Quaternion::CreateFromAxisAngle(Vector3::UnitY, 0.9f, o); Twin("Quaternion.CreateFromAxisAngle(ref)", Quaternion::CreateFromAxisAngle(Vector3::UnitY, 0.9f), o);
        Quaternion::CreateFromRotationMatrix(m, o); Twin("Quaternion.CreateFromRotationMatrix(ref)", Quaternion::CreateFromRotationMatrix(m), o);
        Quaternion::CreateFromYawPitchRoll(0.4f, 0.1f, -0.3f, o); Twin("Quaternion.CreateFromYawPitchRoll(ref)", Quaternion::CreateFromYawPitchRoll(0.4f, 0.1f, -0.3f), o);
        Quaternion::Divide(a, b, o); Twin("Quaternion.Divide(ref)", Quaternion::Divide(a, b), o);
        Quaternion::Dot(a, b, f); Twin("Quaternion.Dot(ref)", Quaternion::Dot(a, b), f);
        Quaternion::Inverse(a, o); Twin("Quaternion.Inverse(ref)", Quaternion::Inverse(a), o);
        Quaternion::Lerp(a, b, 0.3f, o); Twin("Quaternion.Lerp(ref)", Quaternion::Lerp(a, b, 0.3f), o);
        Quaternion::Slerp(a, b, 0.3f, o); Twin("Quaternion.Slerp(ref)", Quaternion::Slerp(a, b, 0.3f), o);
        Quaternion::Subtract(a, b, o); Twin("Quaternion.Subtract(ref)", Quaternion::Subtract(a, b), o);
        Quaternion::Multiply(a, b, o); Twin("Quaternion.Multiply(quaternion, ref)", Quaternion::Multiply(a, b), o);
        Quaternion::Multiply(a, 1.75f, o); Twin("Quaternion.Multiply(scalar, ref)", Quaternion::Multiply(a, 1.75f), o);
        Quaternion::Negate(a, o); Twin("Quaternion.Negate(ref)", Quaternion::Negate(a), o);
        Quaternion::Normalize(Quaternion::Multiply(a, 3.0f), o); Twin("Quaternion.Normalize(ref)", Quaternion::Normalize(Quaternion::Multiply(a, 3.0f)), o);
    }

    void Volumes()
    {
        const BoundingBox box(Vector3(-1, -1, -1), Vector3(1, 2, 1)), other(Vector3(0.5f, 0.5f, 0.5f), Vector3(3, 3, 3));
        const BoundingSphere sphere(Vector3(0.5f, 0.0f, 0.0f), 1.25f), far(Vector3(10, 0, 0), 0.5f);
        const Vector3 point(0.25f, 1.5f, -0.5f);
        const Ray ray(Vector3(-5, 0.5f, 0.2f), Vector3::UnitX);
        const Plane plane(Vector3::UnitY, -0.5f);
        const BoundingFrustum frustum(Matrix::CreateLookAt(Vector3(0, 0, 6), Vector3::Zero, Vector3::Up) * Matrix::CreatePerspectiveFieldOfView(0.9f, 1.0f, 0.5f, 30.0f));
        const Matrix m = Matrix::CreateScale(2.0f) * Matrix::CreateTranslation(1, 2, 3);
        ContainmentType ct{};
        bool hit = false;
        PlaneIntersectionType pt{};
        std::optional<float> d;
        box.Contains(other, ct); Twin("BoundingBox.Contains(BoundingBox, ref)", box.Contains(other), ct);
        box.Contains(sphere, ct); Twin("BoundingBox.Contains(BoundingSphere, ref)", box.Contains(sphere), ct);
        box.Contains(point, ct); Twin("BoundingBox.Contains(Vector3, ref)", box.Contains(point), ct);
        box.Intersects(ray, d); Twin("BoundingBox.Intersects(Ray, ref)", box.Intersects(ray), d);
        box.Intersects(far, hit); Twin("BoundingBox.Intersects(BoundingSphere, ref)", box.Intersects(far), hit);
        box.Intersects(other, hit); Twin("BoundingBox.Intersects(BoundingBox, ref)", box.Intersects(other), hit);
        box.Intersects(plane, pt); Twin("BoundingBox.Intersects(Plane, ref)", box.Intersects(plane), pt);
        BoundingBox ob;
        BoundingBox::CreateFromSphere(sphere, ob); Twin("BoundingBox.CreateFromSphere(ref)", BoundingBox::CreateFromSphere(sphere), ob);
        BoundingBox::CreateMerged(box, other, ob); Twin("BoundingBox.CreateMerged(ref)", BoundingBox::CreateMerged(box, other), ob);
        frustum.Contains(box, ct); Twin("BoundingFrustum.Contains(BoundingBox, ref)", frustum.Contains(box), ct);
        frustum.Contains(sphere, ct); Twin("BoundingFrustum.Contains(BoundingSphere, ref)", frustum.Contains(sphere), ct);
        frustum.Contains(point, ct); Twin("BoundingFrustum.Contains(Vector3, ref)", frustum.Contains(point), ct);
        frustum.Intersects(box, hit); Twin("BoundingFrustum.Intersects(BoundingBox, ref)", frustum.Intersects(box), hit);
        frustum.Intersects(far, hit); Twin("BoundingFrustum.Intersects(BoundingSphere, ref)", frustum.Intersects(far), hit);
        frustum.Intersects(plane, pt); Twin("BoundingFrustum.Intersects(Plane, ref)", frustum.Intersects(plane), pt);
        const Ray inside(Vector3(0, 0, 0), Vector3::Forward);   // CNA answers only for a ray starting inside (see Edges)
        frustum.Intersects(inside, d); Twin("BoundingFrustum.Intersects(Ray from inside, ref)", frustum.Intersects(inside), d);
        BoundingSphere os;
        sphere.Transform(m, os); Twin("BoundingSphere.Transform(ref)", sphere.Transform(m), os);
        sphere.Contains(box, ct); Twin("BoundingSphere.Contains(BoundingBox, ref)", sphere.Contains(box), ct);
        sphere.Contains(far, ct); Twin("BoundingSphere.Contains(BoundingSphere, ref)", sphere.Contains(far), ct);
        sphere.Contains(point, ct); Twin("BoundingSphere.Contains(Vector3, ref)", sphere.Contains(point), ct);
        BoundingSphere::CreateFromBoundingBox(box, os); Twin("BoundingSphere.CreateFromBoundingBox(ref)", BoundingSphere::CreateFromBoundingBox(box), os);
        BoundingSphere::CreateMerged(sphere, far, os); Twin("BoundingSphere.CreateMerged(ref)", BoundingSphere::CreateMerged(sphere, far), os);
        sphere.Intersects(box, hit); Twin("BoundingSphere.Intersects(BoundingBox, ref)", sphere.Intersects(box), hit);
        sphere.Intersects(far, hit); Twin("BoundingSphere.Intersects(BoundingSphere, ref)", sphere.Intersects(far), hit);
        sphere.Intersects(ray, d); Twin("BoundingSphere.Intersects(Ray, ref)", sphere.Intersects(ray), d);
        sphere.Intersects(plane, pt); Twin("BoundingSphere.Intersects(Plane, ref)", sphere.Intersects(plane), pt);
        float f = 0;
        plane.Dot(Vector4(1, 2, 3, 1), f); Twin("Plane.Dot(ref)", plane.Dot(Vector4(1, 2, 3, 1)), f);
        plane.DotCoordinate(point, f); Twin("Plane.DotCoordinate(ref)", plane.DotCoordinate(point), f);
        plane.DotNormal(point, f); Twin("Plane.DotNormal(ref)", plane.DotNormal(point), f);
        plane.Intersects(box, pt); Twin("Plane.Intersects(BoundingBox, ref)", plane.Intersects(box), pt);
        plane.Intersects(sphere, pt); Twin("Plane.Intersects(BoundingSphere, ref)", plane.Intersects(sphere), pt);
        Plane op;
        const Plane loose(Vector3(0.0f, 2.0f, 0.0f), -1.0f);
        Plane::Normalize(loose, op); Twin("Plane.Normalize(ref)", Plane::Normalize(loose), op);
        Plane::Transform(plane, m, op); Twin("Plane.Transform(matrix, ref)", Plane::Transform(plane, m), op);
        Plane::Transform(plane, Quaternion::CreateFromYawPitchRoll(0.4f, 0.1f, -0.3f), op);
        Twin("Plane.Transform(quaternion, ref)", Plane::Transform(plane, Quaternion::CreateFromYawPitchRoll(0.4f, 0.1f, -0.3f)), op);
        ray.Intersects(box, d); Twin("Ray.Intersects(BoundingBox, ref)", ray.Intersects(box), d);
        ray.Intersects(sphere, d); Twin("Ray.Intersects(BoundingSphere, ref)", ray.Intersects(sphere), d);
        const Ray down(Vector3(0, 5, 0), -Vector3::UnitY);
        down.Intersects(plane, d); Twin("Ray.Intersects(Plane, ref)", down.Intersects(plane), d);
        const Rectangle r1(0, 0, 10, 8), r2(5, 4, 10, 10);
        r1.Contains(Point(3, 3), hit); Twin("Rectangle.Contains(Point, ref)", r1.Contains(Point(3, 3)), hit);
        r1.Contains(r2, hit); Twin("Rectangle.Contains(Rectangle, ref)", r1.Contains(r2), hit);
        r1.Intersects(r2, hit); Twin("Rectangle.Intersects(ref)", r1.Intersects(r2), hit);
        Rectangle orr;
        Rectangle::Intersect(r1, r2, orr); Twin("Rectangle.Intersect(ref)", Rectangle::Intersect(r1, r2), orr);
        Rectangle::Union(r1, r2, orr); Twin("Rectangle.Union(ref)", Rectangle::Union(r1, r2), orr);
    }

    // XNA 4.0's own answers at the edges (from its documented behaviour and its implementation, see docs/CNA_FINDINGS.md).
    void Edges()
    {
        // A ray from outside the frustum: XNA clips it against the six planes and returns the entry distance.
        const BoundingFrustum frustum(Matrix::CreateLookAt(Vector3(0, 0, 6), Vector3::Zero, Vector3::Up) * Matrix::CreatePerspectiveFieldOfView(0.9f, 1.0f, 0.5f, 30.0f));
        const Ray outside(Vector3(0, 0, 10), Vector3::Forward);   // starts behind the camera, travels into the frustum
        std::string got;
        bool pass = false;
        try {
            const std::optional<float> d = frustum.Intersects(outside);
            char buf[64];
            std::snprintf(buf, sizeof buf, "%s", d ? std::to_string(*d).c_str() : "null");
            got = buf;
            pass = d.has_value() && std::fabs(*d - 4.5f) < 1e-3f;   // the near plane is 0.5 in front of the camera at z = 6: 10 - 5.5 = 4.5
        } catch (const std::exception& e) {
            got = std::string("threw: ") + e.what();
        }
        Add("BoundingFrustum.Intersects(Ray from outside)", pass, "4.5 (the near plane), as XNA returns", got);
        const Ray away(Vector3(0, 0, 10), Vector3::Backward);
        try {
            const std::optional<float> d = frustum.Intersects(away);
            Add("BoundingFrustum.Intersects(Ray pointing away)", !d.has_value(), "null, as XNA returns", d ? std::to_string(*d) : "null");
        } catch (const std::exception& e) {
            Add("BoundingFrustum.Intersects(Ray pointing away)", false, "null, as XNA returns", std::string("threw: ") + e.what());
        }
        // XNA normalises by multiplying with 1/length, so a zero vector becomes NaN in every component.
        const Vector3 zero = Vector3::Normalize(Vector3::Zero);
        Add("Vector3.Normalize(Zero)", std::isnan(zero.X) && std::isnan(zero.Y) && std::isnan(zero.Z), "NaN components, as XNA", std::to_string(zero.X));
        Add("MathHelper.Clamp(5, 0, 1) and Lerp(2, 4, 0.25)", MathHelper::Clamp(5.0f, 0.0f, 1.0f) == 1.0f && MathHelper::Lerp(2.0f, 4.0f, 0.25f) == 2.5f, "1 and 2.5",
            std::to_string(MathHelper::Clamp(5.0f, 0.0f, 1.0f)) + " and " + std::to_string(MathHelper::Lerp(2.0f, 4.0f, 0.25f)));
        Add("MathHelper.WrapAngle(3 pi)", std::fabs(MathHelper::WrapAngle(3.0f * MathHelper::Pi) - MathHelper::Pi) < 1e-5f, "pi", std::to_string(MathHelper::WrapAngle(3.0f * MathHelper::Pi)));
        const Rectangle empty = Rectangle::Intersect(Rectangle(0, 0, 2, 2), Rectangle(5, 5, 2, 2));
        Add("Rectangle.Intersect of disjoint rectangles", empty.X == 0 && empty.Y == 0 && empty.Width == 0 && empty.Height == 0, "Rectangle.Empty, as XNA",
            std::to_string(empty.X) + "," + std::to_string(empty.Y) + " " + std::to_string(empty.Width) + "x" + std::to_string(empty.Height));
    }

    std::vector<MathCheck> checks_;
    int passed_ = 0, failed_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeMathChecksLab(const RoomDef& room, Services& services)
{
    return std::make_unique<MathChecksLab>(room, services);
}
