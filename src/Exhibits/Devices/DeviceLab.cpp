// Devices and the system: what a CNA game can ask of the machine outside its own window. Haptic devices
// (CNA::Input::Haptics), native dialogs and message boxes, the system tray and the URL launcher, the camera
// (CNA::Devices), the game window's own events and the device-loss notifications, and which platform and
// renderer family the build and the running device belong to. One class, one mode per room.
//
// Everything that opens something outside the museum (a dialog, a tray icon, a browser, a camera, a rumble)
// sits behind a manual button: a visitor or a test script naming it operates it, the smoke sweep never does
// (ControlPanel::AddManualButton). tools/devices_check.sh presses them on a private display with fake
// xdg-open/zenity programs and no session bus, so nothing reaches a real desktop. Every result is logged
// under [museum][devices] so the check can compare it with what it expects.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "CNA/DesktopOS.hpp"
#include "CNA/GraphicsRendererCategory.hpp"
#include "CNA/GraphicsRendererType.hpp"
#include "CNA/Input/Haptics.hpp"
#include "CNA/TargetPlatform.hpp"
#include "CNA/Unsupported3DGraphicsCallBehavior.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GameWindow.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/TextDropEventArgsEXT.hpp"
#ifdef CNA_DEVICES
#include "CNA/Devices/Camera.hpp"
#include "CNA/Devices/FileDialog.hpp"
#include "CNA/Devices/MessageBox.hpp"
#include "CNA/Devices/SystemTray.hpp"
#include "CNA/Devices/UrlLauncher.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Haptics, Dialogs, Tray, Camera, Window, Identity };

Mode ParseMode(const std::string& s)
{
    if (s == "dialogs") return Mode::Dialogs;
    if (s == "tray") return Mode::Tray;
    if (s == "camera") return Mode::Camera;
    if (s == "window") return Mode::Window;
    if (s == "identity") return Mode::Identity;
    return Mode::Haptics;
}

const char* YesNo(bool v) { return v ? "yes" : "no"; }

