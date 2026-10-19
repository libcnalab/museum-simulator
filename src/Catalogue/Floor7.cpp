// Floor 7: audio and input.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec AudioWing()
{
    WingSpec w;
    w.name = "Audio Complex";
    w.tagline = "SoundEffect, instances, 3D audio, synthesis, generated sounds, music and the microphone";
    w.theme = "audio-hall";
    w.accent = {0.95f, 0.50f, 0.65f};
    w.spineSide = Wall::East;
    const auto au = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "audio-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "audio-hall";
        r.apis = std::move(apis);
        return r;
    };
    w.rooms = {
        au("au-basics", "SoundEffect.Play", "Volume, pitch and pan of fire-and-forget sounds, with overlapping playback", "basics", {"SoundEffect.Play", "SoundEffect", "ContentManager.Load<SoundEffect>"}),
        au("au-layers", "Instances and Layers", "Four looping SoundEffectInstances mixed live: volume, pitch, state", "layers", {"SoundEffectInstance", "SoundEffectInstance.IsLooped", "SoundEffectInstance.Volume", "SoundEffectInstance.Pitch", "SoundState"}),
        au("au-spatial", "3D Audio", "AudioEmitter and AudioListener with distance and Doppler settings", "spatial", {"SoundEffectInstance.Apply3D", "AudioEmitter", "AudioListener", "SoundEffect.DopplerScale", "SoundEffect.DistanceScale", "SoundEffect.SpeedOfSound"}),
        au("au-synth", "A Synthesiser", "DynamicSoundEffectInstance playing notes computed from waveform formulas", "synth", {"DynamicSoundEffectInstance", "DynamicSoundEffectInstance.SubmitBuffer", "DynamicSoundEffectInstance.BufferNeeded", "DynamicSoundEffectInstance.SubmitFloatBufferEXT", "AudioChannels", "DynamicSoundEffectInstance.PendingBufferCount", "DynamicSoundEffectInstance.GetSampleSizeInBytes", "SoundEffect.GetSampleDuration"}),
        au("au-generate", "Generated Sounds", "SoundEffect built from a PCM buffer: laser, explosion, bell, coin and wind", "generate", {"SoundEffect(buffer, sampleRate, channels)", "AudioChannels"}),
        au("au-master", "Master Volume and Settings", "SoundEffect.MasterVolume and the static 3D audio properties", "master", {"SoundEffect.MasterVolume", "SoundEffect.DistanceScale", "SoundEffect.DopplerScale", "SoundEffect.SpeedOfSound"}),
        au("au-music", "Song and MediaPlayer", "Streaming music with a queue, pause, repeat and volume", "music", {"Song", "Song.Duration", "Song.TrackNumber", "MediaPlayer.ActiveSongChanged", "MediaPlayer.MediaStateChanged", "MediaPlayer.GameHasControl", "Song.PlayCount", "Song.Rating", "Song.IsRated", "Song.IsProtected", "Song.Artist", "MediaPlayer.Play", "MediaPlayer.Volume", "SongCollection", "MediaQueue", "MediaPlayer.Queue", "MediaPlayer.MoveNext", "MediaPlayer.MovePrevious", "MediaPlayer.IsShuffled", "MediaPlayer.IsVisualizationEnabled", "MediaPlayer.GetVisualizationData", "VisualizationData", "MediaState"}),
        []{
            RoomSpec r;
            r.key = "au-video";
            r.title = "Video";
            r.subtitle = "Loose WebM playback or WmvImporter -> VideoProcessor -> XNB -> VideoReader: every decoded frame is a Texture2D";
            r.exhibit = "video-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.2f;
            r.theme = "audio-hall";
            r.apis = {"Video", "VideoPlayer", "VideoPlayer.Play", "VideoPlayer.Pause", "VideoPlayer.Resume", "VideoPlayer.Stop", "VideoPlayer.GetTexture", "VideoPlayer.IsDisposed", "VideoPlayer.IsLooped", "VideoPlayer.IsMuted", "VideoPlayer.Video", "VideoPlayer.Volume", "VideoPlayer.PlayPosition", "VideoPlayer.State",
                      "WmvImporter", "WmvImporter.Import", "VideoContent", "Processors::VideoProcessor", "Processors::VideoProcessor.Process", "Processors::VideoProcessor.VideoSoundtrackType", "VideoSoundtrackType", "ContentCompiler.Compile(VideoContent)", "ContentManager.Load<Video>"};
            return r;
        }(),
        au("au-mic", "The Microphone", "Capturing 16-bit PCM and a level meter; reports cleanly when there is no microphone", "microphone", {"Microphone", "Microphone.GetData", "Microphone.SampleRate", "Microphone.BufferDuration", "Microphone.GetSampleSizeInBytes", "Microphone.IsHeadset"}),
        au("au-xact", "XACT Cues and Banks", "AudioEngine, WaveBank and SoundBank loaded from files written by tools/gen_xact.py; Cue.Play, Pause, Stop and category volumes", "xact",
           {"AudioEngine", "AudioEngine.Update", "AudioEngine.GetCategory", "AudioCategory.SetVolume", "AudioCategory.Pause", "WaveBank", "SoundBank", "SoundBank.GetCue", "SoundBank.PlayCue",
            "Cue.Play", "Cue.Pause", "Cue.Resume", "Cue.Stop", "AudioStopOptions", "Cue.SetVariable", "Cue.GetVariable", "AudioEngine.SetGlobalVariable", "AudioEngine.GetGlobalVariable", "AudioEngine.RendererDetails", "RendererDetail.FriendlyName", "Cue.IsCreated", "Cue.IsPreparing", "Cue.IsStopping", "SoundBank.IsInUse", "WaveBank.IsInUse"}),
    };
    return w;
}


