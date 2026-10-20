// Phone sensor laboratory: Microsoft.Devices, the Windows Phone side of XNA 4.0 that CNA implements. Environment.DeviceType, the four
// motion sensors (Accelerometer, Gyroscope, Compass, Motion) with their Start/Stop, State, IsSupported, CurrentValue and the
// CurrentValueChanged event, and VibrateController. A desktop usually has none of them, so every readout says what the machine reports;
// the accelerometer has a CNA extension that makes the arrow keys tilt it, which gives the ball on the screen something to roll with.
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <thread>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Devices/DeviceType.hpp"
#include "Microsoft/Devices/Environment.hpp"
#include "Microsoft/Devices/Sensors/Accelerometer.hpp"
#include "Microsoft/Devices/Sensors/Compass.hpp"
#include "Microsoft/Devices/Sensors/Gyroscope.hpp"
#include "Microsoft/Devices/Sensors/Motion.hpp"
#include "Microsoft/Devices/VibrateController.hpp"
#include "System/TimeSpan.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
namespace Sensors = Microsoft::Devices::Sensors;

namespace {

const char* StateName(Sensors::SensorState s)
{
    switch (s) {
        case Sensors::SensorState::NotSupported: return "NotSupported";
        case Sensors::SensorState::Ready: return "Ready";
        case Sensors::SensorState::Initializing: return "Initializing";
        case Sensors::SensorState::NoData: return "NoData";
        case Sensors::SensorState::NoPermissions: return "NoPermissions";
        default: return "Disabled";
    }
}

std::string Fmt(double v)
{
    char buffer[24];
    std::snprintf(buffer, sizeof buffer, "%.2f", v);
    return buffer;
}
std::string Fmt3(const Vector3& v) { return Fmt(static_cast<double>(v.X)) + ", " + Fmt(static_cast<double>(v.Y)) + ", " + Fmt(static_cast<double>(v.Z)); }

class PhoneSensorLab final : public ScreenExhibit {
public:
    PhoneSensorLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    ~PhoneSensorLab() override { Release(); }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Phone sensors", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddToggle("Accelerometer.KeyboardEmulationEnabledEXT (Esc frees the keys)", false, [this](bool v) { Emulate(v); });
        p.AddToggle("Accelerometer.Start", false, [this](bool v) { Run(accel_, v, accelNote_); WatchAccelerometer(v); });
        p.AddToggle("Gyroscope.Start", false, [this](bool v) { Run(gyro_, v, gyroNote_); });
        p.AddToggle("Compass.Start", false, [this](bool v) { Run(compass_, v, compassNote_); });
        p.AddToggle("Motion.Start", false, [this](bool v) { Run(motion_, v, motionNote_); });
        p.AddButton("VibrateController.Start(300 ms)", [this] { Vibrate(); });
        p.AddButton("Legacy Accelerometer.ReadingChanged (one emulated reading)", [this] { LegacyReadingChanged(); });
        p.AddReadout("Environment.DeviceType", [] { return std::string(Microsoft::Devices::Environment::getDeviceTypeProperty() == Microsoft::Devices::DeviceType::Emulator ? "Emulator" : "Device"); });
        p.AddReadout("Accelerometer.IsSupported", [] { return std::string(Sensors::Accelerometer::getIsSupportedProperty() ? "yes" : "no"); });
        p.AddReadout("Gyroscope.IsSupported", [] { return std::string(Sensors::Gyroscope::getIsSupportedProperty() ? "yes" : "no"); });
        p.AddReadout("Compass.IsSupported", [] { return std::string(Sensors::Compass::getIsSupportedProperty() ? "yes" : "no"); });
        p.AddReadout("Motion.IsSupported", [] { return std::string(Sensors::Motion::getIsSupportedProperty() ? "yes" : "no"); });
        p.AddReadout("Last action", [this] { return last_; });
        AddInfoSign(b, "Windows Phone's device API",
                    {"Microsoft.Devices is the part of XNA 4.0 for phones: Accelerometer, Gyroscope, Compass and Motion deliver readings, VibrateController buzzes, Environment says whether this is a device or the emulator.",
                     "CNA implements them over the platform's sensors. Most desktops have none, so IsSupported is false and Start is refused; CNA adds an opt-in keyboard mode that lets the arrow keys tilt the accelerometer."});
    }

