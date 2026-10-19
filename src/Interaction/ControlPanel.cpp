#include "Interaction/ControlPanel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {
constexpr int kHeader = 62;
constexpr int kRowHeight = 66;
constexpr int kPad = 16;

Vector3 Normal(float yaw) { return Vector3(std::sin(yaw), 0.0f, std::cos(yaw)); }
Vector3 Right(float yaw) { return Vector3::Normalize(Vector3::Cross(-Normal(yaw), Vector3::Up)); }
}  // namespace

ControlPanel::ControlPanel(Services& services, const RoomDef& room, std::string title, const Pose& pose)
    : s_(services), room_(room), title_(std::move(title)), pose_(pose)
{
}

int ControlPanel::AddSlider(const std::string& label, float min, float max, float value, std::function<void(float)> onChange,
                            const char* format, float step)
{
    Row r;
    r.kind = Kind::Slider;
    r.label = label;
    r.min = min;
    r.max = max;
    r.value = std::clamp(value, min, max);
    r.step = step;
    r.format = format;
    r.onValue = std::move(onChange);
    rows_.push_back(std::move(r));
    return static_cast<int>(rows_.size()) - 1;
}

int ControlPanel::AddToggle(const std::string& label, bool value, std::function<void(bool)> onChange)
{
    Row r;
    r.kind = Kind::Toggle;
    r.label = label;
    r.value = value ? 1.0f : 0.0f;
    r.onValue = [cb = std::move(onChange)](float v) { if (cb) cb(v > 0.5f); };
    rows_.push_back(std::move(r));
    return static_cast<int>(rows_.size()) - 1;
}

int ControlPanel::AddChoice(const std::string& label, std::vector<std::string> options, int index, std::function<void(int)> onChange)
{
    Row r;
    r.kind = Kind::Choice;
    r.label = label;
    r.options = std::move(options);
    r.value = static_cast<float>(std::clamp(index, 0, std::max(0, static_cast<int>(r.options.size()) - 1)));
    r.onValue = [cb = std::move(onChange)](float v) { if (cb) cb(static_cast<int>(v)); };
    rows_.push_back(std::move(r));
    return static_cast<int>(rows_.size()) - 1;
}

int ControlPanel::AddButton(const std::string& label, std::function<void()> onPress)
{
    Row r;
    r.kind = Kind::Button;
    r.label = label;
    r.onPress = std::move(onPress);
    rows_.push_back(std::move(r));
    return static_cast<int>(rows_.size()) - 1;
}

int ControlPanel::AddManualButton(const std::string& label, std::function<void()> onPress)
{
    const int row = AddButton(label, std::move(onPress));
    rows_[static_cast<std::size_t>(row)].manual = true;
    return row;
}

int ControlPanel::AddReadout(const std::string& label, std::function<std::string()> text)
{
    Row r;
    r.kind = Kind::Readout;
    r.label = label;
    r.text = std::move(text);
    rows_.push_back(std::move(r));
    return static_cast<int>(rows_.size()) - 1;
}

void ControlPanel::AddHeading(const std::string& text)
{
    Row r;
    r.kind = Kind::Heading;
    r.label = text;
    rows_.push_back(std::move(r));
}

void ControlPanel::SetSlider(int row, float value)
{
    Row& r = rows_[static_cast<std::size_t>(row)];
    r.value = std::clamp(value, r.min, r.max);
    dirty_ = true;
}
void ControlPanel::SetToggle(int row, bool value)
{
    rows_[static_cast<std::size_t>(row)].value = value ? 1.0f : 0.0f;
    dirty_ = true;
}
void ControlPanel::SetChoice(int row, int index)
{
    Row& r = rows_[static_cast<std::size_t>(row)];
    r.value = static_cast<float>(std::clamp(index, 0, std::max(0, static_cast<int>(r.options.size()) - 1)));
    dirty_ = true;
}

float ControlPanel::Height() const
{
    const int h = kHeader + static_cast<int>(rows_.size()) * kRowHeight + kPad;
    return pose_.width * static_cast<float>(h) / static_cast<float>(pixelWidth_);
}