class DeviceLab final : public ScreenExhibit {
public:
    DeviceLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services), mode_(ParseMode(room.params.Get("mode"))) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Haptics:
                p.AddButton("Haptics.GetHapticsEXT (list devices)", [this] { ListHaptics(); });
                p.AddManualButton("Open the first device (OpenEXT)", [this] { OpenHaptic(); });
                p.AddManualButton("Rumble 50 % for 300 ms", [this] { Rumble(); });
                p.AddManualButton("Constant effect: create, run, stop, destroy", [this] { ConstantEffect(); });
                p.AddManualButton("Gain 50, autocenter 0, pause, resume", [this] { GainAndPause(); });
                p.AddButton("Close the device (Dispose)", [this] { CloseHaptic(); });
                p.AddReadout("Devices", [this] { return hapticList_; });
                p.AddReadout("Open device", [this] { return hapticState_; });
                p.AddReadout("Last result", [this] { return last_; });
                break;
            case Mode::Dialogs:
                p.AddManualButton("FileDialog.ShowOpenFile (images)", [this] { OpenFile(); });
                p.AddManualButton("FileDialog.ShowSaveFile", [this] { SaveFile(); });
                p.AddManualButton("FileDialog.ShowOpenFolder", [this] { OpenFolder(); });
                p.AddManualButton("MessageBox.ShowSimple (information)", [this] { SimpleBox(); });
                p.AddManualButton("MessageBox.Show (Yes / No / Cancel)", [this] { QuestionBox(); });
                p.AddReadout("Supported", [] { return Supported(); });
                p.AddReadout("Last dialog result", [this] { return DialogResult(); });
                break;
            case Mode::Tray:
                p.AddManualButton("Create a tray icon with two entries", [this] { CreateTray(); });
                p.AddButton("Toggle the checkable entry", [this] { ToggleTrayEntry(); });
                p.AddButton("Enable / disable the first entry", [this] { EnableTrayEntry(); });
                p.AddButton("Remove the tray icon", [this] { RemoveTray(); });
                p.AddManualButton("UrlLauncher.Open (the museum's local page)", [this] { LaunchUrl(); });
                p.AddReadout("Tray", [this] { return trayState_; });
                p.AddReadout("Last result", [this] { return last_; });
                break;
            case Mode::Camera:
                p.AddButton("Camera.AvailableCameras (list)", [this] { ListCameras(); });
                p.AddManualButton("Open the default camera", [this] { OpenCamera(); });
                p.AddButton("Close the camera", [this] { CloseCamera(); });
                p.AddReadout("Cameras", [this] { return cameraList_; });
                p.AddReadout("State", [this] { return CameraState(); });
                p.AddReadout("Frames shown", [this] { return std::to_string(frames_); });
                break;
            case Mode::Window:
                p.AddManualButton("GameWindow.MinimizeEXT, then RestoreEXT 2 s later", [this] { MinimizeRestore(); });
                p.AddButton("Emulate: RenderTarget2D.NotifyContentLostEXT", [this] { EmulateContentLoss(); });
                p.AddReadout("TextDropEXT (drag text onto the window)", [this] { return std::to_string(textDrops_) + (lastText_.empty() ? "" : ": \"" + lastText_ + "\""); });
                p.AddReadout("DeviceLost / Resetting / Reset", [this] { return std::to_string(deviceLost_) + " / " + std::to_string(deviceResetting_) + " / " + std::to_string(deviceReset_); });
                p.AddReadout("ContentLost events / IsContentLost", [this] { return std::to_string(contentLost_) + " / " + (target_ ? YesNo(target_->getIsContentLostProperty()) : "-"); });
                p.AddReadout("Last result", [this] { return last_; });
                break;
            case Mode::Identity:
                p.AddChoice("SetUnsupported3DGraphicsCallBehavior", {"Throw (default)", "WarnAndStub"}, 0, [this](int v) { SetUnsupported3D(v == 1); });
                p.AddReadout("Platform (compile time)", [] { return PlatformText(); });
                p.AddReadout("Desktop OS (run time)", [] { return DesktopText(); });
                p.AddReadout("Renderer family", [this] { return RendererFamily(); });
                p.AddReadout("Unsupported 3D calls", [this] { return Unsupported3DText(); });
                break;
        }
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
        if (mode_ == Mode::Window) {
            try {
                GameWindow& w = S().game->getWindowProperty();
                textToken_ = w.TextDropEXT.Add([this](System::Object*, const TextDropEventArgsEXT& e) {
                    ++textDrops_;
                    lastText_ = std::string(e.getTextProperty()).substr(0, 48);
                    Museum::Log("devices", "TextDropEXT: \"%s\"", lastText_.c_str());
                });
                GraphicsDevice& d = *S().device;
                lostToken_ = d.DeviceLost.Add([this](System::Object*, const System::EventArgs&) { ++deviceLost_; });
                resettingToken_ = d.DeviceResetting.Add([this](System::Object*, const System::EventArgs&) { ++deviceResetting_; });
                resetToken_ = d.DeviceReset.Add([this](System::Object*, const System::EventArgs&) { ++deviceReset_; });
                target_ = std::make_unique<RenderTarget2D>(d, 64, 64, false, SurfaceFormat::Color, DepthFormat::None, 0, RenderTargetUsage::PreserveContents);
                contentToken_ = target_->ContentLost.Add([this](System::Object*, const System::EventArgs&) { ++contentLost_; });
                subscribed_ = true;
            } catch (const std::exception& e) {
                last_ = std::string("subscribing failed: ") + e.what();
                ReportError(last_);
            }
        }
        if (mode_ == Mode::Haptics) ListHaptics();
        if (mode_ == Mode::Camera) ListCameras();
        if (mode_ == Mode::Identity) {
            try {
                originalUnsupported3D_ = S().device->GetUnsupported3DGraphicsCallBehavior();
                Museum::Log("devices", "identity: platform %s, desktop OS %s, renderer %s", PlatformText().c_str(), DesktopText().c_str(), RendererFamily().c_str());
            } catch (const std::exception& e) {
                ReportError(std::string("identity: ") + e.what());
            }
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
#ifdef CNA_DEVICES
        if (mode_ == Mode::Camera && camera_ != nullptr && cameraTexture_ != nullptr) {
            try {
                if (camera_->TryAcquireFrame(*cameraTexture_)) ++frames_;
            } catch (const std::exception& e) {
                last_ = std::string("TryAcquireFrame: ") + e.what();
                Museum::Log("devices", "camera: %s", last_.c_str());
                CloseCamera();
            }
        }
#endif
        if (mode_ == Mode::Window && target_ != nullptr && rebind_) {
            // Binding the target is what takes ownership of its contents again and clears IsContentLost.
            rebind_ = false;
            GraphicsDevice& d = *S().device;
            d.SetRenderTarget(target_.get());
            d.Clear(Color(40, 120, 200, 255));
            d.SetRenderTarget(nullptr);
            Museum::Log("devices", "after rebinding the target: IsContentLost %s", YesNo(target_->getIsContentLostProperty()));
        }
    }

    void OnLeave() override
    {
        CloseHaptic();
#ifdef CNA_DEVICES
        tray_.reset();
        camera_.reset();
#endif
        if (mode_ == Mode::Identity) SetUnsupported3D(originalUnsupported3D_ == CNA::Unsupported3DGraphicsCallBehavior::WarnAndStub);
        Unsubscribe();
    }

    ~DeviceLab() override
    {
        CloseHaptic();
        Unsubscribe();
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        text(Title(), 24, 12, Kit::kGold, 1.0f);
#ifdef CNA_DEVICES
        if (mode_ == Mode::Camera && cameraTexture_ != nullptr && frames_ > 0) {
            sb.Draw(*cameraTexture_, Rectangle(24, 70, 640, 360), Color::White);
            sb.End();
            return;
        }
#endif
        float y = 70.0f;
        for (const std::string& line : ScreenLines()) {
            text(line, 30, y, Kit::kText, 0.68f);
            y += 38.0f;
        }
        sb.End();
    }

