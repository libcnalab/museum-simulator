// SpriteFont laboratory: alignment and wrapping, measurement, transforms, the glyph set, dynamic HUD text
// and world labels projected to the screen. Uses a private SpriteFont so spacing changes never touch the HUD.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <string>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Rendering/Gizmos.hpp"
#include "Rendering/LitMesh.hpp"
#include "UI/FontLoader.hpp"
#include "System/Text/StringBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Align, Measure, Transform, Glyphs, Hud, Labels };

Mode ParseMode(const std::string& s)
{
    if (s == "measure") return Mode::Measure;
    if (s == "transform") return Mode::Transform;
    if (s == "glyphs") return Mode::Glyphs;
    if (s == "hud") return Mode::Hud;
    if (s == "labels") return Mode::Labels;
    return Mode::Align;
}

const char* kParagraph =
    "SpriteFont draws text from a glyph atlas. MeasureString reports how large a string will be, which is "
    "all that alignment, wrapping and layout need: the font itself has no notion of paragraphs.";

class TextLab final : public ScreenExhibit {
public:
    TextLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Align:
                p.AddChoice("Alignment", {"Left", "Centre", "Right", "Justified"}, 0, [this](int v) { align_ = v; });
                p.AddSlider("Box width", 220.0f, 900.0f, width_, [this](float v) { width_ = v; }, "%.0f px", 1.0f);
                p.AddSlider("Text scale", 0.5f, 1.4f, scale_, [this](float v) { scale_ = v; });
                p.AddToggle("Word wrap", true, [this](bool v) { wrap_ = v; });
                break;
            case Mode::Measure:
                p.AddSlider("Scale", 0.4f, 2.0f, scale_, [this](float v) { scale_ = v; });
                p.AddSlider("Spacing (extra pixels)", -3.0f, 12.0f, spacing_, [this](float v) { spacing_ = v; if (font_) font_->setSpacingProperty(v); }, "%.1f");
                p.AddSlider("LineSpacing", 14.0f, 60.0f, lineSpacing_, [this](float v) { lineSpacing_ = v; if (font_) font_->setLineSpacingProperty(static_cast<int>(v)); }, "%.0f", 1.0f);
                p.AddReadout("MeasureString(\"Hello, CNA\")", [this] { return Dim(font_ ? font_->MeasureString("Hello, CNA") * scale_ : Vector2()); });
                p.AddReadout("MeasureString(two lines)", [this] { return Dim(font_ ? font_->MeasureString("two\nlines") * scale_ : Vector2()); });
                break;
            case Mode::Transform:
                p.AddSlider("Rotation speed", -3.0f, 3.0f, speed_, [this](float v) { speed_ = v; });
                p.AddSlider("Scale X", 0.4f, 2.5f, scale_, [this](float v) { scale_ = v; });
                p.AddSlider("Scale Y", 0.4f, 2.5f, scaleY_, [this](float v) { scaleY_ = v; });
                p.AddToggle("Flip (SpriteEffects)", false, [this](bool v) { flip_ = v; });
                break;
            case Mode::Glyphs:
                p.AddToggle("DefaultCharacter = '?'", true, [this](bool v) { useDefault_ = v; if (font_) font_->setDefaultCharacterProperty(v ? std::optional<charcs>('?') : std::nullopt); });
                p.AddSlider("Inspect a glyph (index)", 0.0f, 94.0f, static_cast<float>(inspect_), [this](float v) { inspect_ = static_cast<int>(v); }, "%.0f", 1.0f);
                p.AddReadout("Glyph", [this] { return GlyphText(); });
                p.AddReadout("Glyphs in this font", [this] { return std::to_string(font_ ? font_->getCharactersProperty().size() : 0); });
                p.AddReadout("MeasureString(\"Příliš\")", [this] { return Attempt("Příliš"); });
                p.AddReadout("MeasureString(\"Ωmega\")", [this] { return Attempt("Ωmega"); });
                break;
            case Mode::Hud:
                p.AddSlider("Health", 0.0f, 100.0f, health_, [this](float v) { health_ = v; }, "%.0f", 1.0f);
                p.AddButton("Collect 250 points", [this] { score_ += 250; });
                p.AddToggle("Use StringBuilder overload", true, [this](bool v) { useBuilder_ = v; });
                break;
            case Mode::Labels:
                p.AddSlider("Label scale", 0.4f, 1.2f, scale_, [this](float v) { scale_ = v; });
                p.AddToggle("Edge pointers for off-screen targets", true, [this](bool v) { pointers_ = v; });
                p.AddHeading("Viewport.Project turns world points into pixels");
                break;
        }
        AddInfoSign(b, Title(), Explanation());
        if (mode_ == Mode::Labels) BuildTargets(b);
    }

    void OnScreenLoaded() override
    {
        font_ = FontLoader::Load(*S().content, S().contentRoot, "ui");
        if (font_) {
            font_->setSpacingProperty(spacing_);
            lineSpacing_ = static_cast<float>(font_->getLineSpacingProperty());
            if (mode_ == Mode::Glyphs) font_->setDefaultCharacterProperty(std::optional<charcs>('?'));
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        shownScore_ += (static_cast<float>(score_) - shownScore_) * std::min(1.0f, ctx.dt * 4.0f);
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (mode_ == Mode::Labels) {
            for (std::size_t i = 0; i < targets_.size(); ++i) {
                gizmos_.SolidSphere(ctx, R().ToWorld(targets_[i].x, 1.0f + 0.25f * std::sin(time_ + static_cast<float>(i)), targets_[i].z), 0.28f, targets_[i].colour);
                gizmos_.Label(R().ToWorld(targets_[i].x, 1.7f, targets_[i].z), targets_[i].name, Color(255, 240, 160, 255), scale_);
            }
            return;
        }
        ScreenExhibit::DrawOpaque(ctx);
    }
    void DrawOverlay() override
    {
        gizmos_.DrawLabels();
        if (mode_ != Mode::Labels || !pointers_ || S().mainView == nullptr) return;
        // Off-screen targets get an arrow on the screen edge pointing the way: Viewport.Project plus clamping.
        const Viewport vp = S().device->getViewportProperty();
        const SceneView& v = *S().mainView;
        const SamplerState sampler = SamplerState::LinearClamp;
        S().sprites->Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &sampler, nullptr, nullptr);
        for (const Target& t : targets_) {
            const Vector3 world = R().ToWorld(t.x, 1.0f, t.z);
            const Vector4 clip = Vector4::Transform(Vector4(world, 1.0f), v.view * v.projection);
            Vector3 p = vp.Project(world, v.projection, v.view, Matrix::getIdentityProperty());
            const float w = static_cast<float>(vp.getWidthProperty()), h = static_cast<float>(vp.getHeightProperty());
            const bool behind = clip.W <= 0.0f;
            if (behind) {
                p.X = w - p.X;
                p.Y = h - p.Y;
            }
            const bool onScreen = !behind && p.X > 0 && p.X < w && p.Y > 0 && p.Y < h;
            if (onScreen) continue;
            const Vector2 centre(w * 0.5f, h * 0.5f);
            Vector2 dir = Vector2(p.X, p.Y) - centre;
            if (dir.LengthSquared() < 1.0f) dir = Vector2(0, -1);
            dir.Normalize();
            const float tx = dir.X != 0 ? (w * 0.5f - 40.0f) / std::abs(dir.X) : 1e9f, ty = dir.Y != 0 ? (h * 0.5f - 40.0f) / std::abs(dir.Y) : 1e9f;
            const Vector2 edge = centre + dir * std::min(tx, ty);
            S().sprites->Draw(*S().whiteTexture, Rectangle(static_cast<int>(edge.X) - 9, static_cast<int>(edge.Y) - 9, 18, 18), Color(255, 220, 90, 255));
            S().sprites->DrawString(*S().font, t.name, edge + Vector2(-20.0f - dir.X * 20.0f, 10.0f), Color(255, 240, 160, 255), 0.0f, Vector2(0, 0), 0.55f, SpriteEffects::None, 0.0f);
        }
        S().sprites->End();
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (!font_) return;
        const SamplerState& smp = SamplerState::LinearClamp;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &smp, nullptr, nullptr);
        switch (mode_) {
            case Mode::Align: DrawAlign(sb); break;
            case Mode::Measure: DrawMeasure(sb); break;
            case Mode::Transform: DrawTransform(sb); break;
            case Mode::Glyphs: DrawGlyphs(sb); break;
            case Mode::Hud: DrawHud(sb); break;
            case Mode::Labels: break;
        }
        sb.End();
    }

