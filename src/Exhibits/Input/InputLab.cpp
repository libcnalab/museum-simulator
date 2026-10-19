// Input laboratory: keyboard, press/release edges, mouse, the game pad (real or keyboard-emulated), the four player
// slots, text input with the clipboard, raw joysticks and the machine's other input devices. Every readout is a
// direct query of the CNA input API; nothing here requires a game pad, a joystick or a touch screen, and each room
// says plainly when the hardware it demonstrates is absent.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "CNA/Input/Clipboard.hpp"
#include "CNA/Input/Haptics.hpp"
#include "CNA/Input/InputDevices.hpp"
#include "CNA/Input/Joysticks.hpp"
#include "CNA/Input/Power.hpp"
#include "CNA/Input/TextInputType.hpp"
#include "CNA/Input/Sensors.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Input/Buttons.hpp"
#include "Microsoft/Xna/Framework/Input/GamePad.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"
#include "Microsoft/Xna/Framework/Input/MouseCursor.hpp"
#include "Microsoft/Xna/Framework/Input/TextInputEXT.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchPanel.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;
using namespace Microsoft::Xna::Framework::Input::Touch;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Keyboard, Edges, Mouse, Pad, Players, Text, Joystick, Devices, Touch };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"edges", Mode::Edges}, {"mouse", Mode::Mouse}, {"pad", Mode::Pad}, {"players", Mode::Players},
                                                         {"text", Mode::Text},   {"joystick", Mode::Joystick}, {"devices", Mode::Devices}, {"touch", Mode::Touch}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Keyboard;
}

struct KeyCap {
    const char* label;
    Keys key;
    float width;   // in key units
};

// A compact ANSI layout; each row is a list of keys left to right, a null label is a gap.
const std::vector<std::vector<KeyCap>>& Layout()
{
    static const std::vector<std::vector<KeyCap>> rows = {
        {{"Esc", Keys::Escape, 1.5f}, {"F1", Keys::F1, 1}, {"F2", Keys::F2, 1}, {"F3", Keys::F3, 1}, {"F4", Keys::F4, 1}, {"F5", Keys::F5, 1}, {"F6", Keys::F6, 1},
         {"F7", Keys::F7, 1}, {"F8", Keys::F8, 1}, {"F9", Keys::F9, 1}, {"F10", Keys::F10, 1}, {"F11", Keys::F11, 1}, {"F12", Keys::F12, 1}},
        {{"`", Keys::OemTilde, 1}, {"1", Keys::D1, 1}, {"2", Keys::D2, 1}, {"3", Keys::D3, 1}, {"4", Keys::D4, 1}, {"5", Keys::D5, 1}, {"6", Keys::D6, 1},
         {"7", Keys::D7, 1}, {"8", Keys::D8, 1}, {"9", Keys::D9, 1}, {"0", Keys::D0, 1}, {"-", Keys::OemMinus, 1}, {"=", Keys::OemPlus, 1}, {"Back", Keys::Back, 2}},
        {{"Tab", Keys::Tab, 1.5f}, {"Q", Keys::Q, 1}, {"W", Keys::W, 1}, {"E", Keys::E, 1}, {"R", Keys::R, 1}, {"T", Keys::T, 1}, {"Y", Keys::Y, 1},
         {"U", Keys::U, 1}, {"I", Keys::I, 1}, {"O", Keys::O, 1}, {"P", Keys::P, 1}, {"[", Keys::OemOpenBrackets, 1}, {"]", Keys::OemCloseBrackets, 1}, {"\\", Keys::OemPipe, 1.5f}},
        {{"Caps", Keys::CapsLock, 1.75f}, {"A", Keys::A, 1}, {"S", Keys::S, 1}, {"D", Keys::D, 1}, {"F", Keys::F, 1}, {"G", Keys::G, 1}, {"H", Keys::H, 1},
         {"J", Keys::J, 1}, {"K", Keys::K, 1}, {"L", Keys::L, 1}, {";", Keys::OemSemicolon, 1}, {"'", Keys::OemQuotes, 1}, {"Enter", Keys::Enter, 2.25f}},
        {{"Shift", Keys::LeftShift, 2.25f}, {"Z", Keys::Z, 1}, {"X", Keys::X, 1}, {"C", Keys::C, 1}, {"V", Keys::V, 1}, {"B", Keys::B, 1}, {"N", Keys::N, 1},
         {"M", Keys::M, 1}, {",", Keys::OemComma, 1}, {".", Keys::OemPeriod, 1}, {"/", Keys::OemQuestion, 1}, {"Shift", Keys::RightShift, 2.75f}},
        {{"Ctrl", Keys::LeftControl, 1.5f}, {"Win", Keys::LeftWindows, 1.25f}, {"Alt", Keys::LeftAlt, 1.25f}, {"Space", Keys::Space, 6.25f}, {"Alt", Keys::RightAlt, 1.25f},
         {"Win", Keys::RightWindows, 1.25f}, {"Ctrl", Keys::RightControl, 1.5f}},
    };
    return rows;
}

const char* PadTypeName(GamePadType t)
{
    switch (t) {
        case GamePadType::GamePad: return "GamePad";
        case GamePadType::Wheel: return "Wheel";
        case GamePadType::ArcadeStick: return "ArcadeStick";
        case GamePadType::FlightStick: return "FlightStick";
        case GamePadType::DancePad: return "DancePad";
        default: return "Unknown";
    }
}