private:
    [[nodiscard]] std::string Title() const
    {
        switch (mode_) {
            case Mode::Dialogs: return "Native Dialogs and Message Boxes";
            case Mode::Tray: return "The System Tray and the URL Launcher";
            case Mode::Camera: return "The Camera";
            case Mode::Window: return "Window Events and Device Loss";
            case Mode::Identity: return "Which Platform, Which Renderer Family";
            default: return "Haptic Devices";
        }
    }

    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Dialogs: return {"FileDialog opens the system's own open, save and folder pickers and answers through a callback; MessageBox shows a", "modal box and returns the button pressed. Both are CNAEXT and open only when you press their buttons."};
            case Mode::Tray: return {"SystemTray puts an icon with a menu into the desktop's notification area; its entries call back into the game. UrlLauncher", "hands a URL to the desktop. Hosts without a tray (or a browser) say so instead of failing silently."};
            case Mode::Camera: return {"CNA::Devices::Camera lists the cameras, opens one and copies each new frame into a Texture2D. With no camera, or with", "access refused, the state says NotSupported or Denied: the museum never invents a picture."};
            case Mode::Window: return {"GameWindow raises TextDropEXT for text dragged onto the window. DeviceLost, DeviceResetting and DeviceReset come only from a", "real device loss; the content-loss button is an emulation on this room's own render target, labelled as such."};
            case Mode::Identity: return {"TargetPlatform and the CNA_TARGET_* macros say what the build is for; DesktopOS what it runs on. A renderer belongs to a", "family (native, translation layer, software, web, diagnostic): the build's default is fixed, the running device may differ."};
            default: return {"CNA::Input::Haptics lists force-feedback devices and opens one: rumble, or effects that are created, run, stopped and", "destroyed, with gain, autocenter, pause and resume. No device? The list is empty and opening one is refused."};
        }
    }

    [[nodiscard]] std::vector<std::string> ScreenLines() const
    {
        std::vector<std::string> lines;
        switch (mode_) {
            case Mode::Haptics: lines = {"Devices: " + hapticList_, "Open: " + hapticState_, "Last: " + last_}; break;
            case Mode::Dialogs: lines = {"Supported: " + Supported(), "Last dialog: " + DialogResult(), "Dialogs open only when you press their buttons."}; break;
            case Mode::Tray: lines = {"Tray: " + trayState_, "Last: " + last_}; break;
            case Mode::Camera: lines = {"Cameras: " + cameraList_, "State: " + CameraState(), "Frames: " + std::to_string(frames_)}; break;
            case Mode::Window:
                lines = {"Text drops: " + std::to_string(textDrops_), "DeviceLost " + std::to_string(deviceLost_) + ", DeviceResetting " + std::to_string(deviceResetting_) + ", DeviceReset " + std::to_string(deviceReset_),
                         "ContentLost events " + std::to_string(contentLost_), "Last: " + last_};
                break;
            case Mode::Identity: lines = {"Platform: " + PlatformText(), "Desktop OS: " + DesktopText(), "Renderer: " + RendererFamily(), "Unsupported 3D calls: " + Unsupported3DText()}; break;
        }
        return lines;
    }

    // --- haptics ---------------------------------------------------------------------------------
    [[nodiscard]] bool Open() const { return haptic_.has_value() && haptic_->IsOpenEXT(); }

    void ListHaptics()
    {
        try {
            const auto devices = CNA::Input::Haptics::GetHapticsEXT();
            hapticList_ = std::to_string(devices.size()) + " device(s)";
            for (const auto& info : devices) hapticList_ += ", #" + std::to_string(info.id) + " " + info.name;
            hapticList_ += std::string("; mouse haptic ") + YesNo(CNA::Input::Haptics::IsMouseHapticEXT());
            firstHaptic_ = devices.empty() ? -1 : static_cast<long long>(devices.front().id);
        } catch (const std::exception& e) {
            hapticList_ = std::string("GetHapticsEXT: ") + e.what();
        }
        Museum::Log("devices", "haptics: %s", hapticList_.c_str());
    }

    void OpenHaptic()
    {
        try {
            if (firstHaptic_ < 0) {
                // Opening an id that does not exist is the refusal path: the device comes back closed.
                haptic_.emplace(CNA::Input::Haptics::OpenEXT(0xFFFFFFFFu));
                last_ = std::string("no haptic device; OpenEXT(unknown id) gave IsOpenEXT ") + YesNo(haptic_->IsOpenEXT());
            } else {
                haptic_.emplace(CNA::Input::Haptics::OpenEXT(static_cast<std::uint32_t>(firstHaptic_)));
                const auto caps = haptic_->GetCapabilitiesEXT();
                last_ = std::string("OpenEXT: open ") + YesNo(haptic_->IsOpenEXT()) + ", \"" + haptic_->GetNameEXT() + "\", " + std::to_string(caps.axisCount) + " axes, " +
                        std::to_string(caps.maxEffects) + " effects, rumble " + YesNo(caps.rumbleSupported);
            }
        } catch (const std::exception& e) {
            last_ = std::string("OpenEXT threw: ") + e.what();
        }
        hapticState_ = Open() ? haptic_->GetNameEXT() : "none";
        Museum::Log("devices", "haptics: %s", last_.c_str());
    }

    void Rumble()
    {
        if (!Open()) {
            last_ = "rumble refused: no open device";
        } else {
            const bool init = haptic_->InitRumbleEXT();
            const bool play = init && haptic_->PlayRumbleEXT(0.5f, 300);
            last_ = std::string("InitRumbleEXT ") + YesNo(init) + ", PlayRumbleEXT " + YesNo(play) + ", StopRumbleEXT " + YesNo(play && haptic_->StopRumbleEXT());
        }
        Museum::Log("devices", "haptics: %s", last_.c_str());
    }

    void ConstantEffect()
    {
        if (!Open()) {
            last_ = "effect refused: no open device";
        } else {
            CNA::Input::HapticEffectEXT effect;
            effect.type = CNA::Input::HapticEffectTypeEXT::Constant;
            effect.length = 400;
            effect.level = 12000;
            if (!haptic_->IsEffectSupportedEXT(effect)) {
                last_ = "the device does not support a constant effect";
            } else {
                const int id = haptic_->CreateEffectEXT(effect);
                effect.level = 16000;
                const bool updated = id >= 0 && haptic_->UpdateEffectEXT(id, effect);
                const bool ran = id >= 0 && haptic_->RunEffectEXT(id, 1);
                const bool playing = id >= 0 && haptic_->GetEffectStatusEXT(id);
                const bool stopped = id >= 0 && haptic_->StopEffectEXT(id);
                if (id >= 0) haptic_->DestroyEffectEXT(id);
                last_ = "CreateEffectEXT " + std::to_string(id) + ", update " + YesNo(updated) + ", run " + YesNo(ran) + ", status " + YesNo(playing) + ", stop " + YesNo(stopped) + ", destroyed";
            }
        }
        Museum::Log("devices", "haptics: %s", last_.c_str());
    }

    void GainAndPause()
    {
        if (!Open()) {
            last_ = "gain refused: no open device";
        } else {
            last_ = std::string("SetGainEXT ") + YesNo(haptic_->SetGainEXT(50)) + ", SetAutocenterEXT " + YesNo(haptic_->SetAutocenterEXT(0)) + ", PauseEXT " + YesNo(haptic_->PauseEXT()) +
                    ", ResumeEXT " + YesNo(haptic_->ResumeEXT()) + ", StopAllEffectsEXT " + YesNo(haptic_->StopAllEffectsEXT());
        }
        Museum::Log("devices", "haptics: %s", last_.c_str());
    }

    void CloseHaptic()
    {
        try {
            if (Open()) haptic_->StopAllEffectsEXT();
            if (haptic_) haptic_->Dispose();
            haptic_.reset();
        } catch (const std::exception& e) {
            Museum::Log("devices", "haptics: closing: %s", e.what());
        }
        hapticState_ = "none";
    }

    // --- dialogs ---------------------------------------------------------------------------------
    [[nodiscard]] static std::string Supported()
    {
#ifdef CNA_DEVICES
        try {
            return std::string("FileDialog ") + YesNo(CNA::Devices::FileDialog::getIsSupportedProperty()) + ", MessageBox " + YesNo(CNA::Devices::MessageBox::getIsSupportedProperty()) +
                   ", SystemTray " + YesNo(CNA::Devices::SystemTray::getIsSupportedProperty()) + ", Camera " + YesNo(CNA::Devices::Camera::getIsSupportedProperty());
        } catch (const std::exception& e) {
            return e.what();
        }
#else
        return "not compiled in (CNA_DEVICES)";
#endif
    }

    [[nodiscard]] std::string DialogResult() const
    {
        std::lock_guard<std::mutex> lock(dialogMutex_);
        return dialogResult_;
    }

    void SetDialogResult(const std::string& text)
    {
        {
            std::lock_guard<std::mutex> lock(dialogMutex_);
            dialogResult_ = text;
        }
        Museum::Log("devices", "%s", text.c_str());
    }