void ControlPanel::BuildStand(RoomBuilder& b)
{
    const float h = Height();
    const float bottom = pose_.y - h * 0.5f;
    const Vector3 n = Normal(pose_.yaw);
    // Post behind the screen, a base plate and a thin bezel; the collider keeps visitors from walking through the post.
    const float px = pose_.x - n.X * 0.06f, pz = pose_.z - n.Z * 0.06f;
    b.AddBox(Vector3(px - 0.05f, 0.0f, pz - 0.05f), Vector3(px + 0.05f, bottom, pz + 0.05f), "metal", 1.0f, Vector3(0.4f, 0.42f, 0.5f), true);
    b.AddBox(Vector3(px - 0.35f, 0.0f, pz - 0.35f), Vector3(px + 0.35f, 0.06f, pz + 0.35f), "metal", 1.0f, Vector3(0.3f, 0.32f, 0.38f), false);
    const Vector3 r = Right(pose_.yaw);
    const float span = pose_.width * 0.5f + 0.05f;
    const Vector3 c(pose_.x, 0.0f, pose_.z);
    // Bezel: a slab just behind the screen, a little wider than it, never in front of it.
    const Vector3 p0 = c - n * 0.08f - r * span;
    const Vector3 p1 = c - n * 0.012f + r * span;
    b.AddBox(Vector3(std::min(p0.X, p1.X), pose_.y - h * 0.5f - 0.05f, std::min(p0.Z, p1.Z)),
             Vector3(std::max(p0.X, p1.X), pose_.y + h * 0.5f + 0.05f, std::max(p0.Z, p1.Z)), "metal", 1.0f, Vector3(0.18f, 0.19f, 0.23f), false);
}

void ControlPanel::Load(InteractionSystem& interaction)
{
    interaction_ = &interaction;
    const int h = kHeader + static_cast<int>(rows_.size()) * kRowHeight + kPad;
    screen_ = std::make_unique<LiveScreen>(s_, pixelWidth_, h);
    dirty_ = true;

    const Vector3 centre = Centre();
    const Vector3 r = Right(WorldYaw()) * (pose_.width * 0.5f);
    const Vector3 n = Normal(WorldYaw()) * 0.25f;
    const float halfH = Height() * 0.5f;
    Interactable ia;
    const Vector3 a = centre - r - n - Vector3(0, halfH, 0);
    const Vector3 b = centre + r + n + Vector3(0, halfH, 0);
    ia.bounds = BoundingBox(Vector3::Min(a, b), Vector3::Max(a, b));
    ia.maxDistance = 6.0f;
    ia.wantsHold = true;
    ia.prompt = [this] { return Prompt(); };
    ia.interact = [this](const InteractEvent& e) { Handle(e); };
    ia.command = [this](const std::string& label, const float* value) { return Press(label, value); };
    interaction.Add(std::move(ia));
    int interactive = 0;
    for (const Row& row : rows_) interactive += (row.kind == Kind::Heading || row.kind == Kind::Readout) ? 0 : 1;
    interaction.AddControls(interactive);
}

int ControlPanel::RowAt(const Vector3& hit, float& xFraction) const
{
    const Vector3 centre = Centre();
    const Vector3 rel = hit - centre;
    const float u = Vector3::Dot(rel, Right(WorldYaw())) / pose_.width + 0.5f;
    const float v = 0.5f - rel.Y / Height();
    xFraction = u;
    const float py = v * Height() / pose_.width * static_cast<float>(pixelWidth_);
    const int row = static_cast<int>(std::floor((py - static_cast<float>(kHeader)) / static_cast<float>(kRowHeight)));
    if (row < 0 || row >= static_cast<int>(rows_.size()) || u < 0.0f || u > 1.0f) return -1;
    return row;
}

