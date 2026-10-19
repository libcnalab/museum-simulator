#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Exhibits/Services.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Rendering/LiveScreen.hpp"
#include "World/Catalogue.hpp"

class RoomBuilder;

/**
 * An in-world control console: a screen on a stand whose rows are sliders, toggles, choices,
 * buttons and live read-outs. The screen is a RenderTarget2D drawn with SpriteBatch/SpriteFont
 * (redrawn only when something changed), and the visitor operates it by pointing at a row:
 *   click / E   press (toggle, next choice, button) or set a slider from where you point
 *   hold        drag a slider
 *   wheel       nudge a slider or step a choice
 *   Q / right   previous choice
 * One Interactable covers the whole screen; the hit point selects the row.
 */
class ControlPanel {
public:
    /** Placement in room-local metres: centre of the screen, which way it faces (yaw 0 faces +Z) and its width. */
    struct Pose {
        float x = 0.0f, y = 1.5f, z = 0.0f;
        float yaw = 0.0f;
        float width = 1.8f;
    };

    ControlPanel(Services& services, const RoomDef& room, std::string title, const Pose& pose);

    int AddSlider(const std::string& label, float min, float max, float value, std::function<void(float)> onChange,
                  const char* format = "%.2f", float step = 0.0f);
    int AddToggle(const std::string& label, bool value, std::function<void(bool)> onChange);
    int AddChoice(const std::string& label, std::vector<std::string> options, int index, std::function<void(int)> onChange);
    int AddButton(const std::string& label, std::function<void()> onPress);
    /**
     * A button that opens something outside the museum (a system dialog, a tray icon, a browser, a camera): the visitor
     * or a test script naming it (press:LABEL) operates it, but the smoke sweep ("*") and fuzzing ("#") skip it, so an
     * automatic tour never opens a window on somebody's desktop or blocks on a modal dialog.
     */
    int AddManualButton(const std::string& label, std::function<void()> onPress);
    /** A row of live text; `text` is polled a few times a second. */
    int AddReadout(const std::string& label, std::function<std::string()> text);
    void AddHeading(const std::string& text);

    [[nodiscard]] float SliderValue(int row) const { return rows_[static_cast<std::size_t>(row)].value; }
    [[nodiscard]] bool ToggleValue(int row) const { return rows_[static_cast<std::size_t>(row)].value > 0.5f; }
    [[nodiscard]] int ChoiceValue(int row) const { return static_cast<int>(rows_[static_cast<std::size_t>(row)].value); }
    void SetSlider(int row, float value);
    void SetToggle(int row, bool value);
    void SetChoice(int row, int index);

    /** Post, base plate and a collider, added to the room's baked geometry. Call from Exhibit::BuildStatic. */
    void BuildStand(RoomBuilder& builder);
    void Load(InteractionSystem& interaction);
    void RenderOffscreen(double time);
    void Draw(const DrawContext& ctx);

    [[nodiscard]] float Height() const;   // metres
    /** The direction the screen faces in the world (the pose is in the room's local frame). */
    [[nodiscard]] float WorldYaw() const { return pose_.yaw + room_.frameYaw; }
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 Centre() const { return room_.ToWorld(pose_.x, pose_.y, pose_.z); }

private:
    std::uint32_t fuzzState_ = 2463534242u, fuzzSeed_ = 0;
    enum class Kind { Slider, Toggle, Choice, Button, Readout, Heading };
    struct Row {
        Kind kind = Kind::Heading;
        std::string label;
        float min = 0.0f, max = 1.0f, value = 0.0f, step = 0.0f;
        const char* format = "%.2f";
        std::vector<std::string> options;
        std::function<void(float)> onValue;      // slider, toggle (0/1), choice (index)
        std::function<void()> onPress;           // button
        std::function<std::string()> text;       // readout
        std::string shown;                       // last text drawn for a readout
        bool manual = false;                     // a button the sweep and fuzzing skip (AddManualButton)
    };

    [[nodiscard]] int RowAt(const Microsoft::Xna::Framework::Vector3& hit, float& xFraction) const;
    void Handle(const InteractEvent& event);
    /** Scripted use (tests): operates the first row whose label contains `label`. A slider takes `value`. */
    bool Press(const std::string& label, const float* value);
    /** Label "*" sweeps every control; label "#" is one random fuzz step (see ControlPanel.cpp). */
    void Redraw();
    [[nodiscard]] std::string Prompt() const;

    Services& s_;
    const RoomDef& room_;
    std::string title_;
    Pose pose_;
    int pixelWidth_ = 640;
    std::vector<Row> rows_;
    std::unique_ptr<LiveScreen> screen_;
    InteractionSystem* interaction_ = nullptr;
    int hover_ = -1;
    int shownHover_ = -2;
    bool dirty_ = true;
    double nextPoll_ = 0.0;
};