    void OnLeave() override { Release(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        Text(sb, "Microsoft.Devices.Sensors", 24, 6, 0.8f, Kit::kGold);
        float y = 54.0f;
        Vector3 tilt(0, 0, 0);
        bool haveTilt = false;
        const auto line = [&](const std::string& label, bool supported, const std::string& state, const std::string& value) {
            Text(sb, label + ": " + (supported ? "supported" : "not supported") + ", state " + state, 24, y, 0.55f, supported ? Kit::Rgba(150, 230, 160) : Kit::kDim);
            y += 22.0f;
            if (!value.empty()) { Text(sb, value, 40, y, 0.5f, Kit::kText); y += 22.0f; }
        };
        {
            std::string value = accelNote_;
            const bool supported = Sensors::Accelerometer::getIsSupportedProperty();
            if (accel_ && started_[0]) {
                try {
                    const Sensors::AccelerometerReading r = accel_->getCurrentValueProperty();
                    tilt = r.getAccelerationProperty();
                    haveTilt = accel_->getIsDataValidProperty();
                    value = (haveTilt ? "acceleration " + Fmt3(tilt) : std::string("no data yet")) + "   (" + std::to_string(accelEvents_.load()) + " CurrentValueChanged events)";
                } catch (const std::exception& e) { value = e.what(); }
            }
            line("Accelerometer" + std::string(Sensors::Accelerometer::getKeyboardEmulationEnabledEXT() ? " (arrow keys)" : ""), supported, accel_ ? StateName(accel_->getStateProperty()) : "not created", value);
        }
        {
            std::string value = gyroNote_;
            if (gyro_ && started_[1]) try { value = "rotation rate " + Fmt3(gyro_->getCurrentValueProperty().getRotationRateProperty()); } catch (const std::exception& e) { value = e.what(); }
            line("Gyroscope", Sensors::Gyroscope::getIsSupportedProperty(), gyro_ ? StateName(gyro_->getStateProperty()) : "not created", value);
        }
        {
            std::string value = compassNote_;
            if (compass_ && started_[2]) try { value = "magnetic heading " + Fmt(compass_->getCurrentValueProperty().getMagneticHeadingProperty()) + " deg"; } catch (const std::exception& e) { value = e.what(); }
            line("Compass", Sensors::Compass::getIsSupportedProperty(), compass_ ? StateName(compass_->getStateProperty()) : "not created", value);
        }
        {
            std::string value = motionNote_;
            if (motion_ && started_[3]) {
                try {
                    const Sensors::MotionReading r = motion_->getCurrentValueProperty();
                    value = "yaw " + Fmt(static_cast<double>(r.getAttitudeProperty().getYawProperty())) + ", pitch " + Fmt(static_cast<double>(r.getAttitudeProperty().getPitchProperty())) + ", roll " + Fmt(static_cast<double>(r.getAttitudeProperty().getRollProperty())) + ", gravity " + Fmt3(r.getGravityProperty());
                } catch (const std::exception& e) { value = e.what(); }
            }
            line("Motion", Sensors::Motion::getIsSupportedProperty(), motion_ ? StateName(motion_->getStateProperty()) : "not created", value);
        }
        // The tilt plate: a ball that follows the accelerometer's X and Y (1 g is the edge).
        const float cx = 700.0f, cy = 330.0f, r = 150.0f;
        rect(cx - r, cy - r, r * 2.0f, r * 2.0f, Kit::Rgba(24, 32, 52));
        rect(cx - r, cy - 1, r * 2.0f, 2, Kit::Rgba(70, 84, 120));
        rect(cx - 1, cy - r, 2, r * 2.0f, Kit::Rgba(70, 84, 120));
        const float bx = cx + std::clamp(tilt.X, -1.0f, 1.0f) * (r - 14.0f), by = cy - std::clamp(tilt.Y, -1.0f, 1.0f) * (r - 14.0f);
        rect(bx - 14, by - 14, 28, 28, haveTilt ? Kit::Rgba(255, 190, 70) : Kit::Rgba(80, 80, 90));
        Text(sb, haveTilt ? "tilt plate: the ball follows Accelerometer X and Y" : "tilt plate: waiting for accelerometer readings", cx - r, cy + r + 8, 0.5f, Kit::kDim);
        y = std::max(y, 280.0f);
        Text(sb, "VibrateController: " + vibrate_, 24, y + 10, 0.55f, Kit::kText);
        sb.End();
    }

private:
    void Text(SpriteBatch& sb, const std::string& s, float x, float y, float scale, Color c)
    {
        sb.DrawString(*S().font, s.size() > 110 ? s.substr(0, 107) + "..." : s, Vector2(x, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
    }

    template <typename T>
    void Run(std::unique_ptr<T>& sensor, bool on, std::string& note)
    {
        try {
            if (on) {
                if (!sensor) sensor = std::make_unique<T>();
                sensor->Start();
                note = "started";
                last_ = "Start accepted";
            } else if (sensor) {
                sensor->Stop();
                note = "stopped";
                last_ = "Stop";
            }
            Mark(sensor.get(), on);
        } catch (const std::exception& e) {
            note = std::string("Start refused: ") + std::string(e.what()).substr(0, 80);
            last_ = "Start refused";
            Mark(sensor.get(), false);
        }
    }

    void Mark(const void* sensor, bool on)
    {
        if (sensor == accel_.get()) started_[0] = on;
        else if (sensor == gyro_.get()) started_[1] = on;
        else if (sensor == compass_.get()) started_[2] = on;
        else if (sensor == motion_.get()) started_[3] = on;
    }

    // CurrentValueChanged: counts the readings the accelerometer delivers (on its own thread, hence the atomic).
    void WatchAccelerometer(bool on)
    {
        if (!accel_) return;
        if (on && accelToken_ == 0) accelToken_ = accel_->CurrentValueChanged.Add([this](System::Object*, const Sensors::SensorReadingEventArgs<Sensors::AccelerometerReading>&) { ++accelEvents_; });
        if (!on && accelToken_ != 0) { accel_->CurrentValueChanged.Remove(accelToken_); accelToken_ = 0; }
    }

    void Emulate(bool on)
    {
        try {
            if (accel_ && started_[0]) { accel_->Stop(); started_[0] = false; }
            Sensors::Accelerometer::setKeyboardEmulationEnabledEXT(on);
            S().mutableStats->typingCapture = on;   // the arrow keys tilt instead of walking
            last_ = on ? "arrow keys tilt the accelerometer (start it)" : "keyboard emulation off";
        } catch (const std::exception& e) {
            last_ = std::string("refused: ") + std::string(e.what()).substr(0, 60);
        }
    }

    void Vibrate()
    {
        try {
            auto* controller = Microsoft::Devices::VibrateController::getDefaultProperty();
            vibrate_ = std::string(controller->getIsSupportedProperty() ? "supported" : "not supported") + " (" + controller->getDeviceNameProperty() + ")";
            controller->Start(System::TimeSpan::FromMilliseconds(300.0));
            last_ = "Start(300 ms) accepted";
        } catch (const std::exception& e) {
            vibrate_ = std::string("Start refused: ") + std::string(e.what()).substr(0, 70);
            last_ = "VibrateController refused";
        }
    }

    // Windows Phone 7.0's Accelerometer.ReadingChanged (obsolete, still raised) next to the SensorBase CurrentValueChanged.
    // No phone here: keyboard emulation lets a sensor start, and CNA's InjectSyntheticSensorUpdate hands it one reading
    // in m/s^2 (labelled EMULATED). Both events must carry it, CurrentValueChanged first, in units of g.
    void LegacyReadingChanged()
    {
        const auto check = [this](const std::string& name, const std::string& expected, const std::string& got) {
            Museum::Log("check", "%s %s: %s (expected %s, got %s)", R().key.c_str(), name.c_str(), expected == got ? "PASS" : "FAIL", expected.c_str(), got.c_str());
        };
        const bool hadEmulation = Sensors::Accelerometer::getKeyboardEmulationEnabledEXT();
        try {
            if (!hadEmulation) Sensors::Accelerometer::setKeyboardEmulationEnabledEXT(true);
            Sensors::Accelerometer probe;
            const std::thread::id self = std::this_thread::get_id();
            bool injecting = false;
            std::string order;
            Vector3 current;
            double x = -9, y = -9, z = -9;
            System::DateTimeOffset currentTime, legacyTime;
            probe.CurrentValueChanged.Add([&](System::Object*, const auto& e) {
                if (!injecting || std::this_thread::get_id() != self) return;
                order += order.empty() ? "CurrentValueChanged" : ", CurrentValueChanged";
                current = e.getSensorReadingProperty().getAccelerationProperty();
                currentTime = e.getSensorReadingProperty().getTimestampProperty();
            });
            probe.ReadingChanged.Add([&](System::Object*, const Sensors::AccelerometerReadingEventArgs& e) {
                if (!injecting || std::this_thread::get_id() != self) return;
                order += order.empty() ? "ReadingChanged" : ", ReadingChanged";
                x = e.getXProperty();
                y = e.getYProperty();
                z = e.getZProperty();
                legacyTime = e.getTimestampProperty();
            });
            probe.Start();
            injecting = true;
            probe.InjectSyntheticSensorUpdate(0.0f, 9.80665f, -9.80665f);   // EMULATED: 1 g up, 1 g away
            injecting = false;
            probe.Stop();
            check("one EMULATED reading raises both events", "CurrentValueChanged, ReadingChanged", order);
            check("ReadingChanged X, Y, Z in g (9.80665 m/s^2 = 1 g)", "exactly 0 / 1 / -1",
                  x == 0.0 && y == 1.0 && z == -1.0 ? std::string("exactly 0 / 1 / -1") : Fmt(x) + " / " + Fmt(y) + " / " + Fmt(z));
            check("CurrentValueChanged Acceleration matches ReadingChanged", "same values and Timestamp",
                  current.X == static_cast<float>(x) && current.Y == static_cast<float>(y) && current.Z == static_cast<float>(z) && currentTime == legacyTime ? std::string("same values and Timestamp") : std::string("different"));
            last_ = "legacy ReadingChanged checked with an emulated reading";
        } catch (const std::exception& e) {
            Museum::Log("check", "%s legacy ReadingChanged: SKIP (the emulated accelerometer refused: %s)", R().key.c_str(), e.what());
            last_ = std::string("refused: ") + std::string(e.what()).substr(0, 60);
        }
        if (!hadEmulation) {
            try { Sensors::Accelerometer::setKeyboardEmulationEnabledEXT(false); } catch (const std::exception&) {}
        }
    }

    void Release()
    {
        try {
            if (accel_) { WatchAccelerometer(false); if (started_[0]) accel_->Stop(); }
            if (gyro_ && started_[1]) gyro_->Stop();
            if (compass_ && started_[2]) compass_->Stop();
            if (motion_ && started_[3]) motion_->Stop();
        } catch (const std::exception&) {}
        started_[0] = started_[1] = started_[2] = started_[3] = false;
        accel_.reset();
        gyro_.reset();
        compass_.reset();
        motion_.reset();
        if (Sensors::Accelerometer::getKeyboardEmulationEnabledEXT()) {
            try { Sensors::Accelerometer::setKeyboardEmulationEnabledEXT(false); } catch (const std::exception&) {}
        }
        if (S().mutableStats != nullptr) S().mutableStats->typingCapture = false;
    }

    std::unique_ptr<Sensors::Accelerometer> accel_;
    std::unique_ptr<Sensors::Gyroscope> gyro_;
    std::unique_ptr<Sensors::Compass> compass_;
    std::unique_ptr<Sensors::Motion> motion_;
    bool started_[4] = {false, false, false, false};
    std::size_t accelToken_ = 0;
    std::atomic<int> accelEvents_{0};
    std::string accelNote_ = "press Start", gyroNote_ = "press Start", compassNote_ = "press Start", motionNote_ = "press Start", vibrate_ = "not tried", last_ = "-";
};

}  // namespace

std::unique_ptr<Exhibit> MakePhoneSensorLab(const RoomDef& room, Services& services)
{
    return std::make_unique<PhoneSensorLab>(room, services);
}