std::string ControlPanel::Prompt() const
{
    ControlPanel* self = const_cast<ControlPanel*>(this);
    float fx = 0.0f;
    const int row = interaction_ != nullptr ? RowAt(interaction_->LastHit(), fx) : -1;
    if (row != hover_) {
        self->hover_ = row;
        self->dirty_ = true;
    }
    if (row < 0) return "Control panel: " + title_;
    const Row& r = rows_[static_cast<std::size_t>(row)];
    switch (r.kind) {
        case Kind::Slider: return r.label + "  (click or drag, wheel nudges)";
        case Kind::Toggle: return std::string(r.value > 0.5f ? "Turn off " : "Turn on ") + r.label;
        case Kind::Choice: return r.label + "  (click: next, Q: previous)";
        case Kind::Button: return r.label;
        case Kind::Readout: return r.label;
        default: return title_;
    }
}

void ControlPanel::Handle(const InteractEvent& e)
{
    float fx = 0.0f;
    const int index = RowAt(e.hit, fx);
    if (index < 0) return;
    Row& r = rows_[static_cast<std::size_t>(index)];
    const auto fire = [&](float v) {
        r.value = v;
        if (r.onValue) r.onValue(v);
        dirty_ = true;
    };
    switch (r.kind) {
        case Kind::Slider: {
            if (e.pressed || e.held) {
                const float t = std::clamp((fx * static_cast<float>(pixelWidth_) - static_cast<float>(kPad)) / (static_cast<float>(pixelWidth_ - 2 * kPad)), 0.0f, 1.0f);
                float v = r.min + t * (r.max - r.min);
                if (r.step > 0.0f) v = r.min + std::round((v - r.min) / r.step) * r.step;
                if (std::abs(v - r.value) > 1e-6f) fire(std::clamp(v, r.min, r.max));
            }
            if (e.wheel != 0.0f) {
                const float step = r.step > 0.0f ? r.step : (r.max - r.min) / 20.0f;
                fire(std::clamp(r.value + step * e.wheel, r.min, r.max));
            }
            break;
        }
        case Kind::Toggle:
            if (e.pressed) fire(r.value > 0.5f ? 0.0f : 1.0f);
            break;
        case Kind::Choice: {
            const int n = std::max(1, static_cast<int>(r.options.size()));
            int i = static_cast<int>(r.value);
            if (e.pressed) i = (i + 1) % n;
            else if (e.alternate) i = (i + n - 1) % n;
            else if (e.wheel != 0.0f) i = (i + (e.wheel > 0 ? 1 : n - 1)) % n;
            else break;
            fire(static_cast<float>(i));
            break;
        }
        case Kind::Button:
            if (e.pressed && r.onPress) {
                r.onPress();
                dirty_ = true;
            }
            break;
        default: break;
    }
}