#ifdef CNA_DEVICES
    // The callback can come from the platform's own thread: it only stores text under the mutex.
    [[nodiscard]] CNA::Devices::FileDialog::ResultCallback Report(const std::string& what)
    {
        return [this, what](const std::vector<std::string>& files) {
            std::string text = what + ": " + (files.empty() ? std::string("cancelled (no file)") : std::to_string(files.size()) + " file(s)");
            for (const std::string& f : files) text += " " + f;
            SetDialogResult(text);
        };
    }
#endif

    void OpenFile()
    {
#ifdef CNA_DEVICES
        try {
            CNA::Devices::FileDialog::ShowOpenFile(Report("FileDialog.ShowOpenFile"), {{"Images", "png;jpg"}, {"All files", "*"}}, S().contentRoot, false);
            SetDialogResult("FileDialog.ShowOpenFile: shown, waiting for the callback");
        } catch (const std::exception& e) {
            SetDialogResult(std::string("FileDialog.ShowOpenFile refused: ") + e.what());
        }
#endif
    }

    void SaveFile()
    {
#ifdef CNA_DEVICES
        try {
            CNA::Devices::FileDialog::ShowSaveFile(Report("FileDialog.ShowSaveFile"), {{"Text", "txt"}}, S().contentRoot);
            SetDialogResult("FileDialog.ShowSaveFile: shown, waiting for the callback");
        } catch (const std::exception& e) {
            SetDialogResult(std::string("FileDialog.ShowSaveFile refused: ") + e.what());
        }
#endif
    }

    void OpenFolder()
    {
#ifdef CNA_DEVICES
        try {
            CNA::Devices::FileDialog::ShowOpenFolder(Report("FileDialog.ShowOpenFolder"), S().contentRoot, false);
            SetDialogResult("FileDialog.ShowOpenFolder: shown, waiting for the callback");
        } catch (const std::exception& e) {
            SetDialogResult(std::string("FileDialog.ShowOpenFolder refused: ") + e.what());
        }
#endif
    }

    void SimpleBox()
    {
#ifdef CNA_DEVICES
        try {
            CNA::Devices::MessageBox::ShowSimple(CNA::Devices::MessageBoxType::Information, "CNA Museum message", "MessageBox.ShowSimple: an information box.");
            SetDialogResult("MessageBox.ShowSimple returned");
        } catch (const std::exception& e) {
            SetDialogResult(std::string("MessageBox.ShowSimple refused: ") + e.what());
        }
#endif
    }

    void QuestionBox()
    {
#ifdef CNA_DEVICES
        try {
            const int pressed = CNA::Devices::MessageBox::Show(CNA::Devices::MessageBoxType::Warning, "CNA Museum message", "MessageBox.Show: which button?", {"Yes", "No", "Cancel"});
            SetDialogResult("MessageBox.Show returned button " + std::to_string(pressed) + (pressed >= 0 && pressed < 3 ? std::string(" (") + (pressed == 0 ? "Yes" : pressed == 1 ? "No" : "Cancel") + ")" : std::string(" (closed)")));
        } catch (const std::exception& e) {
            SetDialogResult(std::string("MessageBox.Show refused: ") + e.what());
        }
#endif
    }

    // --- tray and URL ----------------------------------------------------------------------------
    void CreateTray()
    {
#ifdef CNA_DEVICES
        try {
            if (!CNA::Devices::SystemTray::getIsSupportedProperty()) {
                trayState_ = "this host has no system tray (SystemTray.IsSupported false)";
            } else {
                tray_ = std::make_unique<CNA::Devices::SystemTray>("CNA 3D Museum");
                tray_->AddEntry("Say hello", false, false, true, [this] { ++trayClicks_; Museum::Log("devices", "tray entry clicked"); });
                tray_->AddEntry("Night mode", true, false, true, [this] { ++trayClicks_; });
                tray_->setTooltipProperty("CNA 3D Museum: Devices and the System");
                trayState_ = "icon shown with 2 entries";
            }
        } catch (const std::exception& e) {
            tray_.reset();
            trayState_ = std::string("SystemTray refused: ") + e.what();
        }
        Museum::Log("devices", "tray: %s", trayState_.c_str());
#endif
    }

    void ToggleTrayEntry()
    {
#ifdef CNA_DEVICES
        if (tray_ == nullptr) return;
        tray_->SetEntryChecked(1, !tray_->GetEntryChecked(1));
        tray_->SetEntryLabel(1, tray_->GetEntryChecked(1) ? "Night mode (on)" : "Night mode");
        trayState_ = std::string("checkable entry ") + (tray_->GetEntryChecked(1) ? "checked" : "unchecked") + ", clicks " + std::to_string(trayClicks_);
        Museum::Log("devices", "tray: %s", trayState_.c_str());
#endif
    }

    void EnableTrayEntry()
    {
#ifdef CNA_DEVICES
        if (tray_ == nullptr) return;
        tray_->SetEntryEnabled(0, !tray_->GetEntryEnabled(0));
        trayState_ = std::string("first entry ") + (tray_->GetEntryEnabled(0) ? "enabled" : "disabled");
        Museum::Log("devices", "tray: %s", trayState_.c_str());
#endif
    }

    void RemoveTray()
    {
#ifdef CNA_DEVICES
        if (tray_ == nullptr) return;
        tray_.reset();
        trayState_ = "icon removed";
        Museum::Log("devices", "tray: %s", trayState_.c_str());
#endif
    }

    void LaunchUrl()
    {
#ifdef CNA_DEVICES
        // A local page in the build tree, never an external site.
        try {
            const std::filesystem::path page = std::filesystem::absolute(std::filesystem::path(S().contentRoot) / "Textures" / "checker.png").lexically_normal();
            const std::string url = "file://" + page.string();
            const bool ok = CNA::Devices::UrlLauncher::Open(url);
            last_ = std::string("UrlLauncher.Open(") + url + ") returned " + (ok ? "true" : "false");
        } catch (const std::exception& e) {
            last_ = std::string("UrlLauncher.Open refused: ") + e.what();
        }
        Museum::Log("devices", "%s", last_.c_str());
#endif
    }

    // --- camera ----------------------------------------------------------------------------------
    void ListCameras()
    {
#ifdef CNA_DEVICES
        try {
            const auto cams = CNA::Devices::Camera::getAvailableCamerasProperty();
            static const char* where[] = {"unknown side", "front", "back"};
            cameraList_ = std::string("supported ") + YesNo(CNA::Devices::Camera::getIsSupportedProperty()) + ", " + std::to_string(cams.size()) + " camera(s)";
            for (const auto& c : cams) cameraList_ += ", " + c.Name + " (" + where[std::clamp(static_cast<int>(c.Position), 0, 2)] + ")";
        } catch (const std::exception& e) {
            cameraList_ = std::string("AvailableCameras: ") + e.what();
        }
#else
        cameraList_ = "not compiled in (CNA_DEVICES)";
#endif
        Museum::Log("devices", "camera: %s", cameraList_.c_str());
    }

    void OpenCamera()
    {
#ifdef CNA_DEVICES
        try {
            camera_ = std::make_unique<CNA::Devices::Camera>();
            const int w = std::max(1, camera_->getFrameWidthProperty()), h = std::max(1, camera_->getFrameHeightProperty());
            cameraTexture_ = std::make_unique<Texture2D>(*S().device, w, h);
            frames_ = 0;
            last_ = "camera opened, state " + CameraState() + ", " + std::to_string(w) + " x " + std::to_string(h);
        } catch (const std::exception& e) {
            camera_.reset();
            last_ = std::string("Camera refused: ") + e.what();
        }
        Museum::Log("devices", "camera: %s", last_.c_str());
#endif
    }

    void CloseCamera()
    {
#ifdef CNA_DEVICES
        camera_.reset();
        cameraTexture_.reset();
#endif
    }

    [[nodiscard]] std::string CameraState() const
    {
#ifdef CNA_DEVICES
        if (camera_ == nullptr) return "closed";
        static const char* names[] = {"NotSupported", "Closed", "Opening", "Denied", "Ready", "Lost"};
        return names[std::clamp(static_cast<int>(camera_->getStateProperty()), 0, 5)];
#else
        return "not compiled in";
#endif
    }

    // --- window and device loss ------------------------------------------------------------------
    // Minimised for two seconds, then restored, so the minimised state can be seen (and checked from outside:
    // tools/window_check.sh reads the window manager's WM_STATE in between). The restore is deferred through the game, which
    // keeps counting while the museum is paused: losing focus to the minimise pauses it.
    void MinimizeRestore()
    {
        try {
            S().game->getWindowProperty().MinimizeEXT();
            last_ = "MinimizeEXT returned; RestoreEXT follows in 2 s";
            S().after(2.0f, [game = S().game] {
                try {
                    game->getWindowProperty().RestoreEXT();
                    Museum::Log("devices", "window: RestoreEXT returned");
                } catch (const std::exception& e) {
                    Museum::Log("devices", "window: RestoreEXT refused: %s", e.what());
                }
            });
        } catch (const std::exception& e) {
            last_ = std::string("MinimizeEXT refused: ") + e.what();
        }
        Museum::Log("devices", "window: %s", last_.c_str());
    }

    void EmulateContentLoss()
    {
        if (target_ == nullptr) return;
        // An emulation, not a device loss: only this room's own target is told its contents are gone.
        target_->NotifyContentLostEXT();
        last_ = std::string("emulated: ContentLost raised ") + std::to_string(contentLost_) + " time(s), IsContentLost " + YesNo(target_->getIsContentLostProperty());
        Museum::Log("devices", "window: %s", last_.c_str());
        rebind_ = true;
    }

    void Unsubscribe()
    {
        if (!subscribed_) return;
        subscribed_ = false;
        try {
            S().game->getWindowProperty().TextDropEXT.Remove(textToken_);
            GraphicsDevice& d = *S().device;
            d.DeviceLost.Remove(lostToken_);
            d.DeviceResetting.Remove(resettingToken_);
            d.DeviceReset.Remove(resetToken_);
            if (target_) target_->ContentLost.Remove(contentToken_);
        } catch (const std::exception& e) {
            Museum::Log("devices", "unsubscribing: %s", e.what());
        }
        target_.reset();
    }

    // --- identity --------------------------------------------------------------------------------
    [[nodiscard]] static std::string PlatformText()
    {
        std::string macros;
#ifdef CNA_TARGET_APPLE
        macros += " CNA_TARGET_APPLE";
#endif
#ifdef CNA_TARGET_MACOS
        macros += " CNA_TARGET_MACOS";
#endif
#ifdef CNA_TARGET_IOS
        macros += " CNA_TARGET_IOS";
#endif
        return std::string(CNA::getCurrentPlatformName()) + ", mobile " + YesNo(CNA::isMobilePlatform()) + ", Apple " + YesNo(CNA::isApplePlatform()) +
               (CNA::getCurrentPlatform() == CNA::TargetPlatform::Desktop ? ", TargetPlatform::Desktop" : "") + (macros.empty() ? ", no CNA_TARGET_* macro" : "," + macros);
    }

    [[nodiscard]] static std::string DesktopText()
    {
        // getCurrentDesktopOS throws on a platform that is not a desktop (documented), for example the browser build.
        if (CNA::getCurrentPlatform() != CNA::TargetPlatform::Desktop) return "none: this platform is not a desktop";
        switch (CNA::getCurrentDesktopOS()) {
            case CNA::DesktopOS::Windows: return "Windows";
            case CNA::DesktopOS::Linux: return "Linux";
            case CNA::DesktopOS::MacOSX: return "MacOSX";
            default: return "Other";
        }
    }

    [[nodiscard]] std::string RendererFamily() const
    {
        try {
            const CNA::GraphicsRendererType running = S().device->GetGraphicsRendererType();
            const CNA::GraphicsRendererType built = CNA::getCurrentGraphicsRendererType();
            return std::string(CNA::getGraphicsRendererName(running)) + " is " + std::string(CNA::toStringView(CNA::getGraphicsRendererCategory(running))) +
                   "; the build's default " + std::string(CNA::getGraphicsRendererName(built)) + " is " + std::string(CNA::toStringView(CNA::getCurrentGraphicsRendererCategory()));
        } catch (const std::exception& e) {
            return e.what();
        }
    }

    [[nodiscard]] std::string Unsupported3DText() const
    {
        try {
            return S().device->GetUnsupported3DGraphicsCallBehavior() == CNA::Unsupported3DGraphicsCallBehavior::WarnAndStub ? "WarnAndStub (logged once, stubbed)" : "Throw";
        } catch (const std::exception& e) {
            return e.what();
        }
    }

    void SetUnsupported3D(bool stub)
    {
        try {
            S().device->SetUnsupported3DGraphicsCallBehavior(stub ? CNA::Unsupported3DGraphicsCallBehavior::WarnAndStub : CNA::Unsupported3DGraphicsCallBehavior::Throw);
        } catch (const std::exception& e) {
            ReportError(std::string("SetUnsupported3DGraphicsCallBehavior: ") + e.what());
        }
    }

    Mode mode_;
    std::string last_ = "-";
    // haptics. Held in an optional filled from OpenEXT: HapticDevice's inline default constructor needs the complete
    // CNA::Platform::IPlatformHapticDevice, which no public header provides, so a default-constructed member does not
    // compile outside CNA (CNA-DEMOS-ISSUE-009, reproduced here; the moving constructor is out of line and works).
    std::optional<CNA::Input::HapticDevice> haptic_;
    long long firstHaptic_ = -1;
    std::string hapticList_ = "-", hapticState_ = "none";
    // dialogs
    mutable std::mutex dialogMutex_;
    std::string dialogResult_ = "-";
    // tray, camera
    std::string trayState_ = "no icon", cameraList_ = "-";
    int trayClicks_ = 0, frames_ = 0;
#ifdef CNA_DEVICES
    std::unique_ptr<CNA::Devices::SystemTray> tray_;
    std::unique_ptr<CNA::Devices::Camera> camera_;
    std::unique_ptr<Texture2D> cameraTexture_;
#endif
    // window
    std::unique_ptr<RenderTarget2D> target_;
    std::size_t textToken_ = 0, lostToken_ = 0, resettingToken_ = 0, resetToken_ = 0, contentToken_ = 0;
    int textDrops_ = 0, deviceLost_ = 0, deviceResetting_ = 0, deviceReset_ = 0, contentLost_ = 0;
    std::string lastText_;
    bool subscribed_ = false, rebind_ = false;
    // identity
    CNA::Unsupported3DGraphicsCallBehavior originalUnsupported3D_ = CNA::Unsupported3DGraphicsCallBehavior::Throw;
};

}  // namespace

std::unique_ptr<Exhibit> MakeDeviceLab(const RoomDef& room, Services& services)
{
    return std::make_unique<DeviceLab>(room, services);
}