WingSpec InputWing()
{
    WingSpec w;
    w.name = "Input Complex";
    w.tagline = "Keyboard, mouse, game pads, text input, joysticks and the machine's other devices";
    w.theme = "control-room";
    w.accent = {0.95f, 0.50f, 0.65f};
    w.spineSide = Wall::West;
    const auto in = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin = ApiOrigin::Xna) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "input-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "control-room";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        in("in-keyboard", "The Keyboard", "Keyboard.GetState, IsKeyDown, GetPressedKeys and physical/layout-aware key names", "keyboard", {"Keyboard.GetState", "KeyboardState.IsKeyDown", "KeyboardState.GetPressedKeys", "Keyboard.GetKeyNameEXT", "Keyboard.GetModStateEXT", "Keyboard.GetKeyFromNameEXT", "Keyboard.GetScancodeNameEXT", "Keyboard.GetScancodeFromNameEXT", "Keyboard.GetKeyFromScancodeEXT"}),
        in("in-edges", "Pressed, Held, Released", "Finding key edges by comparing this frame's state with the last", "edges", {"KeyboardState", "Keys"}),
        in("in-mouse", "The Mouse", "Position, five buttons, both wheels, relative mode and cursor shapes", "mouse", {"Mouse.GetState", "Mouse.SetPosition", "Mouse.WindowHandle", "MouseState", "ButtonState", "Mouse.IsRelativeMouseModeEXT", "Mouse.SetCursor", "MouseCursor", "MouseCursor.FromTexture2D", "Mouse.SetCaptureEXT"}, ApiOrigin::Xna),
        in("in-pad", "The Game Pad", "Sticks, triggers, buttons, D-pad, dead zones, vibration and keyboard emulation", "pad", {"GamePad.GetState", "GamePad.GetCapabilities", "GamePadState", "GamePadCapabilities", "GamePadCapabilities.IsConnected", "GamePadCapabilities.HasAButton", "GamePadCapabilities.HasBButton", "GamePadCapabilities.HasXButton", "GamePadCapabilities.HasYButton", "GamePadCapabilities.HasBackButton", "GamePadCapabilities.HasStartButton", "GamePadCapabilities.HasBigButton", "GamePadCapabilities.HasDPadUpButton", "GamePadCapabilities.HasDPadDownButton", "GamePadCapabilities.HasDPadLeftButton", "GamePadCapabilities.HasDPadRightButton", "GamePadCapabilities.HasLeftShoulderButton", "GamePadCapabilities.HasRightShoulderButton", "GamePadCapabilities.HasLeftStickButton", "GamePadCapabilities.HasRightStickButton", "GamePadCapabilities.HasLeftXThumbStick", "GamePadCapabilities.HasLeftYThumbStick", "GamePadCapabilities.HasRightXThumbStick", "GamePadCapabilities.HasRightYThumbStick", "GamePadCapabilities.HasLeftTrigger", "GamePadCapabilities.HasRightTrigger", "GamePadCapabilities.HasLeftVibrationMotor", "GamePadCapabilities.HasRightVibrationMotor", "GamePadCapabilities.HasVoiceSupport", "GamePadDeadZone", "Buttons", "Buttons.BigButton", "Buttons.LeftThumbstickUp", "Buttons.RightThumbstickLeft", "GamePadState.IsButtonDown", "GamePadState.IsButtonUp", "GamePadState.Equals", "GamePad.SetVibration", "GamePad.KeyboardEmulationEnabledEXT", "GamePad.SetTriggerVibrationEXT", "GamePad.SetLightBarEXT", "GamePad.GetGUIDEXT", "GamePad.GetGyroEXT", "GamePad.GetPlayerIndexEXT", "GamePad.GetButtonLabelEXT", "GamePad.GetTouchpadFingerCountEXT"}),
        in("in-players", "Four Player Slots", "PlayerIndex, IsConnected, packet numbers and capabilities", "players", {"PlayerIndex", "GamePad.GetCapabilities", "GamePadCapabilities", "GamePad.GetNameEXT"}),
        in("in-text", "Text Input and Clipboard", "TextInputEXT characters, Backspace by polling, and copy/paste", "text", {"TextInputEXT.TextInput", "Clipboard.SetTextEXT", "Clipboard.GetTextEXT", "Clipboard.SetDataEXT", "Clipboard.GetMimeTypesEXT", "TextInputEXT.StartTextInputWithTypeEXT", "TextInputTypeEXT", "Clipboard.SetPrimarySelectionTextEXT", "Clipboard.GetPrimarySelectionTextEXT", "Clipboard.HasPrimarySelectionTextEXT"}, ApiOrigin::CnaExtension),
        in("in-joystick", "Raw Joysticks", "Axes, buttons and hats of any connected joystick", "joystick", {"Joysticks.GetJoysticksEXT", "Joysticks.GetCapabilitiesEXT", "Joysticks.GetStateEXT"}, ApiOrigin::CnaExtension),
        in("in-touch", "Touch and Gestures", "TouchPanel points and gestures: tap, hold, drags, flick and pinch, with simulated fingers", "touch", {"TouchPanel.GetState", "TouchPanel.ReadGesture", "TouchPanel.EnabledGestures", "GestureType", "GestureSample.Position2", "GestureSample.Delta2", "GestureSample.Timestamp", "TouchLocation.Id", "TouchLocation.State", "TouchLocation.TryGetPreviousLocation", "TouchPanel.MouseTouchEmulationEnabledEXT"}, ApiOrigin::Xna),
        in("in-devices", "Other Input Devices", "Mice, keyboards, touch, sensors and power as the machine reports them", "devices", {"InputDevices.GetMiceEXT", "TouchPanel.GetState", "TouchPanel.MouseTouchEmulationEnabledEXT", "Sensors.GetSensorsEXT", "Power.GetInfoEXT", "GamePad.GetConnectionStateEXT", "GamePad.GetPowerInfoEXT", "GamePad.GetTouchpadCountEXT", "GamePad.GetFirmwareVersionEXT", "GamePad.GetSerialEXT", "Mouse.GetGlobalPositionEXT", "Haptics.GetHapticsEXT", "Haptics.IsMouseHapticEXT", "Haptics.IsJoystickHapticEXT"}, ApiOrigin::CnaExtension),
        [] {
            RoomSpec r;
            r.key = "in-phone";
            r.title = "Phone Sensors";
            r.subtitle = "Microsoft.Devices: Accelerometer, Gyroscope, Compass, Motion and VibrateController, with arrow keys that can tilt the accelerometer";
            r.exhibit = "phonesensor-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "control-room";
            r.apis = {"Accelerometer", "Accelerometer.IsSupported", "Accelerometer.Start", "Accelerometer.Stop", "Accelerometer.State", "SensorBase.CurrentValue", "SensorBase.CurrentValueChanged", "Accelerometer.KeyboardEmulationEnabledEXT",
                      "AccelerometerReading.Acceleration", "Gyroscope", "Gyroscope.IsSupported", "GyroscopeReading.RotationRate", "Compass", "Compass.IsSupported", "CompassReading.MagneticHeading", "Motion", "Motion.IsSupported",
                      "MotionReading.Attitude", "MotionReading.Gravity", "AttitudeReading", "SensorState", "SensorReadingEventArgs", "VibrateController", "VibrateController.Default", "VibrateController.Start", "VibrateController.IsSupported",
                      "Microsoft.Devices.Environment.DeviceType", "DeviceType"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
    };
    w.rooms[3].apis.push_back("GamePadCapabilities.GamePadType");
    return w;
}