bool ControlPanel::Press(const std::string& label, const float* value)
{
    if (label == "*") {
        // Smoke test: operate every control through its whole range (sliders at min, middle and max; every choice
        // option; toggles both ways; buttons once). Exceptions propagate to the caller, which logs them.
        for (Row& r : rows_) {
            const auto fire = [&](float v) {
                r.value = v;
                if (r.onValue) r.onValue(v);
            };
            switch (r.kind) {
                case Kind::Slider: fire(r.min); fire((r.min + r.max) * 0.5f); fire(r.max); fire(r.min + (r.max - r.min) * 0.3f); break;
                case Kind::Toggle: fire(1.0f); fire(0.0f); fire(1.0f); fire(0.0f); break;
                case Kind::Choice:
                    for (std::size_t i = 0; i < r.options.size(); ++i) fire(static_cast<float>(i));
                    fire(0.0f);
                    break;
                case Kind::Button:
                    if (r.onPress && !r.manual) r.onPress();
                    break;
                default: break;
            }
        }
        dirty_ = true;
        return true;
    }
    if (label == "#") {
        // Fuzz step: one random control gets a random value (the extremes are chosen a quarter of the time, because that is where
        // renderers and CNA reject things). The generator is the panel's own, so a run is repeatable.
        if (value != nullptr && static_cast<std::uint32_t>(*value) != fuzzSeed_) {   // a new seed re-keys this panel's generator
            fuzzSeed_ = static_cast<std::uint32_t>(*value);
            fuzzState_ = 2463534242u ^ (fuzzSeed_ * 2654435761u);
            if (fuzzState_ == 0) fuzzState_ = 1;
        }
        std::vector<Row*> live;
        for (Row& r : rows_)
            if (r.kind == Kind::Slider || r.kind == Kind::Toggle || r.kind == Kind::Choice || (r.kind == Kind::Button && !r.manual)) live.push_back(&r);
        if (live.empty()) return false;
        const auto next = [this] {
            fuzzState_ ^= fuzzState_ << 13;
            fuzzState_ ^= fuzzState_ >> 17;
            fuzzState_ ^= fuzzState_ << 5;
            return fuzzState_;
        };
        Row& r = *live[next() % live.size()];
        const float unit = static_cast<float>(next() % 10001) / 10000.0f;
        switch (r.kind) {
            case Kind::Slider: {
                const std::uint32_t pick = next() % 8;
                r.value = pick == 0 ? r.min : pick == 1 ? r.max : r.min + (r.max - r.min) * unit;
                if (r.onValue) r.onValue(r.value);
                break;
            }
            case Kind::Toggle: r.value = r.value > 0.5f ? 0.0f : 1.0f; if (r.onValue) r.onValue(r.value); break;
            case Kind::Choice: r.value = static_cast<float>(next() % std::max<std::size_t>(1, r.options.size())); if (r.onValue) r.onValue(r.value); break;
            default: if (r.onPress) r.onPress(); break;
        }
        dirty_ = true;
        return true;
    }
    for (Row& r : rows_) {
        if (r.label.find(label) == std::string::npos) continue;
        const auto fire = [&](float v) {
            r.value = v;
            if (r.onValue) r.onValue(v);
            dirty_ = true;
        };
        switch (r.kind) {
            case Kind::Button:
                if (r.onPress) r.onPress();
                dirty_ = true;
                return true;
            case Kind::Toggle: fire(value != nullptr ? (*value > 0.5f ? 1.0f : 0.0f) : (r.value > 0.5f ? 0.0f : 1.0f)); return true;
            case Kind::Choice: {
                const int n = std::max(1, static_cast<int>(r.options.size()));
                fire(value != nullptr ? std::clamp(*value, 0.0f, static_cast<float>(n - 1)) : static_cast<float>((static_cast<int>(r.value) + 1) % n));
                return true;
            }
            case Kind::Slider:
                if (value == nullptr) return false;
                fire(std::clamp(*value, r.min, r.max));
                return true;
            default: continue;
        }
    }
    return false;
}

