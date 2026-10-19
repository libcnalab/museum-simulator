// The small value types of the framework, each used for real: the packed vector formats (what a vertex or texel
// really stores, and what is lost), Rectangle and Point (intersection, union, containment) and Color (lerp,
// premultiplication, packed value).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Alpha8.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Bgr565.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Bgra4444.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Bgra5551.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Byte4.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/HalfVector2.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/HalfVector4.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/NormalizedByte2.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/NormalizedShort2.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Short2.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/NormalizedByte4.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/NormalizedShort4.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Rg32.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Rgba1010102.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Rgba64.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Short4.hpp"
#include "Microsoft/Xna/Framework/Point.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::Graphics::PackedVector;

namespace {

enum class Mode { Packed, Rect, Color };

struct Packed {
    std::string hex;
    Vector4 back;
    int bits = 0;
};

template <typename T>
Packed Pack(const Vector4& v)
{
    T p = [&] {
        if constexpr (std::is_constructible_v<T, Vector4>) return T(v);
        else if constexpr (std::is_constructible_v<T, Vector3>) return T(Vector3(v.X, v.Y, v.Z));
        else return T(Vector2(v.X, v.Y));
    }();
    Packed out;
    const auto value = p.getPackedValueProperty();
    out.bits = static_cast<int>(sizeof(value) * 8);
    char buf[32];
    std::snprintf(buf, sizeof buf, "0x%0*llX", static_cast<int>(sizeof(value) * 2), static_cast<unsigned long long>(value));
    out.hex = buf;
    out.back = p.ToVector4();
    // Generic code sees every format only through the XNA interfaces: write the packed bits into a fresh value through
    // IPackedVectorT<T>.PackedValue and read it back through IPackedVector.ToVector4. Both must agree with the direct calls.
    T copy = p;
    IPackedVectorT<std::remove_cv_t<decltype(value)>>& bits = copy;
    bits.setPackedValueProperty(value);
    const IPackedVector& any = copy;
    if (bits.getPackedValueProperty() != value || any.ToVector4() != out.back) out.hex += " (interface disagrees)";
    return out;
}

struct Format {
    const char* name;
    float lo, hi;           // the range of values the format represents
    Packed (*pack)(const Vector4&);
};

const Format kFormats[] = {
    {"Byte4 (0..255)", 0.0f, 255.0f, &Pack<Byte4>},
    {"NormalizedByte4", -1.0f, 1.0f, &Pack<NormalizedByte4>},
    {"NormalizedShort4", -1.0f, 1.0f, &Pack<NormalizedShort4>},
    {"Short4", -32768.0f, 32767.0f, &Pack<Short4>},
    {"HalfVector4", -4.0f, 4.0f, &Pack<HalfVector4>},
    {"Rgba1010102", 0.0f, 1.0f, &Pack<Rgba1010102>},
    {"Rgba64", 0.0f, 1.0f, &Pack<Rgba64>},
    {"Bgra4444", 0.0f, 1.0f, &Pack<Bgra4444>},
    {"Bgra5551", 0.0f, 1.0f, &Pack<Bgra5551>},
    {"Bgr565 (no alpha)", 0.0f, 1.0f, &Pack<Bgr565>},
    {"Rg32 (two channels)", 0.0f, 1.0f, &Pack<Rg32>},
    {"Short2 (two channels)", -32768.0f, 32767.0f, &Pack<Short2>},
    {"NormalizedShort2", -1.0f, 1.0f, &Pack<NormalizedShort2>},
    {"NormalizedByte2", -1.0f, 1.0f, &Pack<NormalizedByte2>},
    {"HalfVector2", -4.0f, 4.0f, &Pack<HalfVector2>},
};

class StructLab final : public ScreenExhibit {
public:
    StructLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f)
    {
        const std::string m = room.params.Get("mode");
        mode_ = m == "rect" ? Mode::Rect : (m == "color" ? Mode::Color : Mode::Packed);
    }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        if (mode_ == Mode::Packed) {
            std::vector<std::string> names;
            for (const Format& f : kFormats) names.emplace_back(f.name);
            p.AddChoice("Format", names, 5, [this](int v) { format_ = v; });
            p.AddSlider("X (red)", 0.0f, 1.0f, value_[0], [this](float v) { value_[0] = v; });
            p.AddSlider("Y (green)", 0.0f, 1.0f, value_[1], [this](float v) { value_[1] = v; });
            p.AddSlider("Z (blue)", 0.0f, 1.0f, value_[2], [this](float v) { value_[2] = v; });
            p.AddSlider("W (alpha)", 0.0f, 1.0f, value_[3], [this](float v) { value_[3] = v; });
            p.AddReadout("PackedValue", [this] { return Current().hex; });
            p.AddReadout("Bits", [this] { return std::to_string(Current().bits); });
        } else if (mode_ == Mode::Rect) {
            p.AddSlider("A width", 40.0f, 320.0f, a_[2], [this](float v) { a_[2] = v; }, "%.0f", 1.0f);
            p.AddSlider("A height", 40.0f, 240.0f, a_[3], [this](float v) { a_[3] = v; }, "%.0f", 1.0f);
            p.AddSlider("B x", 0.0f, 600.0f, b_[0], [this](float v) { b_[0] = v; moving_ = false; }, "%.0f", 1.0f);
            p.AddSlider("B y", 0.0f, 400.0f, b_[1], [this](float v) { b_[1] = v; moving_ = false; }, "%.0f", 1.0f);
            p.AddToggle("Move B around A", true, [this](bool v) { moving_ = v; });
            p.AddSlider("Inflate A by", -30.0f, 60.0f, inflate_, [this](float v) { inflate_ = v; }, "%.0f", 1.0f);
        } else {
            p.AddSlider("Colour A hue", 0.0f, 1.0f, hueA_, [this](float v) { hueA_ = v; });
            p.AddSlider("Colour B hue", 0.0f, 1.0f, hueB_, [this](float v) { hueB_ = v; });
            p.AddSlider("Lerp amount", 0.0f, 1.0f, amount_, [this](float v) { amount_ = v; });
            p.AddSlider("Alpha", 0.0f, 1.0f, alpha_, [this](float v) { alpha_ = v; });
        }
        AddInfoSign(b, Title(), Explanation());
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        Texture2D& white = *S().whiteTexture;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::PointClamp, nullptr, nullptr);
        const auto rect = [&](int x, int y, int w, int h, Color c) { sb.Draw(white, Rectangle(x, y, std::max(0, w), std::max(0, h)), c); };
        const auto frame = [&](Rectangle r, Color c) { rect(r.X, r.Y, r.Width, 3, c); rect(r.X, r.Y + r.Height - 3, r.Width, 3, c); rect(r.X, r.Y, 3, r.Height, c); rect(r.X + r.Width - 3, r.Y, 3, r.Height, c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(Title(), 24, 10, Kit::kGold, 1.0f);
        if (mode_ == Mode::Packed) DrawPacked(rect, text);
        else if (mode_ == Mode::Rect) DrawRect(rect, frame, text);
        else DrawColor(rect, text);
        sb.End();
    }

    void OnScreenLoaded() override {}

public:
    void Update(const UpdateContext& ctx) override
    {
        if (ctx.nearby) time_ += ctx.dt;
        if (mode_ == Mode::Rect && moving_) {
            b_[0] = 280.0f + std::cos(time_ * 0.8f) * 230.0f;
            b_[1] = 200.0f + std::sin(time_ * 1.1f) * 140.0f;
        }
    }

private:
    [[nodiscard]] std::string Title() const
    {
        return mode_ == Mode::Packed ? "Packed vector formats" : (mode_ == Mode::Rect ? "Rectangle and Point" : "Color");
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        if (mode_ == Mode::Packed) return {"Vertices and textures store colours and normals in compact formats. Pack a Vector4 into each, read it back with", "ToVector4(): the hex PackedValue is what the GPU sees, and the difference is what the format cannot hold."};
        if (mode_ == Mode::Rect) return {"Rectangle.Intersects, Intersect, Union, Contains and Inflate on two rectangles and a point, drawn as they are", "computed. Rectangles use integer pixels; the first quality of 2D collision is exactly this."};
        return {"Color is four bytes. Color.Lerp mixes two, Multiply scales, FromNonPremultiplied converts for blending, and the", "PackedValue is the 32-bit word a vertex or texel stores. The colours below are computed with these calls."};
    }

    [[nodiscard]] Packed Current() const
    {
        const Format& f = kFormats[format_];
        const auto scaled = [&](float t) { return f.lo + (f.hi - f.lo) * t; };
        return f.pack(Vector4(scaled(value_[0]), scaled(value_[1]), scaled(value_[2]), scaled(value_[3])));
    }

    template <typename Rect, typename Text>
    void DrawPacked(Rect& rect, Text& text)
    {
        const Format& f = kFormats[format_];
        const Packed p = Current();
        const auto scaled = [&](float t) { return f.lo + (f.hi - f.lo) * t; };
        const float in[4] = {scaled(value_[0]), scaled(value_[1]), scaled(value_[2]), scaled(value_[3])};
        const float out[4] = {p.back.X, p.back.Y, p.back.Z, p.back.W};
        text(std::string(f.name) + "   " + std::to_string(p.bits) + " bits   PackedValue " + p.hex, 24, 56, Kit::kText, 0.8f);
        static const char* names[4] = {"X", "Y", "Z", "W"};
        for (int i = 0; i < 4; ++i) {
            const float y = 120.0f + static_cast<float>(i) * 90.0f;
            const float span = f.hi - f.lo;
            const auto bar = [&](float v, int row, Color c) { rect(120, static_cast<int>(y) + row * 28, static_cast<int>(std::clamp((v - f.lo) / span, 0.0f, 1.0f) * 600.0f), 22, c); };
            text(names[i], 40, y, Kit::kText, 0.8f);
            bar(in[i], 0, Kit::Rgba(110, 190, 255));
            bar(out[i], 1, Kit::Rgba(255, 190, 90));
            char buf[96];
            std::snprintf(buf, sizeof buf, "in %.4f   out %.4f   error %.5f", static_cast<double>(in[i]), static_cast<double>(out[i]), static_cast<double>(std::fabs(in[i] - out[i])));
            text(buf, 130, y + 56, Kit::kDim, 0.55f);
        }
        text("blue: the value you put in    orange: what the format returns", 24, 500, Kit::kDim, 0.55f);
    }

    template <typename Rect, typename Frame, typename Text>
    void DrawRect(Rect& rect, Frame& frame, Text& text)
    {
        Rectangle A(160, 120, static_cast<int>(a_[2]), static_cast<int>(a_[3]));
        const Rectangle B(static_cast<int>(b_[0]), static_cast<int>(b_[1]), 140, 110);
        Rectangle Ai = A;
        Ai.Inflate(static_cast<int>(inflate_), static_cast<int>(inflate_));
        const bool hit = Ai.Intersects(B);
        const Rectangle isect = Rectangle::Intersect(Ai, B), uni = Rectangle::Union(Ai, B);
        rect(uni.X, uni.Y, uni.Width, uni.Height, Color(255, 255, 255, 14));
        frame(uni, Kit::Rgba(120, 130, 160));
        rect(Ai.X, Ai.Y, Ai.Width, Ai.Height, Color(90, 140, 255, 90));
        rect(B.X, B.Y, B.Width, B.Height, Color(255, 170, 80, 90));
        frame(Ai, Kit::Rgba(110, 170, 255));
        frame(B, Kit::Rgba(255, 190, 90));
        if (hit && isect.Width > 0) rect(isect.X, isect.Y, isect.Width, isect.Height, Color(120, 255, 160, 170));
        const Point centre = Ai.getCenterProperty();
        rect(centre.X - 4, centre.Y - 4, 8, 8, Color::White);
        const bool pointInside = Ai.Contains(Point(static_cast<int>(b_[0]) + 70, static_cast<int>(b_[1]) + 55));
        rect(static_cast<int>(b_[0]) + 66, static_cast<int>(b_[1]) + 51, 8, 8, pointInside ? Color(120, 255, 160, 255) : Color(255, 120, 110, 255));
        text("A (blue, inflated by " + std::to_string(static_cast<int>(inflate_)) + ") and B (orange, 140 x 110)", 24, 56, Kit::kText, 0.7f);
        text(std::string("A.Intersects(B): ") + (hit ? "true" : "false") + "     Intersect: " + std::to_string(isect.Width) + " x " + std::to_string(isect.Height) + "     Union: " + std::to_string(uni.Width) + " x " + std::to_string(uni.Height), 24, 470, Kit::kText, 0.62f);
        text(std::string("A.Contains(centre of B): ") + (pointInside ? "true" : "false") + "     A centre: " + std::to_string(centre.X) + ", " + std::to_string(centre.Y) + "     A.Left/Right " + std::to_string(Ai.getLeftProperty()) + "/" + std::to_string(Ai.getRightProperty()) + "  Bottom " + std::to_string(Ai.getBottomProperty()) + "  Location " + std::to_string(Ai.getLocationProperty().X) + "," + std::to_string(Ai.getLocationProperty().Y) + (Ai.getIsEmptyProperty() ? "  EMPTY" : ""), 24, 498, Kit::kDim, 0.55f);
    }

    static Color Hue(float h, float alpha)
    {
        const auto ch = [&](float o) { return static_cast<int>((0.5f + 0.5f * std::cos((h - o) * MathHelper::TwoPi)) * 255.0f); };
        return Color(ch(0.0f), ch(0.3333f), ch(0.6667f), static_cast<int>(alpha * 255.0f));
    }

    template <typename Rect, typename Text>
    void DrawColor(Rect& rect, Text& text)
    {
        const Color a = Hue(hueA_, alpha_), b = Hue(hueB_, alpha_);
        const Color mix = Color::Lerp(a, b, amount_);
        const Color scaled = Color::Multiply(a, 0.5f);
        const Color nonPre = Color::FromNonPremultiplied(Vector4(a.ToVector4().X, a.ToVector4().Y, a.ToVector4().Z, alpha_));
        const struct { const char* name; Color c; } items[] = {{"A", a}, {"B", b}, {"Lerp(A, B)", mix}, {"Multiply(A, 0.5)", scaled}, {"FromNonPremultiplied", nonPre}};
        for (int i = 0; i < 5; ++i) {
            const int x = 40 + i * 180;
            rect(x, 120, 160, 160, Kit::Rgba(60, 64, 80));   // a backdrop so the alpha can be seen
            rect(x, 120, 80, 80, Kit::Rgba(110, 114, 130));
            rect(x + 80, 200, 80, 80, Kit::Rgba(110, 114, 130));
            rect(x, 120, 160, 160, items[i].c);
            text(items[i].name, static_cast<float>(x), 290, Kit::kText, 0.55f);
            char buf[64];
            std::snprintf(buf, sizeof buf, "R%d G%d B%d A%d", items[i].c.getRProperty(), items[i].c.getGProperty(), items[i].c.getBProperty(), items[i].c.getAProperty());
            text(buf, static_cast<float>(x), 318, Kit::kDim, 0.5f);
            std::snprintf(buf, sizeof buf, "0x%08X", items[i].c.getPackedValueProperty());
            text(buf, static_cast<float>(x), 342, Kit::kDim, 0.5f);
        }
        text("PackedValue is the 32-bit word (alpha in the top byte); Color stores premultiplied alpha as XNA does.", 40, 400, Kit::kDim, 0.55f);
        Color repacked;
        repacked.PackFromVector4(mix.ToVector4());
        const Vector3 rgb = mix.ToVector3();
        char line[160];
        std::snprintf(line, sizeof line, "Lerp(A, B).ToVector3() = (%.2f, %.2f, %.2f);  PackFromVector4(ToVector4()) %s the original", static_cast<double>(rgb.X), static_cast<double>(rgb.Y), static_cast<double>(rgb.Z),
                      repacked.getPackedValueProperty() == mix.getPackedValueProperty() ? "reproduces" : "DIFFERS from");
        text(line, 40, 430, Kit::kDim, 0.55f);
    }

    Mode mode_ = Mode::Packed;
    int format_ = 5;
    float value_[4] = {0.8f, 0.35f, 0.6f, 1.0f};
    float a_[4] = {0, 0, 200, 150}, b_[2] = {300, 200}, inflate_ = 0.0f, time_ = 0.0f;
    bool moving_ = true;
    float hueA_ = 0.05f, hueB_ = 0.6f, amount_ = 0.5f, alpha_ = 0.85f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeStructLab(const RoomDef& room, Services& services)
{
    return std::make_unique<StructLab>(room, services);
}