private:
    struct Target {
        const char* name;
        float x, z;
        Vector3 colour;
    };
    void BuildTargets(RoomBuilder&)
    {
        targets_ = {{"Alpha", -4.0f, -3.0f, {0.9f, 0.3f, 0.3f}}, {"Bravo", 4.0f, -4.0f, {0.3f, 0.9f, 0.4f}}, {"Charlie", 4.5f, 3.5f, {0.3f, 0.5f, 1.0f}},
                    {"Delta", -3.5f, 4.0f, {0.95f, 0.8f, 0.3f}}, {"Echo", 0.0f, 0.0f, {0.8f, 0.4f, 0.95f}}};
    }
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Text alignment and wrapping", "Measuring text", "Rotating and scaling text", "Glyphs and the default character", "HUD text and StringBuilder", "World labels on the screen"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Align: return {"SpriteFont has no alignment: you measure each line with MeasureString and offset it yourself.", "Left, centre and right differ by the start x; justified spreads the spaces so every line fills the box."};
            case Mode::Measure: return {"MeasureString returns the size of a string at scale 1. Spacing adds pixels between characters and", "LineSpacing sets the line height; the boxes drawn here are exactly the measured rectangles."};
            case Mode::Transform: return {"DrawString takes rotation, origin, a uniform or per-axis scale, SpriteEffects and a colour. The", "origin is set to the text's centre so it spins in place. Per-character colours are one DrawString each."};
            case Mode::Glyphs: return {"A SpriteFont only knows the characters it was built with. Anything else throws, unless the", "DefaultCharacter is set, in which case it is substituted. All glyphs of this font are drawn below."};
            case Mode::Hud: return {"A HUD is text that changes every frame. The StringBuilder overload of DrawString lets a counter be", "edited in place instead of building a new std::string each frame."};
            default: return {"Labels over 3D objects: project the world position with Viewport.Project, skip anything behind the camera,", "and for targets outside the screen draw an edge pointer in the direction of the projected point."};
        }
    }
    static std::string Dim(const Vector2& v)
    {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.1f x %.1f px", static_cast<double>(v.X), static_cast<double>(v.Y));
        return buf;
    }
    [[nodiscard]] std::string Attempt(const std::string& s) const
    {
        if (!font_) return "-";
        try {
            return Dim(font_->MeasureString(s));
        } catch (const std::exception& e) {
            std::string m = e.what();
            return "throws: " + m.substr(0, 30);
        }
    }
    void Str(SpriteBatch& sb, const std::string& t, float x, float y, Color c, float scale = 1.0f)
    {
        sb.DrawString(*font_, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
    }

    std::vector<std::string> WrapLines(const std::string& text, float maxWidth) const
    {
        std::vector<std::string> lines;
        std::string line, word;
        const auto flush = [&] {
            if (!line.empty()) lines.push_back(line);
            line.clear();
        };
        for (std::size_t i = 0; i <= text.size(); ++i) {
            if (i == text.size() || text[i] == ' ') {
                if (word.empty()) continue;
                const std::string cand = line.empty() ? word : line + " " + word;
                if (!line.empty() && font_->MeasureString(cand).X * scale_ > maxWidth) {
                    flush();
                    line = word;
                } else {
                    line = cand;
                }
                word.clear();
            } else {
                word.push_back(text[i]);
            }
        }
        flush();
        return lines;
    }
    void DrawAlign(SpriteBatch& sb)
    {
        const float x0 = (960.0f - width_) * 0.5f;
        const auto frame = [&](float x, float y, float w, float h) {
            sb.Draw(*S().whiteTexture, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), 2), Color(255, 214, 90, 255));
            sb.Draw(*S().whiteTexture, Rectangle(static_cast<int>(x), static_cast<int>(y + h), static_cast<int>(w), 2), Color(255, 214, 90, 255));
            sb.Draw(*S().whiteTexture, Rectangle(static_cast<int>(x), static_cast<int>(y), 2, static_cast<int>(h)), Color(255, 214, 90, 255));
            sb.Draw(*S().whiteTexture, Rectangle(static_cast<int>(x + w), static_cast<int>(y), 2, static_cast<int>(h) + 2), Color(255, 214, 90, 255));
        };
        const std::vector<std::string> lines = wrap_ ? WrapLines(kParagraph, width_) : std::vector<std::string>{kParagraph};
        const float lh = static_cast<float>(font_->getLineSpacingProperty()) * scale_;
        frame(x0, 60, width_, lh * static_cast<float>(lines.size()) + 20.0f);
        float y = 70.0f;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const float w = font_->MeasureString(lines[i]).X * scale_;
            float x = x0 + 8.0f;
            if (align_ == 1) x = x0 + (width_ - w) * 0.5f;
            else if (align_ == 2) x = x0 + width_ - w - 8.0f;
            if (align_ == 3 && i + 1 < lines.size()) {
                // Justified: spread the spaces so the line fills the box.
                std::vector<std::string> words;
                std::string cur;
                for (char c : lines[i]) {
                    if (c == ' ') { words.push_back(cur); cur.clear(); }
                    else cur.push_back(c);
                }
                words.push_back(cur);
                float total = 0;
                for (const auto& wd : words) total += font_->MeasureString(wd).X * scale_;
                const float gap = words.size() > 1 ? (width_ - 16.0f - total) / static_cast<float>(words.size() - 1) : 0.0f;
                float wx = x0 + 8.0f;
                for (const auto& wd : words) {
                    sb.DrawString(*font_, wd, Vector2(wx, y), Color::White, 0.0f, Vector2(0, 0), scale_, SpriteEffects::None, 0.0f);
                    wx += font_->MeasureString(wd).X * scale_ + gap;
                }
            } else {
                sb.DrawString(*font_, lines[i], Vector2(x, y), Color::White, 0.0f, Vector2(0, 0), scale_, SpriteEffects::None, 0.0f);
            }
            y += lh;
        }
        Str(sb, "each line placed with MeasureString: x = box + (width - lineWidth) * factor", 20, 490, Color(150, 255, 170, 255), 0.65f);
    }

    void DrawMeasure(SpriteBatch& sb)
    {
        const char* items[3] = {"Hello, CNA", "two\nlines", "MeasureString"};
        float y = 40.0f;
        for (const char* t : items) {
            const Vector2 size = font_->MeasureString(t) * scale_;
            const float x = 60.0f;
            sb.Draw(*S().whiteTexture, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(size.X), static_cast<int>(size.Y)), Color(60, 90, 150, 255));
            sb.DrawString(*font_, t, Vector2(x, y), Color::White, 0.0f, Vector2(0, 0), scale_, SpriteEffects::None, 0.0f);
            Str(sb, Dim(size), x + size.X + 20.0f, y, Color(255, 226, 150, 255), 0.6f);
            y += size.Y + 24.0f;
        }
    }

    void DrawTransform(SpriteBatch& sb)
    {
        const std::string text = "Rotate me";
        const Vector2 size = font_->MeasureString(text);
        const SpriteEffects fx = flip_ ? SpriteEffects::FlipHorizontally : SpriteEffects::None;
        sb.DrawString(*font_, text, Vector2(240, 150), Color(255, 200, 90, 255), time_ * speed_, size * 0.5f, Vector2(scale_, scaleY_), fx, 0.0f);
        sb.DrawString(*font_, "shadow + outline", Vector2(560, 105), Color(0, 0, 0, 255), 0.0f, Vector2(0, 0), 1.2f, SpriteEffects::None, 0.0f);
        for (const Vector2 o : {Vector2(-2, 0), Vector2(2, 0), Vector2(0, -2), Vector2(0, 2)})
            sb.DrawString(*font_, "shadow + outline", Vector2(558, 103) + o, Color(20, 20, 40, 255), 0.0f, Vector2(0, 0), 1.2f, SpriteEffects::None, 0.0f);
        sb.DrawString(*font_, "shadow + outline", Vector2(558, 103), Color(255, 255, 255, 255), 0.0f, Vector2(0, 0), 1.2f, SpriteEffects::None, 0.0f);
        // Per-character colour: one DrawString per glyph, advancing by the measured width of the prefix.
        const std::string rainbow = "per-character colour";
        for (std::size_t i = 0; i < rainbow.size(); ++i) {
            const float x = 60.0f + font_->MeasureString(rainbow.substr(0, i)).X * 1.3f;
            const float hue = static_cast<float>(i) / static_cast<float>(rainbow.size()) * 6.0f;
            const Color c(static_cast<std::uint8_t>(127 + 127 * std::sin(hue)), static_cast<std::uint8_t>(127 + 127 * std::sin(hue + 2.1f)), static_cast<std::uint8_t>(127 + 127 * std::sin(hue + 4.2f)), static_cast<std::uint8_t>(255));
            sb.DrawString(*font_, std::string(1, rainbow[i]), Vector2(x, 330.0f + std::sin(time_ * 3.0f + static_cast<float>(i)) * 8.0f), c, 0.0f, Vector2(0, 0), 1.3f, SpriteEffects::None, 0.0f);
        }
        // Text on a circle.
        const std::string ring = "text on a circle - text on a circle - ";
        for (std::size_t i = 0; i < ring.size(); ++i) {
            const float a = time_ * 0.5f + static_cast<float>(i) / static_cast<float>(ring.size()) * MathHelper::TwoPi;
            sb.DrawString(*font_, std::string(1, ring[i]), Vector2(800.0f + std::cos(a) * 110.0f, 410.0f + std::sin(a) * 110.0f), Color(150, 220, 255, 255), a + MathHelper::PiOver2,
                          Vector2(5, 10), 0.8f, SpriteEffects::None, 0.0f);
        }
    }

    void DrawGlyphs(SpriteBatch& sb)
    {
        const auto& chars = font_->getCharactersProperty();
        int col = 0, row = 0;
        for (const charcs c : chars) {
            const std::string s(1, static_cast<char>(c < 128 ? c : '#'));
            if (c < 128) sb.DrawString(*font_, s, Vector2(24.0f + static_cast<float>(col) * 30.0f, 20.0f + static_cast<float>(row) * 34.0f), Color::White, 0.0f, Vector2(0, 0), 0.9f, SpriteEffects::None, 0.0f);
            if (++col >= 30) {
                col = 0;
                ++row;
            }
        }
        DrawInspector(sb);
        Str(sb, useDefault_ ? "DefaultCharacter = '?': unknown glyphs are substituted" : "No DefaultCharacter: unknown glyphs throw", 24, 330, Color(255, 226, 150, 255), 0.8f);
        if (useDefault_) {
            try {
                Str(sb, "Omega: Ω   Czech: Příliš žluťoučký", 24, 380, Color::White, 0.9f);
            } catch (const std::exception& e) {
                // With DefaultCharacter set, unknown glyphs are substituted: a throw here is a defect, not the demonstration.
                static bool reported = false;   // per-frame path: report the first failure only
                if (!reported) { reported = true; ReportError(std::string("DrawString with DefaultCharacter: ") + e.what()); }
            }
        } else {
            Str(sb, "(the line is not drawn: drawing it would throw)", 24, 380, Color(255, 150, 140, 255), 0.8f);
        }
    }

    // The glyph as the font stores it: a rectangle of the atlas texture, the cropping that was trimmed away and the kerning triple (left bearing, width, right bearing).
    [[nodiscard]] std::string GlyphText() const
    {
        if (!font_) return "-";
        const auto& chars = font_->getCharactersProperty();
        if (chars.empty()) return "-";
        const std::size_t i = std::min<std::size_t>(static_cast<std::size_t>(inspect_), chars.size() - 1);
        const Rectangle& g = font_->getGlyphBoundsEXT()[i];
        const Rectangle& c = font_->getCroppingEXT()[i];
        const Vector3& k = font_->getKerningEXT()[i];
        char buf[200];
        std::snprintf(buf, sizeof buf, "'%c'  bounds %d,%d %dx%d  crop %d,%d  kerning %.1f / %.1f / %.1f", chars[i] < 128 ? static_cast<char>(chars[i]) : '?', g.X, g.Y, g.Width, g.Height, c.X, c.Y, static_cast<double>(k.X), static_cast<double>(k.Y), static_cast<double>(k.Z));
        return buf;
    }
    void DrawInspector(SpriteBatch& sb)
    {
        if (!font_ || font_->getCharactersProperty().empty()) return;
        const std::size_t i = std::min<std::size_t>(static_cast<std::size_t>(inspect_), font_->getCharactersProperty().size() - 1);
        const Rectangle g = font_->getGlyphBoundsEXT()[i];
        const Rectangle c = font_->getCroppingEXT()[i];
        const int scale = std::max(1, std::min(220 / std::max(1, g.Width + 8), 220 / std::max(1, g.Height + 8)));
        sb.Draw(*S().whiteTexture, Rectangle(660, 170, 240, 150), Color(30, 36, 56, 255));
        // The cell the glyph occupies once uncropped, then the stored pixels inside it.
        sb.Draw(*S().whiteTexture, Rectangle(680, 180, (c.Width > 0 ? c.Width : g.Width) * scale, (c.Height > 0 ? c.Height : g.Height) * scale), Color(60, 70, 110, 255));
        sb.Draw(font_->getTextureEXT(), Rectangle(680 + (c.X) * scale, 180 + (c.Y) * scale, g.Width * scale, g.Height * scale), g, Color::White);
        Str(sb, GlyphText(), 24, 300, Color(170, 180, 210, 255), 0.55f);
    }

    void DrawHud(SpriteBatch& sb)
    {
        // A health bar, score counter, clock and frame counter: everything a typical HUD shows.
        sb.Draw(*S().whiteTexture, Rectangle(40, 40, 360, 34), Color(0, 0, 0, 160));
        sb.Draw(*S().whiteTexture, Rectangle(44, 44, static_cast<int>(352.0f * health_ / 100.0f), 26), health_ > 30.0f ? Color(80, 220, 110, 255) : Color(240, 80, 70, 255));
        Str(sb, "HEALTH " + std::to_string(static_cast<int>(health_)), 50, 46, Color::White, 0.75f);
        char buf[64];
        std::snprintf(buf, sizeof buf, "SCORE %07d", static_cast<int>(shownScore_ + 0.5f));
        if (useBuilder_) {
            System::Text::StringBuilder sbld;
            sbld.Append(std::string(buf));
            sb.DrawString(*font_, sbld, Vector2(40, 100), Color(255, 226, 150, 255));
        } else {
            Str(sb, buf, 40, 100, Color(255, 226, 150, 255), 1.0f);
        }
        const int total = static_cast<int>(time_ * 1000.0f);
        std::snprintf(buf, sizeof buf, "%02d:%02d.%03d", total / 60000, (total / 1000) % 60, total % 1000);
        Str(sb, buf, 700, 40, Color::White, 1.4f);
        std::snprintf(buf, sizeof buf, "frame %lld   %.0f fps", S().stats->frame, static_cast<double>(S().stats->fps));
        Str(sb, buf, 40, 480, Color(150, 255, 170, 255), 0.7f);
        Str(sb, useBuilder_ ? "score drawn with DrawString(StringBuilder)" : "score drawn with DrawString(std::string)", 40, 150, Color(170, 180, 210, 255), 0.6f);
    }

    Mode mode_;
    Gizmos gizmos_;
    std::unique_ptr<SpriteFont> font_;
    std::vector<Target> targets_;
    float time_ = 0.0f;
    float width_ = 620.0f, scale_ = 0.9f, scaleY_ = 1.2f, speed_ = 0.8f, spacing_ = 0.0f, lineSpacing_ = 27.0f, health_ = 72.0f, shownScore_ = 0.0f;
    int align_ = 0, score_ = 1250, inspect_ = 33;
    bool wrap_ = true, flip_ = false, useDefault_ = true, useBuilder_ = true, pointers_ = true;
};

}  // namespace

std::unique_ptr<Exhibit> MakeTextLab(const RoomDef& room, Services& services)
{
    return std::make_unique<TextLab>(room, services);
}