void ControlPanel::Redraw()
{
    const int w = pixelWidth_;
    const int h = kHeader + static_cast<int>(rows_.size()) * kRowHeight + kPad;
    const Color kBg(14, 18, 30, 255);
    screen_->Render(kBg, [&](SpriteBatch& sb, int, int) {
        Texture2D& white = *s_.textures->White();
        const auto rect = [&](int x, int y, int rw, int rh, Color c) { sb.Draw(white, Rectangle(x, y, rw, rh), c); };
        const auto text = [&](const SpriteFont& font, const std::string& t, float x, float y, Color c, float scale) {
            sb.DrawString(font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
        };
        rect(0, 0, w, 4, Color(200, 170, 90, 255));
        rect(0, h - 4, w, 4, Color(200, 170, 90, 255));
        text(*s_.titleFont, title_, 18.0f, 8.0f, Color(255, 226, 150, 255), std::min(0.62f, 590.0f / std::max(1.0f, s_.titleFont->MeasureString(title_).X)));
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            Row& r = rows_[i];
            const int y = kHeader + static_cast<int>(i) * kRowHeight;
            const bool hot = static_cast<int>(i) == hover_ && r.kind != Kind::Heading && r.kind != Kind::Readout;
            if (hot) rect(6, y + 2, w - 12, kRowHeight - 4, Color(40, 52, 84, 255));
            switch (r.kind) {
                case Kind::Heading:
                    text(*s_.font, r.label, 18.0f, static_cast<float>(y) + 16.0f, Color(255, 214, 90, 255), 0.88f);
                    rect(18, y + kRowHeight - 8, w - 36, 2, Color(90, 100, 130, 255));
                    break;
                case Kind::Slider: {
                    char buf[48];
                    std::snprintf(buf, sizeof buf, r.format, static_cast<double>(r.value));
                    text(*s_.font, r.label, 18.0f, static_cast<float>(y) + 2.0f, Color(230, 235, 245, 255), 0.84f);
                    const float vw = s_.font->MeasureString(buf).X * 0.84f;
                    text(*s_.font, buf, static_cast<float>(w) - 18.0f - vw, static_cast<float>(y) + 2.0f, Color(255, 226, 150, 255), 0.84f);
                    const int tx = kPad + 4, tw = w - 2 * (kPad + 4), ty = y + 44;
                    rect(tx, ty, tw, 10, Color(50, 58, 80, 255));
                    const float t = (r.max > r.min) ? (r.value - r.min) / (r.max - r.min) : 0.0f;
                    rect(tx, ty, static_cast<int>(t * static_cast<float>(tw)), 10, Color(110, 170, 255, 255));
                    rect(tx + static_cast<int>(t * static_cast<float>(tw)) - 4, ty - 5, 9, 20, Color(255, 255, 255, 255));
                    break;
                }
                case Kind::Toggle: {
                    const bool on = r.value > 0.5f;
                    text(*s_.font, r.label, 18.0f, static_cast<float>(y) + 16.0f, Color(230, 235, 245, 255), 0.88f);
                    rect(w - 110, y + 12, 86, 32, on ? Color(70, 170, 100, 255) : Color(90, 60, 66, 255));
                    rect(w - 110 + (on ? 52 : 4), y + 16, 30, 24, Color(240, 240, 245, 255));
                    break;
                }
                case Kind::Choice: {
                    const std::string v = r.options.empty() ? "" : r.options[static_cast<std::size_t>(r.value)];
                    text(*s_.font, r.label, 18.0f, static_cast<float>(y) + 16.0f, Color(230, 235, 245, 255), 0.88f);
                    const std::string shown = "< " + v + " >";
                    const float vw = s_.font->MeasureString(shown).X * 0.7f;
                    text(*s_.font, shown, static_cast<float>(w) - 18.0f - vw, static_cast<float>(y) + 16.0f, Color(255, 226, 150, 255), 0.88f);
                    break;
                }
                case Kind::Button:
                    rect(18, y + 8, w - 36, kRowHeight - 16, hot ? Color(90, 120, 190, 255) : Color(60, 84, 140, 255));
                    text(*s_.font, r.label, static_cast<float>(w) * 0.5f - s_.font->MeasureString(r.label).X * 0.35f, static_cast<float>(y) + 16.0f,
                         Color::White, 0.88f);
                    break;
                case Kind::Readout:
                    text(*s_.font, r.label, 18.0f, static_cast<float>(y) + 16.0f, Color(170, 180, 200, 255), 0.84f);
                    {
                        const float vw = s_.font->MeasureString(r.shown).X * 0.84f;
                        text(*s_.font, r.shown, static_cast<float>(w) - 18.0f - vw, static_cast<float>(y) + 16.0f, Color(150, 255, 170, 255), 0.84f);
                    }
                    break;
            }
        }
    });
    dirty_ = false;
}

void ControlPanel::RenderOffscreen(double time)
{
    if (!screen_) return;
    if (time >= nextPoll_) {
        nextPoll_ = time + 0.25;
        for (Row& r : rows_) {
            if (r.kind != Kind::Readout || !r.text) continue;
            std::string now = r.text();
            if (now != r.shown) {
                r.shown = std::move(now);
                dirty_ = true;
            }
        }
    }
    if (dirty_ || !screen_->HasContent()) Redraw();
}

void ControlPanel::Draw(const DrawContext& ctx)
{
    if (!screen_ || !screen_->HasContent()) return;
    RenderStates::SetOpaque3D(ctx.device);
    const Vector3 centre = Centre();
    const Vector3 r = Right(WorldYaw()) * pose_.width;
    const Vector3 up(0, Height(), 0);
    QuadDraw::Textured(ctx, screen_->Texture(), centre - r * 0.5f - up * 0.5f, r, up);
}