class InputLab final : public ScreenExhibit {
public:
    InputLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))) {}

    ~InputLab() override { RestoreGlobals(); }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Keyboard:
                p.AddReadout("Keys down", [this] { return std::to_string(down_.size()); });
                p.AddReadout("Modifiers", [this] { return Modifiers(); });
                p.AddToggle("Label pressed keys by layout name", true, [this](bool v) { names_ = v; });
                p.AddReadout("Last key pressed", [this] { return lastKey_; });
                p.AddChoice("Look a key up by name", {"Space", "Return", "Left Shift", "A", "F5", "Escape", "Keypad Enter", "Not a key"}, 0, [this](int v) { LookUp(v); });
                p.AddReadout("Name / scancode lookup", [this] { return lookup_; });
                p.AddReadout("Scancode name / key name", [this] { return lookupNames_; });
                break;
            case Mode::Edges:
                p.AddChoice("Watch key", {"Space", "Z", "X", "Enter", "Left Shift"}, 0, [this](int v) { watch_ = v; Reset(); });
                p.AddButton("Reset counters", [this] { Reset(); });
                p.AddReadout("Presses (up to down)", [this] { return std::to_string(presses_); });
                p.AddReadout("Releases (down to up)", [this] { return std::to_string(releases_); });
                p.AddReadout("Current hold", [this] { return Fmt(hold_) + " s"; });
                p.AddReadout("Longest hold", [this] { return Fmt(longest_) + " s"; });
                break;
            case Mode::Mouse:
                p.AddChoice("Cursor shape (Esc shows it)", {"Arrow", "Crosshair", "Hand", "IBeam", "No", "SizeAll", "Wait", "WaitArrow", "SizeNS", "SizeWE", "SizeNESW", "SizeNWSE", "FromTexture2D (a ring made here)"}, 0, [this](int v) { SetCursor(v); });
                p.AddReadout("Position X, Y", [this] { return std::to_string(mx_) + ", " + std::to_string(my_); });
                p.AddReadout("Wheel (vertical)", [this] { return std::to_string(wheel_); });
                p.AddReadout("Wheel (horizontal)", [this] { return std::to_string(hwheel_); });
                p.AddButton("Mouse.SetPosition (window centre)", [this] {
                    const Viewport vp = S().device->getViewportProperty();
                    Mouse::SetPosition(vp.getWidthProperty() / 2, vp.getHeightProperty() / 2);
                });
                p.AddReadout("Mouse.WindowHandle", [] { return std::to_string(Mouse::getWindowHandleProperty()); });
                p.AddToggle("Capture the mouse (SetCaptureEXT)", false, [this](bool v) { try { captured_ = Mouse::SetCaptureEXT(v) ? v : false; } catch (const std::exception&) { captured_ = false; } });
                p.AddReadout("Mouse capture", [this] { return std::string(captured_ ? "on: events keep coming while a button is held outside the window" : "off"); });
                p.AddReadout("Relative mode", [] { return std::string(Mouse::getIsRelativeMouseModeEXTProperty() ? "on: X, Y are movement" : "off: X, Y are window pixels"); });
                break;
            case Mode::Pad:
                p.AddToggle("Keyboard emulation (CNAEXT)", false, [](bool v) { GamePad::setKeyboardEmulationEnabledEXT(v); });
                p.AddChoice("Dead zone", {"IndependentAxes", "Circular", "None"}, 0, [this](int v) { zone_ = v; });
                p.AddButton("Vibrate (SetVibration)", [this] { vibrated_ = GamePad::SetVibration(PlayerIndex::One, 1.0f, 1.0f); vibrateNote_ = vibrated_ ? "accepted" : "no motor: returned false"; GamePad::SetVibration(PlayerIndex::One, 0.0f, 0.0f); });
                p.AddReadout("SetVibration", [this] { return vibrateNote_; });
                p.AddButton("SetTriggerVibrationEXT / SetLightBarEXT", [this] { TriggerAndLight(); });
                p.AddReadout("Trigger motors and light bar", [this] { return triggerNote_; });
                p.AddReadout("Identity (type, GUID, gyro, touchpad, label of A)", [this] { return identityNote_; });
                p.AddReadout("GamePadCapabilities (present / possible)", [] { return CapabilitySummary(); });
                p.AddReadout("IsConnected", [this] { return std::string(pad_.getIsConnectedProperty() ? "yes" : "no"); });
                p.AddReadout("Packet number", [this] { return std::to_string(pad_.getPacketNumberProperty()); });
                p.AddReadout("IsButtonDown(Buttons.*)", [this] { return PressedButtons(); });
                p.AddReadout("IsButtonUp(A|B|X|Y)", [this] { return std::string(pad_.IsButtonUp(Buttons::A | Buttons::B | Buttons::X | Buttons::Y) ? "all four up" : "at least one held"); });
                p.AddReadout("Equals(previous state)", [this] { return std::string(pad_.Equals(previousPad_) ? "unchanged" : "changed"); });
                break;
            case Mode::Players:
                p.AddToggle("Keyboard emulation (CNAEXT)", false, [](bool v) { GamePad::setKeyboardEmulationEnabledEXT(v); });
                p.AddReadout("Emulation", [] { return std::string(GamePad::getKeyboardEmulationEnabledEXT() ? "on (player one only)" : "off"); });
                p.AddReadout("Connected pads", [this] { return std::to_string(connected_); });
                break;
            case Mode::Text:
                p.AddButton("Start typing (Esc stops)", [this] { StartTyping(); });
                p.AddButton("Clear", [this] { text_.clear(); });
                p.AddButton("Copy text to clipboard", [this] { Copy(); });
                p.AddButton("Paste clipboard", [this] { Paste(); });
                p.AddChoice("Text input type (on-screen keyboard)", {"Text", "TextName", "TextEmail", "TextUsername", "TextPasswordHidden", "TextPasswordVisible", "Number", "NumberPasswordHidden", "NumberPasswordVisible"}, 0, [this](int v) { inputType_ = v; if (typing_) StartTyping(); });
                p.AddButton("Copy as HTML too (SetDataEXT, text/html)", [this] { CopyHtml(); });
                p.AddButton("Set the primary selection (X11/Wayland middle-click)", [this] { SetPrimary(); });
                p.AddButton("Paste the primary selection", [this] { PastePrimary(); });
                p.AddReadout("Primary selection (HasPrimarySelectionTextEXT)", [this] { return primaryNote_; });
                p.AddReadout("Clipboard formats (GetMimeTypesEXT)", [this] { return mimeTypes_; });
                p.AddReadout("Text input", [this] { return std::string(typing_ ? "ACTIVE: keys type, Esc stops" : "off"); });
                p.AddReadout("Clipboard", [this] { return clipboardNote_; });
                break;
            case Mode::Joystick:
                p.AddButton("Rescan (GetJoysticksEXT)", [this] { Scan(); });
                p.AddReadout("Joysticks", [this] { return std::to_string(joysticks_.size()); });
                p.AddReadout("Selected", [this] { return joysticks_.empty() ? std::string("none connected") : joysticks_[static_cast<std::size_t>(selected_)].name; });
                break;
            case Mode::Touch:
                p.AddToggle("Mouse as touch (CNAEXT)", false, [](bool v) { TouchPanel::setMouseTouchEmulationEnabledEXT(v); });
                p.AddChoice("Enabled gestures", {"Tap, double tap, hold", "Drags and flick", "Pinch", "All of them"}, 3, [this](int v) { SetGestures(v); });
                p.AddButton("Simulate a tap", [this] { StartSim(1); });
                p.AddButton("Simulate a drag", [this] { StartSim(2); });
                p.AddButton("Simulate a pinch", [this] { StartSim(3); });
                p.AddReadout("Touch points now", [this] { return std::to_string(touches_); });
                p.AddReadout("Gestures seen", [this] { return std::to_string(gestureCount_); });
                break;
            case Mode::Devices:
                p.AddToggle("Mouse as touch (CNAEXT)", false, [](bool v) { TouchPanel::setMouseTouchEmulationEnabledEXT(v); });
                p.AddReadout("Mice", [this] { return std::to_string(mice_); });
                p.AddReadout("Keyboards", [this] { return std::to_string(keyboards_); });
                p.AddReadout("Touch devices", [this] { return std::to_string(touchDevices_); });
                p.AddReadout("Touch points now", [this] { return std::to_string(touches_); });
                break;
        }
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
        if (mode_ == Mode::Text) {
            token_ = TextInputEXT::TextInput.Add([this](SharpRuntime::charcs c) { OnChar(c); });
            subscribed_ = true;
        }
        if (mode_ == Mode::Joystick || mode_ == Mode::Devices) Scan();
        if (mode_ == Mode::Touch) {
            savedWidth_ = TouchPanel::getDisplayWidthProperty();
            savedHeight_ = TouchPanel::getDisplayHeightProperty();
            savedGestures_ = TouchPanel::getEnabledGesturesProperty();
            TouchPanel::setDisplayWidthProperty(960);
            TouchPanel::setDisplayHeightProperty(540);
            SetGestures(3);
            touchSetup_ = true;
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.playerInRoom && !ctx.nearby) return;
        time_ += ctx.dt;
        const KeyboardState ks = Keyboard::GetState();
        switch (mode_) {
            case Mode::Keyboard: UpdateKeyboard(ks); break;
            case Mode::Edges: UpdateEdges(ks, ctx.dt); break;
            case Mode::Mouse: UpdateMouse(); break;
            case Mode::Pad: previousPad_ = pad_;
                pad_ = zone_ == 0 ? GamePad::GetState(PlayerIndex::One, GamePadDeadZone::IndependentAxes)
                                               : (zone_ == 1 ? GamePad::GetState(PlayerIndex::One, GamePadDeadZone::Circular) : GamePad::GetState(PlayerIndex::One, GamePadDeadZone::None));
                try {
                    identityNote_ = PadIdentity();
                } catch (const std::exception& e) {
                    identityNote_ = std::string("pad identity unavailable: ") + e.what();
                }
                break;
            case Mode::Players: UpdatePlayers(); break;
            case Mode::Text:
                if (typing_ && !S().mutableStats->typingCapture) StopTyping();
                if (typing_ && ks.IsKeyDown(Keys::Back) && !prevBack_ && !text_.empty()) text_.pop_back();
                prevBack_ = ks.IsKeyDown(Keys::Back);
                break;
            case Mode::Joystick: UpdateJoystick(ctx.dt); break;
            case Mode::Devices: UpdateDevices(); break;
            case Mode::Touch: UpdateTouch(); break;
        }
    }

    void OnLeave() override { RestoreGlobals(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        const SamplerState& smp = SamplerState::LinearClamp;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &smp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto frame = [&](float x, float y, float w, float h, Color c) { rect(x, y, w, 2, c); rect(x, y + h - 2, w, 2, c); rect(x, y, 2, h, c); rect(x + w - 2, y, 2, h, c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.8f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(Title(), 24, 14, Kit::kGold, 1.0f);
        const Color lit = Kit::Rgba(255, 190, 70), off = Kit::Rgba(52, 62, 86), line = Kit::Rgba(120, 135, 170);
        switch (mode_) {
            case Mode::Keyboard: {
                const float unit = 56.0f, x0 = 24.0f, y0 = 70.0f;
                float y = y0;
                for (const auto& row : Layout()) {
                    float x = x0;
                    for (const KeyCap& k : row) {
                        const bool d = keys_.IsKeyDown(k.key);   // the snapshot taken this frame, asked key by key
                        rect(x, y, k.width * unit - 4, unit - 6, d ? lit : off);
                        text(k.label, x + 6, y + 10, d ? Kit::Rgba(20, 20, 20) : Kit::kText, 0.55f);
                        x += k.width * unit;
                    }
                    y += unit;
                }
                std::string pressed;
                for (const Keys k : down_) {
                    pressed += names_ ? KeyName(k) : std::to_string(static_cast<int>(k));
                    pressed += "  ";
                }
                text("GetPressedKeys(): " + (pressed.empty() ? std::string("(nothing held)") : pressed), 24, y + 14, Kit::kText, 0.8f);
                text("Modifiers: " + Modifiers(), 24, y + 44, Kit::kDim, 0.7f);
                break;
            }
            case Mode::Edges: {
                const char* rows[3] = {"IsKeyDown (level)", "pressed this frame (edge)", "released this frame (edge)"};
                for (int r = 0; r < 3; ++r) {
                    const float y = 110.0f + static_cast<float>(r) * 120.0f;
                    text(rows[r], 24, y - 30, Kit::kText, 0.75f);
                    rect(24, y, 912, 70, Kit::Rgba(18, 24, 38));
                    for (int i = 0; i < kHistory; ++i) {
                        const std::uint8_t v = history_[static_cast<std::size_t>((head_ + i) % kHistory)];
                        const bool on = r == 0 ? (v & 1) != 0 : (r == 1 ? (v & 2) != 0 : (v & 4) != 0);
                        if (on) rect(24.0f + static_cast<float>(i) * (912.0f / kHistory), y + (r == 0 ? 8.0f : 4.0f), r == 0 ? 912.0f / kHistory + 1 : 4.0f, r == 0 ? 54.0f : 62.0f, r == 0 ? lit : (r == 1 ? Kit::Rgba(110, 230, 150) : Kit::Rgba(240, 110, 110)));
                    }
                    frame(24, y, 912, 70, line);
                }
                text("time runs left to right; one column per frame. The edges are found by comparing this frame's state with the last.", 24, 490, Kit::kDim, 0.65f);
                break;
            }
            case Mode::Mouse: {
                // A mouse body whose buttons light up.
                rect(60, 90, 150, 220, Kit::Rgba(60, 70, 96));
                frame(60, 90, 150, 220, line);
                rect(66, 96, 66, 86, left_ ? lit : off);
                rect(138, 96, 66, 86, right_ ? lit : off);
                rect(120, 110, 20, 54, middle_ ? lit : Kit::Rgba(40, 48, 70));
                rect(30, 200, 28, 38, x1_ ? lit : off);
                rect(30, 250, 28, 38, x2_ ? lit : off);
                text("L", 92, 124, Kit::kText, 0.8f);
                text("R", 164, 124, Kit::kText, 0.8f);
                text("X1", 30, 205, Kit::kText, 0.5f);
                text("X2", 30, 255, Kit::kText, 0.5f);
                text("wheel " + std::to_string(wheel_), 70, 320, Kit::kText, 0.7f);
                // Position trail.
                rect(260, 90, 330, 200, Kit::Rgba(18, 24, 38));
                frame(260, 90, 330, 200, line);
                for (int i = 0; i < kTrail; ++i) {
                    const Vector2& v = trail_[static_cast<std::size_t>((trailHead_ + i) % kTrail)];
                    const float a = static_cast<float>(i) / kTrail;
                    rect(260.0f + std::clamp(v.X, 0.0f, 1.0f) * 322.0f, 90.0f + std::clamp(v.Y, 0.0f, 1.0f) * 192.0f, 6, 6, Color(static_cast<int>(255 * a), static_cast<int>(190 * a), 70, 255));
                }
                text(Mouse::getIsRelativeMouseModeEXTProperty() ? "trail of movement (relative mode)" : "trail of window position", 262, 296, Kit::kDim, 0.6f);
                // Wheel history.
                rect(620, 90, 316, 200, Kit::Rgba(18, 24, 38));
                frame(620, 90, 316, 200, line);
                rect(620, 188, 316, 2, line);
                for (int i = 0; i < kTrail; ++i) {
                    const float w = wheelHist_[static_cast<std::size_t>((trailHead_ + i) % kTrail)];
                    rect(622.0f + static_cast<float>(i) * 2.6f, w >= 0 ? 188 - std::min(95.0f, w * 30.0f) : 190, 2.5f, std::min(95.0f, std::abs(w) * 30.0f), Kit::Rgba(110, 190, 255));
                }
                text("scroll wheel notches per frame", 622, 296, Kit::kDim, 0.6f);
                text("Mouse.GetState(): X " + std::to_string(mx_) + "  Y " + std::to_string(my_) + "   buttons: " + std::string(left_ ? "L " : "") + (right_ ? "R " : "") + (middle_ ? "M " : "") + (x1_ ? "X1 " : "") + (x2_ ? "X2" : ""), 24, 380, Kit::kText, 0.8f);
                text(std::string("In this museum the mouse is captured for looking around (relative mode), so X and Y are movement since the last frame.\n") +
                         "Press Esc to release it: the cursor appears and X, Y become window pixels. The cursor shape control uses Mouse.SetCursor.",
                     24, 430, Kit::kDim, 0.65f);
                break;
            }
            case Mode::Pad: DrawPad(sb, rect, frame, text, pad_, 24, 70, lit, off, line); break;
            case Mode::Players: {
                for (int i = 0; i < 4; ++i) {
                    const float y = 80.0f + static_cast<float>(i) * 100.0f;
                    rect(24, y, 912, 86, Kit::Rgba(18, 24, 38));
                    frame(24, y, 912, 86, slot_[static_cast<std::size_t>(i)].connected ? lit : line);
                    text("Player " + std::to_string(i + 1) + (slot_[static_cast<std::size_t>(i)].connected ? "  CONNECTED" : "  empty"), 40, y + 10, slot_[static_cast<std::size_t>(i)].connected ? lit : Kit::kDim, 0.9f);
                    text(slot_[static_cast<std::size_t>(i)].detail, 40, y + 46, Kit::kText, 0.65f);
                }
                text("GamePad.GetState(PlayerIndex) reports four slots; GetCapabilities and the EXT queries describe each pad.", 24, 490, Kit::kDim, 0.65f);
                break;
            }
            case Mode::Text: {
                rect(24, 70, 912, 270, Kit::Rgba(18, 24, 38));
                frame(24, 70, 912, 270, typing_ ? lit : line);
                std::string shown = text_ + ((typing_ && std::fmod(time_, 1.0f) < 0.5f) ? "|" : "");
                float y = 82.0f;
                std::string lineText;
                for (const char c : shown) {
                    if (c == '\n' || lineText.size() >= 62) {
                        text(lineText, 40, y, Kit::kText, 0.8f);
                        y += 30;
                        lineText.clear();
                        if (c == '\n') continue;
                    }
                    lineText += c;
                }
                text(lineText, 40, y, Kit::kText, 0.8f);
                text(typing_ ? "TYPING: TextInputEXT delivers characters (layout, shift and IME aware). Esc stops." : "Press 'Start typing' on the control panel, then use the keyboard.", 24, 360, typing_ ? lit : Kit::kDim, 0.7f);
                text("Clipboard: " + clipboardNote_, 24, 400, Kit::kText, 0.7f);
                text("The 'Back' key is polled with Keyboard.GetState; printable characters arrive as TextInput events, which is how a text box must read them.", 24, 450, Kit::kDim, 0.6f);
                break;
            }
            case Mode::Joystick: {
                if (joysticks_.empty()) {
                    text("No joystick or controller is connected.", 40, 150, Kit::kText, 1.0f);
                    text("Joysticks.GetJoysticksEXT() returned an empty list; plug one in and press 'Rescan'.", 40, 200, Kit::kDim, 0.75f);
                    text("(The museum never requires one: everything has a keyboard and mouse path.)", 40, 240, Kit::kDim, 0.75f);
                    break;
                }
                const auto& j = joysticks_[static_cast<std::size_t>(selected_)];
                text(j.name + "   axes " + std::to_string(caps_.axisCount) + "  buttons " + std::to_string(caps_.buttonCount) + "  hats " + std::to_string(caps_.hatCount), 24, 66, Kit::kText, 0.75f);
                for (std::size_t a = 0; a < state_.axes.size() && a < 8; ++a) {
                    const float v = static_cast<float>(state_.axes[a]) / 32768.0f;
                    const float y = 110.0f + static_cast<float>(a) * 36.0f;
                    rect(120, y, 400, 22, Kit::Rgba(18, 24, 38));
                    rect(320.0f + std::min(0.0f, v) * 200.0f, y, std::abs(v) * 200.0f, 22, lit);
                    text("axis " + std::to_string(a), 24, y, Kit::kText, 0.6f);
                }
                for (std::size_t b = 0; b < state_.buttons.size() && b < 32; ++b) {
                    const float x = 560.0f + static_cast<float>(b % 8) * 46.0f, y = 110.0f + static_cast<float>(b / 8) * 46.0f;
                    rect(x, y, 40, 40, state_.buttons[b] ? lit : off);
                    text(std::to_string(b), x + 6, y + 8, state_.buttons[b] ? Kit::Rgba(20, 20, 20) : Kit::kText, 0.55f);
                }
                break;
            }
            case Mode::Touch: {
                rect(24, 70, 912, 330, Kit::Rgba(18, 24, 38));
                frame(24, 70, 912, 330, line);
                for (const TouchPoint& t : points_) {
                    const float px = 24.0f + t.position.X / 960.0f * 912.0f, py = 70.0f + t.position.Y / 540.0f * 330.0f;
                    rect(px - 14, py - 14, 28, 28, lit);
                    static const char* const states[] = {"Invalid", "Released", "Pressed", "Moved"};
                    // TouchLocation.TryGetPreviousLocation: where this finger was one frame earlier (false for a finger that was just pressed).
                    text("id " + std::to_string(t.id) + " " + states[std::clamp(t.state, 0, 3)] + (t.hasPrevious ? ", before " + Fmt(t.previous.X) + "," + Fmt(t.previous.Y) : std::string(", no previous")), px + 20, py - 10, Kit::kText, 0.5f);
                }
                float y = 410.0f;
                for (const std::string& g : gestureLog_) {
                    text(g, 30, y, Kit::kText, 0.55f);
                    y += 16.0f;
                }
                if (gestureLog_.empty()) text("No gesture yet: press a Simulate button, or switch on Mouse as touch and click and drag here.", 30, 410, Kit::kDim, 0.65f);
                break;
            }
            case Mode::Devices: {
                const std::string lines[] = {
                    "Mice: " + std::to_string(mice_) + "    Keyboards: " + std::to_string(keyboards_) + "    Touch devices: " + std::to_string(touchDevices_),
                    "TouchPanel.GetState(): " + std::to_string(touches_) + " touch point(s) now;  capabilities: " + (touchConnected_ ? "connected, max " + std::to_string(touchMax_) : std::string("no touch panel")),
                    "Sensors.GetSensorsEXT(): " + std::to_string(sensorCount_) + " sensor(s)" + (sensorCount_ > 0 ? "" : "   (typical for a desktop)"),
                    accel_ ? "Accelerometer: " + Fmt(accelV_.X) + ", " + Fmt(accelV_.Y) + ", " + Fmt(accelV_.Z) : std::string("Accelerometer: not available here"),
                    gyro_ ? "Gyroscope: " + Fmt(gyroV_.X) + ", " + Fmt(gyroV_.Y) + ", " + Fmt(gyroV_.Z) : std::string("Gyroscope: not available here"),
                    "Haptics.GetHapticsEXT(): " + std::to_string(hapticCount_) + " haptic device(s);  IsMouseHapticEXT: " + mouseHaptic_ +
                        (joysticks_.empty() ? std::string() : std::string(";  IsJoystickHapticEXT(first pad): ") + joystickHaptic_),
                    "Power.GetInfoEXT(): " + powerNote_,
                    padNote_,
                    "Mouse.GetGlobalPositionEXT: " + globalMouse_,
                    "Mouse-to-touch emulation: " + std::string(TouchPanel::getMouseTouchEmulationEnabledEXT() ? "ON (hold the left button to make a touch)" : "off (a mouse is not a touch screen)"),
                };
                float y = 66.0f;
                for (const std::string& l : lines) {
                    text(l, 30, y, Kit::kText, 0.8f);
                    y += 44.0f;
                }
                break;
            }
        }
        sb.End();
    }

private:
    static constexpr int kHistory = 300;
    static constexpr int kTrail = 120;

    struct Slot {
        bool connected = false;
        std::string detail = "GetState: IsConnected false";
    };

    static std::string Fmt(float v)
    {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.2f", static_cast<double>(v));
        return buf;
    }

    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"The keyboard", "Pressed, held, released", "The mouse", "The game pad", "Four player slots", "Text input and the clipboard", "Raw joysticks", "Other input devices", "Touch and gestures"};
        return t[static_cast<int>(mode_)];
    }

    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Keyboard: return {"Keyboard.GetState() returns a snapshot every frame: IsKeyDown(Keys) and GetPressedKeys(). The layout above", "lights each key you hold, including the ones that walk you around. Names come from the active keyboard layout."};
            case Mode::Edges: return {"A snapshot only says whether a key is down. To know it was pressed this frame, compare with last frame's", "KeyboardState: down now and up before is a press, up now and down before is a release. Hold a key, tap it, repeat."};
            case Mode::Touch: return {"TouchPanel reports touch points and recognises gestures: tap, double tap, hold, drags, flick and pinch. With no touch", "screen, mouse-to-touch emulation (CNAEXT) turns a drag into a touch, and the Simulate buttons move virtual fingers."};
            case Mode::Mouse: return {"Mouse.GetState() gives position, five buttons and the scroll wheel. The museum uses relative mode (CNAEXT) so", "looking never hits a screen edge; there X and Y are movement. Esc frees the pointer and Mouse.SetCursor changes its shape."};
            case Mode::Pad: return {"GamePad.GetState(PlayerIndex.One) reads sticks, triggers, buttons and the D-pad with a dead zone. No pad? Turn on", "CNA's keyboard emulation (WASD stick, arrows right stick, KLJI buttons, TFGH D-pad, QE bumpers, ZC triggers)."};
            case Mode::Players: return {"XNA addresses up to four pads by PlayerIndex. Each slot reports IsConnected, a PacketNumber and, through", "GetCapabilities and CNAEXT queries, its type and name. Emulation always lands on player one."};
            case Mode::Text: return {"Games read characters, not keys, with the CNAEXT TextInputEXT.TextInput event (layouts, shift and input methods", "handled for you). The CNAEXT Clipboard copies and pastes text. While typing, the museum's own hotkeys are off."};
            case Mode::Joystick: return {"CNAEXT Joysticks exposes any joystick or controller as raw axes, buttons and hats, beyond the standard XNA", "GamePad layout. Without hardware this room says so; with a device it draws every axis and button live."};
            default: return {"CNAEXT queries describe the machine: attached mice and keyboards, touch devices, motion sensors and power.", "Most desktops report no touch or sensors, and the room says so. Mouse-to-touch emulation lets you try TouchPanel anyway."};
        }
    }

    void RestoreGlobals()
    {
        if (subscribed_) {
            TextInputEXT::TextInput.Remove(token_);
            subscribed_ = false;
        }
        if (typing_) StopTyping();
        if (mode_ == Mode::Pad || mode_ == Mode::Players) GamePad::setKeyboardEmulationEnabledEXT(false);
        if (mode_ == Mode::Devices || mode_ == Mode::Touch) TouchPanel::setMouseTouchEmulationEnabledEXT(false);
        if (mode_ == Mode::Touch && touchSetup_) {
            touchSetup_ = false;
            TouchPanel::setDisplayWidthProperty(savedWidth_);
            TouchPanel::setDisplayHeightProperty(savedHeight_);
            TouchPanel::setEnabledGesturesProperty(savedGestures_);
        }
        if (mode_ == Mode::Mouse) {
            try { Mouse::SetCursor(MouseCursor::getArrowProperty()); } catch (const std::exception&) {}
        }
    }

    // --- keyboard ----------------------------------------------------------------------------------
    [[nodiscard]] bool IsDown(Keys k) const { return std::find(down_.begin(), down_.end(), k) != down_.end(); }

    static std::string KeyName(Keys k)
    {
        std::string n;
        try { n = Keyboard::GetKeyNameEXT(k); } catch (const std::exception&) {}
        return n.empty() ? "key " + std::to_string(static_cast<int>(k)) : n;
    }

    [[nodiscard]] std::string Modifiers() const
    {
        std::string s;
        const auto m = static_cast<unsigned>(mods_);
        if (m & 1u) s += "Shift ";
        if (m & 2u) s += "Ctrl ";
        if (m & 4u) s += "Alt ";
        if (m & 8u) s += "Gui ";
        if (m & 16u) s += "CapsLock ";
        if (m & 32u) s += "NumLock ";
        return s.empty() ? "none" : s;
    }

    void UpdateKeyboard(const KeyboardState& ks)
    {
        const std::vector<Keys> now = ks.GetPressedKeys();
        for (const Keys k : now)
            if (!IsDown(k)) lastKey_ = KeyName(k);
        down_ = now;
        keys_ = ks;
        try { mods_ = Keyboard::GetModStateEXT(); } catch (const std::exception&) {}
    }

    void Reset()
    {
        presses_ = releases_ = 0;
        hold_ = longest_ = 0.0f;
        history_.fill(0);
    }

    [[nodiscard]] Keys WatchedKey() const
    {
        static const Keys keys[] = {Keys::Space, Keys::Z, Keys::X, Keys::Enter, Keys::LeftShift};
        return keys[watch_];
    }

    void UpdateEdges(const KeyboardState& ks, float dt)
    {
        const bool now = ks.IsKeyDown(WatchedKey());
        const bool pressed = now && !prevDown_;
        const bool released = !now && prevDown_;
        if (pressed) ++presses_;
        if (released) ++releases_;
        hold_ = now ? hold_ + dt : 0.0f;
        longest_ = std::max(longest_, hold_);
        prevDown_ = now;
        history_[static_cast<std::size_t>(head_)] = static_cast<std::uint8_t>((now ? 1 : 0) | (pressed ? 2 : 0) | (released ? 4 : 0));
        head_ = (head_ + 1) % kHistory;
    }

    // --- mouse -------------------------------------------------------------------------------------
    void UpdateMouse()
    {
        const MouseState m = Mouse::GetState();
        mx_ = m.getXProperty();
        my_ = m.getYProperty();
        const int w = m.getScrollWheelValueProperty();
        const float notches = static_cast<float>(w - lastWheel_) / 120.0f;
        lastWheel_ = w;
        wheel_ = w / 120;
        hwheel_ = m.getHorizontalScrollWheelValueEXTProperty() / 120;
        left_ = m.getLeftButtonProperty() == ButtonState::Pressed;
        right_ = m.getRightButtonProperty() == ButtonState::Pressed;
        middle_ = m.getMiddleButtonProperty() == ButtonState::Pressed;
        x1_ = m.getXButton1Property() == ButtonState::Pressed;
        x2_ = m.getXButton2Property() == ButtonState::Pressed;
        Vector2 p;
        if (Mouse::getIsRelativeMouseModeEXTProperty()) {
            trailPos_.X = std::clamp(trailPos_.X + static_cast<float>(mx_) * 0.004f, 0.0f, 1.0f);
            trailPos_.Y = std::clamp(trailPos_.Y + static_cast<float>(my_) * 0.004f, 0.0f, 1.0f);
            p = trailPos_;
        } else {
            const Viewport vp = S().device->getViewportProperty();
            p = Vector2(static_cast<float>(mx_) / static_cast<float>(std::max(1, vp.getWidthProperty())), static_cast<float>(my_) / static_cast<float>(std::max(1, vp.getHeightProperty())));
        }
        trail_[static_cast<std::size_t>(trailHead_)] = p;
        wheelHist_[static_cast<std::size_t>(trailHead_)] = notches;
        trailHead_ = (trailHead_ + 1) % kTrail;
    }

    void SetCursor(int index)
    {
        try {
            switch (index) {
                case 1: Mouse::SetCursor(MouseCursor::getCrosshairProperty()); break;
                case 2: Mouse::SetCursor(MouseCursor::getHandProperty()); break;
                case 3: Mouse::SetCursor(MouseCursor::getIBeamProperty()); break;
                case 4: Mouse::SetCursor(MouseCursor::getNoProperty()); break;
                case 5: Mouse::SetCursor(MouseCursor::getSizeAllProperty()); break;
                case 6: Mouse::SetCursor(MouseCursor::getWaitProperty()); break;
                case 7: Mouse::SetCursor(MouseCursor::getWaitArrowProperty()); break;
                case 8: Mouse::SetCursor(MouseCursor::getSizeNSProperty()); break;
                case 9: Mouse::SetCursor(MouseCursor::getSizeWEProperty()); break;
                case 10: Mouse::SetCursor(MouseCursor::getSizeNESWProperty()); break;
                case 11: Mouse::SetCursor(MouseCursor::getSizeNWSEProperty()); break;
                case 12: {
                    // A cursor from a Texture2D: a 32 x 32 ring with a dot, hot spot in the middle.
                    if (!ringTexture_) {
                        std::vector<Color> px(32 * 32, Color(0, 0, 0, 0));
                        for (int y = 0; y < 32; ++y)
                            for (int x = 0; x < 32; ++x) {
                                const float d = std::sqrt((static_cast<float>(x) - 15.5f) * (static_cast<float>(x) - 15.5f) + (static_cast<float>(y) - 15.5f) * (static_cast<float>(y) - 15.5f));
                                if (d > 11.0f && d < 14.5f) px[static_cast<std::size_t>(y * 32 + x)] = Color(255, 210, 80, 255);
                                else if (d < 2.5f) px[static_cast<std::size_t>(y * 32 + x)] = Color(255, 255, 255, 255);
                            }
                        ringTexture_ = std::make_unique<Texture2D>(*S().device, 32, 32);
                        ringTexture_->SetData(px.data(), 32 * 32);
                    }
                    customCursor_ = std::make_unique<MouseCursor>(MouseCursor::FromTexture2D(*ringTexture_, 16, 16));
                    Mouse::SetCursor(*customCursor_);
                    break;
                }
                default: Mouse::SetCursor(MouseCursor::getArrowProperty()); break;
            }
        } catch (const std::exception&) {}
    }

    // --- game pad ----------------------------------------------------------------------------------
    // What player one's pad says about itself (the pad room and the devices room both show it); with no pad connected each
    // answer is empty, false or "unknown" rather than an error.
    [[nodiscard]] static std::string PadIdentity()
    {
        const PlayerIndex one = PlayerIndex::One;
        Vector3 gv;
        const int fingers = GamePad::GetTouchpadCountEXT(one) > 0 ? GamePad::GetTouchpadFingerCountEXT(one, 0) : 0;
        static const char* labels[] = {"unknown", "A", "B", "X", "Y", "Cross", "Circle", "Square", "Triangle"};
        const int label = std::clamp(static_cast<int>(GamePad::GetButtonLabelEXT(one, Buttons::A)), 0, 8);
        return "type " + std::string(PadTypeName(GamePad::GetCapabilities(one).getGamePadTypeProperty())) + ", GUID \"" + GamePad::GetGUIDEXT(one) + "\", gyro " +
               (GamePad::GetGyroEXT(one, gv) ? "yes" : "no") + ", player index " + std::to_string(GamePad::GetPlayerIndexEXT(one)) + ", touchpad fingers " +
               std::to_string(fingers) + ", the A button is printed as " + labels[label];
    }

    [[nodiscard]] static std::string CapabilitySummary()
    {
        const GamePadCapabilities c = GamePad::GetCapabilities(PlayerIndex::One);
        const int buttons = static_cast<int>(c.getHasAButtonProperty()) +
            static_cast<int>(c.getHasBButtonProperty()) + static_cast<int>(c.getHasXButtonProperty()) +
            static_cast<int>(c.getHasYButtonProperty()) + static_cast<int>(c.getHasBackButtonProperty()) +
            static_cast<int>(c.getHasStartButtonProperty()) + static_cast<int>(c.getHasBigButtonProperty()) +
            static_cast<int>(c.getHasLeftShoulderButtonProperty()) +
            static_cast<int>(c.getHasRightShoulderButtonProperty()) +
            static_cast<int>(c.getHasLeftStickButtonProperty()) +
            static_cast<int>(c.getHasRightStickButtonProperty());
        const int dpad = static_cast<int>(c.getHasDPadUpButtonProperty()) +
            static_cast<int>(c.getHasDPadDownButtonProperty()) +
            static_cast<int>(c.getHasDPadLeftButtonProperty()) +
            static_cast<int>(c.getHasDPadRightButtonProperty());
        const int axes = static_cast<int>(c.getHasLeftXThumbStickProperty()) +
            static_cast<int>(c.getHasLeftYThumbStickProperty()) +
            static_cast<int>(c.getHasRightXThumbStickProperty()) +
            static_cast<int>(c.getHasRightYThumbStickProperty());
        const int triggers = static_cast<int>(c.getHasLeftTriggerProperty()) +
            static_cast<int>(c.getHasRightTriggerProperty());
        const int motors = static_cast<int>(c.getHasLeftVibrationMotorProperty()) +
            static_cast<int>(c.getHasRightVibrationMotorProperty());
        return std::string(c.getIsConnectedProperty() ? "connected; " : "disconnected; ") +
            "buttons " + std::to_string(buttons) + "/11, d-pad " + std::to_string(dpad) +
            "/4, axes " + std::to_string(axes) + "/4, triggers " +
            std::to_string(triggers) + "/2, motors " + std::to_string(motors) +
            "/2, voice " + (c.getHasVoiceSupportProperty() ? "yes" : "no");
    }

    // Every Buttons flag the state reports as down: the bit-flag enum is how XNA asks "is this button held" in one call.
    [[nodiscard]] std::string PressedButtons() const
    {
        static const std::pair<Buttons, const char*> all[] = {{Buttons::A, "A"}, {Buttons::B, "B"}, {Buttons::X, "X"}, {Buttons::Y, "Y"}, {Buttons::Start, "Start"}, {Buttons::Back, "Back"},
                                                              {Buttons::LeftShoulder, "LB"}, {Buttons::RightShoulder, "RB"}, {Buttons::LeftStick, "LS"}, {Buttons::RightStick, "RS"},
                                                              {Buttons::DPadUp, "Up"}, {Buttons::DPadDown, "Down"}, {Buttons::DPadLeft, "Left"}, {Buttons::DPadRight, "Right"},
                                                              {Buttons::LeftTrigger, "LT"}, {Buttons::RightTrigger, "RT"}, {Buttons::BigButton, "Big"},
                                                              // The sticks as buttons: a direction counts as pressed once the stick is far enough over (a virtual d-pad).
                                                              {Buttons::LeftThumbstickUp, "LS-up"}, {Buttons::LeftThumbstickDown, "LS-down"}, {Buttons::LeftThumbstickLeft, "LS-left"}, {Buttons::LeftThumbstickRight, "LS-right"},
                                                              {Buttons::RightThumbstickUp, "RS-up"}, {Buttons::RightThumbstickDown, "RS-down"}, {Buttons::RightThumbstickLeft, "RS-left"}, {Buttons::RightThumbstickRight, "RS-right"}};
        std::string held;
        for (const auto& [flag, name] : all)
            if (pad_.IsButtonDown(flag)) held += (held.empty() ? "" : " ") + std::string(name);
        return held.empty() ? std::string("none held") : held;
    }

    template <typename Rect, typename Frame, typename Text>
    void DrawPad(SpriteBatch&, Rect& rect, Frame& frame, Text& text, const GamePadState& s, float x0, float y0, Color lit, Color off, Color line)
    {
        const bool emu = GamePad::getKeyboardEmulationEnabledEXT();
        if (!s.getIsConnectedProperty()) {
            text("No game pad is connected (IsConnected is false).", x0, y0 + 20, Kit::kText, 1.0f);
            text("Switch on 'Keyboard emulation' on the control panel: CNA then adds a virtual pad driven by the keyboard.", x0, y0 + 70, Kit::kDim, 0.75f);
            return;
        }
        text(emu ? "Keyboard-emulated pad (CNAEXT) merged into player one" : "A physical pad", x0, y0 - 6, lit, 0.75f);
        const auto stick = [&](float cx, float cy, const Vector2& v, const char* name, bool pressed) {
            rect(cx - 90, cy - 90, 180, 180, Kit::Rgba(18, 24, 38));
            frame(cx - 90, cy - 90, 180, 180, pressed ? lit : line);
            rect(cx - 90, cy - 1, 180, 2, off);
            rect(cx - 1, cy - 90, 2, 180, off);
            rect(cx + v.X * 80.0f - 9, cy - v.Y * 80.0f - 9, 18, 18, lit);
            text(std::string(name) + "  " + Fmt(v.X) + ", " + Fmt(v.Y), cx - 90, cy + 98, Kit::kText, 0.6f);
        };
        stick(130, 220, s.getThumbSticksProperty().getLeftProperty(), "left stick", s.getButtonsProperty().getLeftStickProperty() == ButtonState::Pressed);
        stick(500, 330, s.getThumbSticksProperty().getRightProperty(), "right stick", s.getButtonsProperty().getRightStickProperty() == ButtonState::Pressed);
        const auto trig = [&](float x, float v, const char* name) {
            rect(x, 80, 40, 100, Kit::Rgba(18, 24, 38));
            rect(x, 180 - v * 100.0f, 40, v * 100.0f, lit);
            frame(x, 80, 40, 100, line);
            text(std::string(name) + " " + Fmt(v), x - 10, 186, Kit::kText, 0.55f);
        };
        trig(260, s.getTriggersProperty().getLeftProperty(), "LT");
        trig(660, s.getTriggersProperty().getRightProperty(), "RT");
        const auto btn = [&](float x, float y, const char* label, ButtonState st) {
            rect(x, y, 56, 56, st == ButtonState::Pressed ? lit : off);
            text(label, x + 8, y + 12, st == ButtonState::Pressed ? Kit::Rgba(20, 20, 20) : Kit::kText, 0.7f);
        };
        const GamePadButtons& b = s.getButtonsProperty();
        btn(760, 230, "A", b.getAProperty());
        btn(820, 180, "B", b.getBProperty());
        btn(700, 180, "X", b.getXProperty());
        btn(760, 130, "Y", b.getYProperty());
        btn(300, 80, "LB", b.getLeftShoulderProperty());
        btn(600, 80, "RB", b.getRightShoulderProperty());
        btn(420, 130, "Back", b.getBackProperty());
        btn(500, 130, "Start", b.getStartProperty());
        const GamePadDPad& d = s.getDPadProperty();
        btn(330, 330, "Up", d.getUpProperty());
        btn(330, 420, "Dn", d.getDownProperty());
        btn(270, 375, "Lf", d.getLeftProperty());
        btn(390, 375, "Rt", d.getRightProperty());
        text(emu ? "WASD left stick, arrows right stick, KLJI = A B X Y, TFGH D-pad, QE bumpers, ZC triggers" : "", 24, 500, Kit::kDim, 0.55f);
    }

    void UpdatePlayers()
    {
        connected_ = 0;
        for (int i = 0; i < 4; ++i) {
            const PlayerIndex pi = static_cast<PlayerIndex>(i);
            const GamePadState s = GamePad::GetState(pi);
            Slot& slot = slot_[static_cast<std::size_t>(i)];
            slot.connected = s.getIsConnectedProperty();
            if (!slot.connected) {
                slot.detail = "GetState: IsConnected false";
                continue;
            }
            ++connected_;
            const GamePadCapabilities c = GamePad::GetCapabilities(pi);
            std::string name;
            try { name = GamePad::GetNameEXT(pi); } catch (const std::exception&) {}
            slot.detail = "type " + std::string(PadTypeName(c.getGamePadTypeProperty())) + "  packet " + std::to_string(s.getPacketNumberProperty()) + (name.empty() ? "" : "  name \"" + name + "\"") +
                          "  left stick " + Fmt(s.getThumbSticksProperty().getLeftProperty().X) + "," + Fmt(s.getThumbSticksProperty().getLeftProperty().Y);
        }
    }

    void TriggerAndLight()
    {
        try {
            const bool motors = GamePad::SetTriggerVibrationEXT(PlayerIndex::One, 1.0f, 1.0f);
            GamePad::SetTriggerVibrationEXT(PlayerIndex::One, 0.0f, 0.0f);
            GamePad::SetLightBarEXT(PlayerIndex::One, Color(80, 160, 255, 255));
            triggerNote_ = motors ? "trigger motors accepted; the light bar was set to blue" : "no trigger motors (returned false); the light bar call is ignored without a pad";
        } catch (const std::exception& e) {
            triggerNote_ = std::string(e.what()).substr(0, 60);
        }
    }

    // --- text and clipboard ------------------------------------------------------------------------
    void StartTyping()
    {
        typing_ = true;
        S().mutableStats->typingCapture = true;
        try {
            // The type picks what a touch device's on-screen keyboard offers (an email keyboard, digits only, a hidden password); a desktop ignores it.
            TextInputEXT::StartTextInputWithTypeEXT(static_cast<CNA::Input::TextInputTypeEXT>(inputType_));
        } catch (const std::exception&) {
            try { TextInputEXT::StartTextInput(); } catch (const std::exception&) {}
        }
    }
    void StopTyping()
    {
        typing_ = false;
        S().mutableStats->typingCapture = false;
        try { TextInputEXT::StopTextInput(); } catch (const std::exception&) {}
    }
    void OnChar(SharpRuntime::charcs c)
    {
        if (!typing_ || text_.size() > 300) return;
        if (c == u'\r' || c == u'\n') text_ += '\n';
        else if (c >= 32 && c < 127) text_ += static_cast<char>(c);
        else if (c >= 127) text_ += '?';
    }
    void LookUp(int which)
    {
        static const char* names[] = {"Space", "Return", "Left Shift", "A", "F5", "Escape", "Keypad Enter", "Not a key"};
        try {
            const Keys physical = Keyboard::GetScancodeFromNameEXT(names[which]);
            const Keys layout = Keyboard::GetKeyFromNameEXT(names[which]);
            const Keys translated = Keyboard::GetKeyFromScancodeEXT(physical);
            lookup_ = std::string("\"") + names[which] + "\": physical " +
                std::to_string(static_cast<int>(physical)) + " -> layout " +
                std::to_string(static_cast<int>(translated)) + ", named layout " +
                std::to_string(static_cast<int>(layout));
            lookupNames_ = "'" + Keyboard::GetScancodeNameEXT(physical) + "' / '" +
                Keyboard::GetKeyNameEXT(translated) + "'";
        } catch (const std::exception& e) {
            lookup_ = std::string("\"") + names[which] + "\": " + std::string(e.what()).substr(0, 50);
            lookupNames_ = "-";
        }
    }
    // The clipboard holds several formats of one copy: plain text and, here, the same text as an HTML fragment.
    void CopyHtml()
    {
        try {
            const std::string html = "<b>" + text_ + "</b>";
            const std::vector<std::uint8_t> bytes(html.begin(), html.end());
            const bool ok = CNA::Input::Clipboard::SetDataEXT("text/html", bytes);
            std::string list;
            for (const std::string& m : CNA::Input::Clipboard::GetMimeTypesEXT()) list += (list.empty() ? "" : ", ") + m;
            mimeTypes_ = list.empty() ? "none" : list;
            clipboardNote_ = ok ? "wrote text/html (" + std::to_string(bytes.size()) + " bytes)" : "the clipboard refused text/html";
        } catch (const std::exception& e) {
            clipboardNote_ = std::string("unavailable: ") + e.what();
        }
    }
    // The primary selection is a second, separate clipboard on X11 and Wayland: whatever was selected last, pasted with the middle button.
    void SetPrimary()
    {
        try {
            CNA::Input::Clipboard::SetPrimarySelectionTextEXT(text_.empty() ? std::string("selected in the museum") : text_);
            primaryNote_ = std::string("set; Has says ") + (CNA::Input::Clipboard::HasPrimarySelectionTextEXT() ? "yes" : "no");
            Museum::Log("input", "primary selection: %s", primaryNote_.c_str());
        } catch (const std::exception& e) {
            primaryNote_ = std::string("unavailable: ") + std::string(e.what()).substr(0, 50);
        }
    }
    void PastePrimary()
    {
        try {
            if (!CNA::Input::Clipboard::HasPrimarySelectionTextEXT()) {
                primaryNote_ = "no primary selection here";
                return;
            }
            std::string t = CNA::Input::Clipboard::GetPrimarySelectionTextEXT();
            for (char& c : t) if (c != '\n' && (c < 32 || c > 126)) c = '?';
            text_ += t.substr(0, 200);
            primaryNote_ = "pasted " + std::to_string(std::min<std::size_t>(t.size(), 200)) + " characters";
            Museum::Log("input", "primary selection: %s", primaryNote_.c_str());
        } catch (const std::exception& e) {
            primaryNote_ = std::string("unavailable: ") + std::string(e.what()).substr(0, 50);
        }
    }
    void Copy()
    {
        try {
            CNA::Input::Clipboard::SetTextEXT(text_);
            clipboardNote_ = "copied " + std::to_string(text_.size()) + " characters";
        } catch (const std::exception& e) {
            clipboardNote_ = std::string("unavailable: ") + e.what();
        }
    }
    void Paste()
    {
        try {
            if (!CNA::Input::Clipboard::HasTextEXT()) {
                clipboardNote_ = "the clipboard has no text";
                return;
            }
            std::string t = CNA::Input::Clipboard::GetTextEXT();
            for (char& c : t) if (c != '\n' && (c < 32 || c > 126)) c = '?';
            text_ += t.substr(0, 200);
            clipboardNote_ = "pasted " + std::to_string(std::min<std::size_t>(t.size(), 200)) + " characters";
        } catch (const std::exception& e) {
            clipboardNote_ = std::string("unavailable: ") + e.what();
        }
    }

    // --- joysticks and devices ---------------------------------------------------------------------
    void Scan()
    {
        try {
            joysticks_ = CNA::Input::Joysticks::GetJoysticksEXT();
        } catch (const std::exception&) {
            joysticks_.clear();
        }
        selected_ = 0;
        try {
            mice_ = static_cast<int>(CNA::Input::InputDevices::GetMiceEXT().size());
            keyboards_ = static_cast<int>(CNA::Input::InputDevices::GetKeyboardsEXT().size());
            touchDevices_ = static_cast<int>(CNA::Input::InputDevices::GetTouchDevicesEXT().size());
            hapticCount_ = static_cast<int>(CNA::Input::Haptics::GetHapticsEXT().size());
            mouseHaptic_ = CNA::Input::Haptics::IsMouseHapticEXT() ? "yes" : "no";
            if (!joysticks_.empty()) joystickHaptic_ = CNA::Input::Haptics::IsJoystickHapticEXT(joysticks_[0].id) ? "yes" : "no";
        } catch (const std::exception&) {}
    }
    void UpdateJoystick(float)
    {
        if (joysticks_.empty()) return;
        try {
            const auto id = joysticks_[static_cast<std::size_t>(selected_)].id;
            caps_ = CNA::Input::Joysticks::GetCapabilitiesEXT(id);
            state_ = CNA::Input::Joysticks::GetStateEXT(id);
        } catch (const std::exception&) {}
    }
    void SetGestures(int v)
    {
        GestureType g = GestureType::None;
        if (v == 0 || v == 3) g = g | GestureType::Tap | GestureType::DoubleTap | GestureType::Hold;
        if (v == 1 || v == 3) g = g | GestureType::HorizontalDrag | GestureType::VerticalDrag | GestureType::FreeDrag | GestureType::Flick | GestureType::DragComplete;
        if (v == 2 || v == 3) g = g | GestureType::Pinch | GestureType::PinchComplete;
        TouchPanel::setEnabledGesturesProperty(g);
    }
    void StartSim(int kind)
    {
        simKind_ = kind;
        simFrame_ = 0;
        // The platform marks a touch device as present on the first touch-down, and the per-frame TouchPanel update (previous locations,
        // removal of released fingers) only runs once it is. The simulated fingers stand in for a device while they move.
        if (flushFrames_ == 0) deviceWas_ = TouchPanel::getTouchDeviceExistsProperty();
        TouchPanel::setTouchDeviceExistsProperty(true);
        flushFrames_ = 0;
    }
    static const char* GestureName(GestureType t)
    {
        switch (t) {
            case GestureType::Tap: return "Tap";
            case GestureType::DoubleTap: return "DoubleTap";
            case GestureType::Hold: return "Hold";
            case GestureType::HorizontalDrag: return "HorizontalDrag";
            case GestureType::VerticalDrag: return "VerticalDrag";
            case GestureType::FreeDrag: return "FreeDrag";
            case GestureType::Pinch: return "Pinch";
            case GestureType::Flick: return "Flick";
            case GestureType::DragComplete: return "DragComplete";
            case GestureType::PinchComplete: return "PinchComplete";
            default: return "None";
        }
    }
    void UpdateTouch()
    {
        // Virtual fingers for the Simulate buttons, fed through the same entry point the platform's touch events use
        // (normalised 0..1 positions): a tap, a drag across the panel, or two fingers moving apart.
        if (simFrame_ >= 0) {
            const int frames = simKind_ == 1 ? 4 : (simKind_ == 2 ? 30 : 50);
            // The platform's touch path makes two calls per event: INTERNAL_setTouchState (the point TouchPanel.GetState reports, in pixels) and
            // INTERNAL_onTouchEvent (the gesture recogniser, normalised); doing both here makes the simulated fingers real touch points too.
            const auto event = [](int id, TouchLocationState st, float x, float y, float dx, float dy) {
                const float w = static_cast<float>(TouchPanel::getDisplayWidthProperty()), h = static_cast<float>(TouchPanel::getDisplayHeightProperty());
                TouchPanel::INTERNAL_setTouchState(id, st, Vector2(std::round(x * w), std::round(y * h)), 1.0f);
                TouchPanel::INTERNAL_onTouchEvent(id, st, x, y, dx, dy);
            };
            const float t = static_cast<float>(simFrame_) / static_cast<float>(frames);
            if (simKind_ == 1) {
                if (simFrame_ == 0) event(101, TouchLocationState::Pressed, 0.5f, 0.5f, 0, 0);
                if (simFrame_ == frames - 1) event(101, TouchLocationState::Released, 0.5f, 0.5f, 0, 0);
            } else if (simKind_ == 2) {
                const float x = 0.3f + 0.4f * t, y = 0.5f + 0.05f * t;
                if (simFrame_ == 0) event(101, TouchLocationState::Pressed, x, y, 0, 0);
                else if (simFrame_ == frames - 1) event(101, TouchLocationState::Released, x, y, 0, 0);
                else event(101, TouchLocationState::Moved, x, y, 0.4f / static_cast<float>(frames), 0.05f / static_cast<float>(frames));
            } else {
                const float ax = 0.5f - 0.06f - 0.17f * t, bx = 0.5f + 0.06f + 0.17f * t, y = 0.5f;
                const float d = 0.17f / static_cast<float>(frames);
                if (simFrame_ == 0) {
                    event(101, TouchLocationState::Pressed, ax, y, 0, 0);
                    event(102, TouchLocationState::Pressed, bx, y, 0, 0);
                } else if (simFrame_ == frames - 1) {
                    event(101, TouchLocationState::Released, ax, y, 0, 0);
                    event(102, TouchLocationState::Released, bx, y, 0, 0);
                } else {
                    event(101, TouchLocationState::Moved, ax, y, -d, 0);
                    event(102, TouchLocationState::Moved, bx, y, d, 0);
                }
            }
            simFrame_ = simFrame_ + 1 >= frames ? -1 : simFrame_ + 1;
            if (simFrame_ < 0) flushFrames_ = 4;
        } else if (flushFrames_ > 0 && --flushFrames_ == 0) {
            // Two frames after the release the panel has dropped the fingers; the museum has no touch device again unless it had one.
            TouchPanel::setTouchDeviceExistsProperty(deviceWas_);
        }
        try {
            const TouchCollection t = TouchPanel::GetState();
            touches_ = t.getCountProperty();
            if (std::getenv("MUSEUM_DIAG_TOUCH") != nullptr && (touches_ > 0 || simFrame_ >= 0)) Museum::Log("touch", "frame sim=%d touches=%d gesture-available=%d", simFrame_, touches_, TouchPanel::getIsGestureAvailableProperty() ? 1 : 0);
            points_.clear();
            for (int i = 0; i < t.getCountProperty(); ++i) {
                const TouchLocation& loc = t[static_cast<std::size_t>(i)];
                TouchPoint point{loc.getPositionProperty(), Vector2(0, 0), false, loc.getIdProperty(), static_cast<int>(loc.getStateProperty())};
                TouchLocation previous;
                point.hasPrevious = loc.TryGetPreviousLocation(previous);
                if (point.hasPrevious) point.previous = previous.getPositionProperty();
                points_.push_back(point);
            }
            int guard = 0;
            while (TouchPanel::getIsGestureAvailableProperty() && guard++ < 16) {
                const GestureSample g = TouchPanel::ReadGesture();
                // The timestamp counts from an arbitrary start (here the machine's uptime), so show it from the first gesture seen.
                if (gestureCount_ == 0) gestureEpoch_ = g.getTimestampProperty().getTotalSecondsProperty();
                ++gestureCount_;
                gestureLog_.insert(gestureLog_.begin(), std::string(GestureName(g.getGestureTypeProperty())) + "   at " + Fmt(g.getPositionProperty().X) + ", " + Fmt(g.getPositionProperty().Y) + "   delta " + Fmt(g.getDeltaProperty().X) + ", " + Fmt(g.getDeltaProperty().Y) + "   Timestamp +" + Fmt(static_cast<float>(g.getTimestampProperty().getTotalSecondsProperty() - gestureEpoch_)) + " s" +
                                          (g.getGestureTypeProperty() == GestureType::Pinch || g.getGestureTypeProperty() == GestureType::PinchComplete
                                               ? "   2nd finger at " + Fmt(g.getPosition2Property().X) + ", " + Fmt(g.getPosition2Property().Y) + " delta " + Fmt(g.getDelta2Property().X) + ", " + Fmt(g.getDelta2Property().Y)
                                               : std::string()));
                if (gestureLog_.size() > 8) gestureLog_.pop_back();
            }
        } catch (const std::exception&) {}
    }
    void UpdateDevices()
    {
        try {
            const TouchCollection t = TouchPanel::GetState();
            touches_ = t.getCountProperty();
            const TouchPanelCapabilities c = TouchPanel::GetCapabilities();
            touchConnected_ = c.getIsConnectedProperty();
            touchMax_ = c.getMaximumTouchCountProperty();
        } catch (const std::exception&) {}
        try {
            sensorCount_ = static_cast<int>(CNA::Input::Sensors::GetSensorsEXT().size());
            accel_ = CNA::Input::Sensors::GetAccelerometerEXT(accelV_);
            gyro_ = CNA::Input::Sensors::GetGyroscopeEXT(gyroV_);
        } catch (const std::exception&) {}
        try {
            // Questions about player one's pad that have an honest answer even with no pad: the extension queries report "nothing there".
            const PlayerIndex one = PlayerIndex::One;
            static const char* link[] = {"unknown link", "wired", "wireless"};
            int battery = -1;
            const auto power = GamePad::GetPowerInfoEXT(one, battery);
            padNote_ = std::string("GamePad extensions, player one: ") + link[std::clamp(static_cast<int>(GamePad::GetConnectionStateEXT(one)), 0, 2)] + ", power state " + std::to_string(static_cast<int>(power)) +
                       (battery >= 0 ? ", battery " + std::to_string(battery) + " %" : "") + ", touchpads " + std::to_string(GamePad::GetTouchpadCountEXT(one)) +
                       ", firmware " + std::to_string(GamePad::GetFirmwareVersionEXT(one)) + ", serial \"" + GamePad::GetSerialEXT(one) + "\"";
            identityNote_ = PadIdentity();
            int gx = 0, gy = 0;
            Mouse::GetGlobalPositionEXT(gx, gy);
            globalMouse_ = std::to_string(gx) + ", " + std::to_string(gy) + " (desktop pixels, whichever window has the pointer)";
        } catch (const std::exception& e) {
            padNote_ = std::string("GamePad extensions unavailable: ") + e.what();
        }
        try {
            int seconds = -1, percent = -1;
            const auto state = CNA::Input::Power::GetInfoEXT(seconds, percent);
            powerNote_ = "state " + std::to_string(static_cast<int>(state)) + ", " + (percent >= 0 ? std::to_string(percent) + " %" : std::string("no battery percentage"));
        } catch (const std::exception&) {
            powerNote_ = "unavailable";
        }
    }

    Mode mode_;
    std::string padNote_ = "GamePad extensions: -", globalMouse_ = "-";
    float time_ = 0.0f;
    // keyboard
    std::vector<Keys> down_;
    KeyboardState keys_;
    CNA::Input::KeyModifiersEXT mods_ = static_cast<CNA::Input::KeyModifiersEXT>(0);
    bool names_ = true;
    std::unique_ptr<Texture2D> ringTexture_;
    std::unique_ptr<MouseCursor> customCursor_;
    std::string lastKey_ = "(none yet)";
    std::string lookup_ = "-", lookupNames_ = "-", mimeTypes_ = "-", triggerNote_ = "-", identityNote_ = "-";
    int inputType_ = 0;
    bool captured_ = false;
    // edges
    int watch_ = 0, presses_ = 0, releases_ = 0;
    float hold_ = 0.0f, longest_ = 0.0f;
    bool prevDown_ = false;
    std::array<std::uint8_t, kHistory> history_{};
    int head_ = 0;
    // mouse
    int mx_ = 0, my_ = 0, wheel_ = 0, hwheel_ = 0, lastWheel_ = 0;
    bool left_ = false, right_ = false, middle_ = false, x1_ = false, x2_ = false;
    std::array<Vector2, kTrail> trail_{};
    std::array<float, kTrail> wheelHist_{};
    int trailHead_ = 0;
    Vector2 trailPos_{0.5f, 0.5f};
    // pad
    GamePadState pad_;
    GamePadState previousPad_;
    int zone_ = 0;
    bool vibrated_ = false;
    std::string vibrateNote_ = "not tried";
    std::array<Slot, 4> slot_{};
    int connected_ = 0;
    // text
    std::string text_;
    bool typing_ = false, subscribed_ = false, prevBack_ = false;
    std::uint64_t token_ = 0;
    std::string clipboardNote_ = "nothing copied yet", primaryNote_ = "-";
    // joystick / devices
    std::vector<CNA::Input::JoystickInfoEXT> joysticks_;
    int selected_ = 0;
    CNA::Input::JoystickCapabilitiesEXT caps_;
    CNA::Input::JoystickStateEXT state_;
    int simKind_ = 0, simFrame_ = -1, gestureCount_ = 0, savedWidth_ = 0, savedHeight_ = 0;
    GestureType savedGestures_ = GestureType::None;
    bool touchSetup_ = false;
    struct TouchPoint {
        Vector2 position, previous;
        bool hasPrevious;
        int id, state;
    };
    std::vector<TouchPoint> points_;
    double gestureEpoch_ = 0.0;
    int flushFrames_ = 0;
    bool deviceWas_ = false;
    std::vector<std::string> gestureLog_;
    int hapticCount_ = 0;
    std::string mouseHaptic_ = "?", joystickHaptic_ = "?";
    int mice_ = 0, keyboards_ = 0, touchDevices_ = 0, touches_ = 0, touchMax_ = 0, sensorCount_ = 0;
    bool touchConnected_ = false, accel_ = false, gyro_ = false;
    Vector3 accelV_, gyroV_;
    std::string powerNote_ = "...";
};

}  // namespace

std::unique_ptr<Exhibit> MakeInputLab(const RoomDef& room, Services& services)
{
    return std::make_unique<InputLab>(room, services);
}