// Added after the audio and input wings so that no existing room moves: devices and services outside the game's
// own window. Buttons that open something outside the museum are manual (see DeviceLab.cpp).
WingSpec DevicesWing()
{
    WingSpec w;
    w.name = "Devices and the System";
    w.tagline = "Haptics, native dialogs, the tray, the camera, window events and which platform this is";
    w.theme = "control-room";
    w.accent = {0.60f, 0.75f, 0.95f};
    w.spineSide = Wall::East;
    const auto dv = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "device-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "control-room";
        r.apis = std::move(apis);
        r.origin = ApiOrigin::CnaExtension;
        return r;
    };
    w.rooms = {
        dv("dev-haptics", "Haptic Devices", "List, open, rumble, run a constant effect, gain and pause; a refused open when there is no device", "haptics",
           {"Haptics.GetHapticsEXT", "Haptics.OpenEXT", "Haptics.IsMouseHapticEXT", "HapticInfoEXT", "HapticDevice", "HapticDevice.IsOpenEXT", "HapticDevice.GetNameEXT", "HapticDevice.GetCapabilitiesEXT", "HapticCapabilitiesEXT",
            "HapticDevice.InitRumbleEXT", "HapticDevice.PlayRumbleEXT", "HapticDevice.StopRumbleEXT", "HapticEffectEXT", "HapticEffectTypeEXT", "HapticDevice.IsEffectSupportedEXT", "HapticDevice.CreateEffectEXT",
            "HapticDevice.UpdateEffectEXT", "HapticDevice.RunEffectEXT", "HapticDevice.GetEffectStatusEXT", "HapticDevice.StopEffectEXT", "HapticDevice.DestroyEffectEXT", "HapticDevice.SetGainEXT",
            "HapticDevice.SetAutocenterEXT", "HapticDevice.PauseEXT", "HapticDevice.ResumeEXT", "HapticDevice.StopAllEffectsEXT", "HapticDevice.Dispose"}),
        dv("dev-dialogs", "Native Dialogs", "FileDialog open, save and folder pickers with a callback, MessageBox with buttons", "dialogs",
           {"CNA::Devices::FileDialog", "FileDialog.IsSupported", "FileDialog.ShowOpenFile", "FileDialog.ShowSaveFile", "FileDialog.ShowOpenFolder", "CNA::Devices::FileDialogFilter", "FileDialog.ResultCallback",
            "CNA::Devices::MessageBox", "MessageBox.IsSupported", "MessageBox.ShowSimple", "MessageBox.Show", "MessageBoxType"}),
        dv("dev-tray", "Tray Icon and URLs", "A tray icon with a checkable menu entry, and a local page handed to the desktop", "tray",
           {"CNA::Devices::SystemTray", "SystemTray.IsSupported", "SystemTray.AddEntry", "SystemTray.Tooltip", "SystemTray.SetEntryLabel", "SystemTray.SetEntryChecked", "SystemTray.GetEntryChecked",
            "SystemTray.SetEntryEnabled", "SystemTray.GetEntryEnabled", "CNA::Devices::UrlLauncher", "UrlLauncher.Open"}),
        dv("dev-camera", "The Camera", "Available cameras, opening the default one and copying its frames into a Texture2D", "camera",
           {"CNA::Devices::Camera", "Camera.IsSupported", "Camera.AvailableCameras", "CameraDeviceInfo", "CameraPosition", "Camera.State", "CameraState", "Camera.FrameWidth", "Camera.FrameHeight", "Camera.TryAcquireFrame"}),
        dv("dev-window", "Window Events and Device Loss", "TextDropEXT, minimise and restore, DeviceLost/Reset events and an emulated content loss", "window",
           {"GameWindow.TextDropEXT", "TextDropEventArgsEXT", "TextDropEventArgsEXT.Text", "GameWindow.MinimizeEXT", "GameWindow.RestoreEXT", "GraphicsDevice.DeviceLost", "GraphicsDevice.DeviceResetting",
            "GraphicsDevice.DeviceReset", "RenderTarget2D.ContentLost", "RenderTarget2D.IsContentLost", "RenderTarget2D.NotifyContentLostEXT"}),
        dv("dev-identity", "Platform and Renderer Family", "TargetPlatform, DesktopOS, the renderer category of the build and of the running device", "identity",
           {"CNA::getCurrentPlatform", "CNA::getCurrentPlatformName", "CNA::isMobilePlatform", "CNA::isApplePlatform", "CNA::TargetPlatform", "CNA::getCurrentDesktopOS", "CNA::DesktopOS",
            "CNA::getGraphicsRendererCategory", "CNA::getCurrentGraphicsRendererCategory", "CNA::GraphicsRendererCategory", "GraphicsDevice.GetGraphicsRendererType", "CNA::getCurrentGraphicsRendererType",
            "CNA::getGraphicsRendererName", "GraphicsDevice.SetUnsupported3DGraphicsCallBehavior", "GraphicsDevice.GetUnsupported3DGraphicsCallBehavior", "CNA::Unsupported3DGraphicsCallBehavior"}),
        [] {
            RoomSpec r;
            r.key = "dev-notify";
            r.title = "Push Notifications";
            r.subtitle = "An HttpNotificationChannel on loopback: open, find, raw and toast messages, dispatch, close and late sends";
            r.exhibit = "notification-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.2f;
            r.theme = "control-room";
            r.apis = {"HttpNotificationChannel", "HttpNotificationChannel.Find", "HttpNotificationChannel.Open", "HttpNotificationChannel.Close", "HttpNotificationChannel.ChannelUri",
                      "HttpNotificationChannel.ChannelUriUpdated", "NotificationChannelUriEventArgs", "NotificationChannelUriEventArgs.ChannelUri", "HttpNotificationChannel.HttpNotificationReceived",
                      "HttpNotificationEventArgs", "HttpNotificationEventArgs.Body", "HttpNotificationEventArgs.BodyAsString", "HttpNotificationChannel.DispatchPendingNotificationsEXT",
                      "HttpNotificationChannel.BindToShellToast", "HttpNotificationChannel.IsShellToastBound", "RawPushNotificationMessage", "RawPushNotificationMessage.RawData",
                      "RawPushNotificationMessage.SendAsync", "ToastPushNotificationMessage", "ToastPushNotificationMessage.Title", "ToastPushNotificationMessage.SubTitle",
                      "ToastPushNotificationMessage.SendAsync", "MessageSendPriority"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
        [] {
            RoomSpec r;
            r.key = "dev-medialib";
            r.title = "The Media Library";
            r.subtitle = "Songs, albums, artists, genres, playlists and picture albums of the Music and Pictures folders, opened on request";
            r.exhibit = "medialibrary-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.2f;
            r.theme = "control-room";
            r.apis = {"MediaLibrary", "MediaLibrary.Songs", "MediaLibrary.Albums", "MediaLibrary.Artists", "MediaLibrary.Genres", "MediaLibrary.Playlists", "MediaLibrary.Pictures",
                      "MediaLibrary.RootPictureAlbum", "MediaLibrary.SavedPictures", "MediaLibrary.MediaSource", "MediaLibrary.SavePicture", "MediaLibrary.Dispose", "MediaLibrary.IsDisposed",
                      "MediaSource.Name", "SongCollection", "Song.Name", "Song.Album", "Song.Artist", "Song.Genre", "Song.Duration", "Song.TrackNumber", "AlbumCollection", "Album.Name",
                      "Album.Artist", "Album.Songs", "Album.Duration", "Album.HasArt", "ArtistCollection", "Artist.Name", "Artist.Albums", "Artist.Songs", "GenreCollection", "Genre.Name",
                      "Genre.Songs", "PlaylistCollection", "Playlist.Name", "Playlist.Songs", "Playlist.Duration", "PictureCollection", "PictureAlbum", "PictureAlbum.Name", "PictureAlbum.Albums",
                      "PictureAlbum.Pictures", "PictureAlbum.Parent", "PictureAlbumCollection", "Picture.Name", "Picture.Width", "Picture.Height", "Picture.GetThumbnail", "Texture2D.FromStream"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor7()
{
    FloorSpec f;
    f.index = 7;
    f.name = "Audio and Input";
    f.tagline = "Sound, music, keyboards, mice and game pads";
    f.theme = "concourse";
    f.accent = {0.95f, 0.50f, 0.65f};
    f.wings.push_back(AudioWing());
    f.wings.push_back(InputWing());
    f.wings.push_back(DevicesWing());
    return f;
}
