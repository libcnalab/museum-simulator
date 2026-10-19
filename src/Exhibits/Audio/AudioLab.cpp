// Audio laboratory: SoundEffect, SoundEffectInstance, 3D audio, DynamicSoundEffectInstance, generated sounds,
// the static SoundEffect settings, Song/MediaPlayer and the Microphone. A live screen in every room draws what is
// being asked of the audio system (CNA reports no output levels, so the meters show the requested values).
// Everything degrades silently when audio is disabled or no device exists: the controls still work.
#include <algorithm>
#include <cmath>
#include <complex>
#include <exception>
#include <random>
#include <string>

#include "Audio/AudioSystem.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Audio/AudioCategory.hpp"
#include "Microsoft/Xna/Framework/Audio/AudioChannels.hpp"
#include "Microsoft/Xna/Framework/Audio/AudioEngine.hpp"
#include "Microsoft/Xna/Framework/Audio/AudioEmitter.hpp"
#include "Microsoft/Xna/Framework/Audio/AudioListener.hpp"
#include "Microsoft/Xna/Framework/Audio/AudioStopOptions.hpp"
#include "Microsoft/Xna/Framework/Audio/Cue.hpp"
#include "Microsoft/Xna/Framework/Audio/DynamicSoundEffectInstance.hpp"
#include "Microsoft/Xna/Framework/Audio/Microphone.hpp"
#include "Microsoft/Xna/Framework/Audio/NoAudioHardwareException.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundBank.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp"
#include "Microsoft/Xna/Framework/Audio/WaveBank.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Media/MediaPlayer.hpp"
#include "Microsoft/Xna/Framework/Media/MediaQueue.hpp"
#include "Microsoft/Xna/Framework/Media/SongCollection.hpp"
#include "Microsoft/Xna/Framework/Media/VisualizationData.hpp"
#include "Microsoft/Xna/Framework/Media/MediaState.hpp"
#include "Microsoft/Xna/Framework/Media/Song.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Audio;
using namespace Microsoft::Xna::Framework::Media;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Basics, Layers, Spatial, Synth, Generate, Master, Music, Microphone, Xact };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"layers", Mode::Layers}, {"spatial", Mode::Spatial}, {"synth", Mode::Synth}, {"generate", Mode::Generate},
                                                         {"master", Mode::Master}, {"music", Mode::Music}, {"microphone", Mode::Microphone}, {"xact", Mode::Xact}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Basics;
}

using Bytes = std::vector<SharpRuntime::bytecs>;

Bytes ToPcm16(const std::vector<float>& samples)
{
    Bytes out(samples.size() * 2);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const std::int16_t v = static_cast<std::int16_t>(std::clamp(samples[i], -1.0f, 1.0f) * 32000.0f);
        out[i * 2] = static_cast<SharpRuntime::bytecs>(v & 0xFF);
        out[i * 2 + 1] = static_cast<SharpRuntime::bytecs>((v >> 8) & 0xFF);
    }
    return out;
}

class AudioLab final : public ScreenExhibit {
public:
    AudioLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))), rng_(99) {}

    ~AudioLab() override
    {
        RestoreGlobals();
        if (songToken_ != 0) MediaPlayer::ActiveSongChanged.Remove(songToken_);
        if (stateToken_ != 0) MediaPlayer::MediaStateChanged.Remove(stateToken_);
        if (synth_ && neededToken_ != 0) synth_->BufferNeeded.Remove(neededToken_);
    }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        const bool on = S().audio != nullptr && S().audio->Enabled();
        switch (mode_) {
            case Mode::Basics:
                p.AddSlider("Volume", 0.0f, 1.0f, volume_, [this](float v) { volume_ = v; });
                p.AddSlider("Pitch (-1 .. 1 octave)", -1.0f, 1.0f, pitch_, [this](float v) { pitch_ = v; });
                p.AddSlider("Pan (left .. right)", -1.0f, 1.0f, pan_, [this](float v) { pan_ = v; });
                for (const char* n : {"click", "chirp", "drums", "spark"}) p.AddButton(std::string("Play ") + n, [this, n] { PlayOnce(n); });
                p.AddReadout("Last sound", [this] { return last_; });
                p.AddReadout("Audio output", [on] { return std::string(on ? "available" : "disabled or no device"); });
                break;
            case Mode::Layers:
                for (int i = 0; i < 4; ++i) p.AddSlider(std::string(kLayers[i]) + " volume", 0.0f, 1.0f, layerVolume_[static_cast<std::size_t>(i)], [this, i](float v) { SetLayer(i, v); });
                p.AddToggle("Pitch bend all (slow motion)", false, [this](bool v) { bend_ = v; ApplyLayers(); });
                p.AddReadout("Instances playing", [this] { return std::to_string(Playing()) + " of 4"; });
                break;
            case Mode::Spatial:
                p.AddSlider("Orbit radius", 1.5f, 5.0f, radius_, [this](float v) { radius_ = v; });
                p.AddSlider("Orbit speed", 0.0f, 3.0f, speed_, [this](float v) { speed_ = v; });
                p.AddSlider("DistanceScale", 0.2f, 4.0f, distanceScale_, [this](float v) { distanceScale_ = v; SoundEffect::setDistanceScaleProperty(v); }, "%.2f");
                p.AddSlider("DopplerScale", 0.0f, 4.0f, dopplerScale_, [this](float v) { dopplerScale_ = v; SoundEffect::setDopplerScaleProperty(v); }, "%.2f");
                p.AddSlider("SpeedOfSound (m/s)", 20.0f, 343.0f, soundSpeed_, [this](float v) { soundSpeed_ = v; SoundEffect::setSpeedOfSoundProperty(v); }, "%.0f");
                p.AddReadout("Distance to emitter", [this] { return Fmt1(distance_) + " m"; });
                break;
            case Mode::Synth:
                p.AddChoice("Waveform", {"Sine", "Square", "Sawtooth", "Triangle", "Noise"}, 0, [this](int v) { wave_ = v; });
                p.AddSlider("Attack (ms)", 1.0f, 300.0f, attack_, [this](float v) { attack_ = v; }, "%.0f");
                p.AddSlider("Release (ms)", 20.0f, 1200.0f, release_, [this](float v) { release_ = v; }, "%.0f");
                p.AddToggle("Submit float32 samples (SubmitFloatBufferEXT)", false, [this](bool v) { SetFloatMode(v); });
                p.AddSlider("Octave", 3.0f, 6.0f, octave_, [this](float v) { octave_ = v; }, "%.0f", 1.0f);
                for (int i = 0; i < 8; ++i) p.AddButton(std::string("Note ") + kNotes[i], [this, i] { PlayNote(i); });
                p.AddReadout("BufferNeeded events", [this] { return std::to_string(bufferNeeded_); });
                p.AddReadout("PendingBufferCount", [this] { return synth_ ? std::to_string(synth_->getPendingBufferCountProperty()) : std::string("-"); });
                p.AddReadout("Sample size helpers", [this] {
                    if (!synth_) return std::string("-");
                    const int bytes = synth_->GetSampleSizeInBytes(System::TimeSpan::FromMilliseconds(100.0));
                    return std::to_string(bytes) + " bytes = 100 ms (" + Fmt1(static_cast<float>(SoundEffect::GetSampleDuration(bytes, 22050, AudioChannels::Mono).getTotalMillisecondsProperty())) + " ms back)";
                });
                break;
            case Mode::Generate:
                p.AddChoice("Sound", {"Laser", "Explosion", "Bell", "Coin", "Wind"}, 0, [this](int v) { gen_ = v; Generate(); });
                p.AddButton("Play it", [this] { PlayGenerated(); });
                p.AddReadout("Samples", [this] { return std::to_string(samples_.size()) + " at 22050 Hz"; });
                p.AddReadout("Duration", [this] { return Fmt1(static_cast<float>(samples_.size()) / 22050.0f) + " s"; });
                break;
            case Mode::Master:
                p.AddSlider("SoundEffect.MasterVolume", 0.0f, 1.0f, master_, [this](float v) { master_ = v; if (S().audio) S().audio->SetMasterVolume(v); }, "%.2f");
                p.AddButton("Play a test chirp", [this] { PlayOnce("chirp"); });
                p.AddReadout("MasterVolume now", [] { return Fmt1(SoundEffect::getMasterVolumeProperty()); });
                p.AddReadout("DistanceScale", [] { return Fmt1(SoundEffect::getDistanceScaleProperty()); });
                p.AddReadout("DopplerScale", [] { return Fmt1(SoundEffect::getDopplerScaleProperty()); });
                p.AddReadout("SpeedOfSound", [] { return Fmt1(SoundEffect::getSpeedOfSoundProperty()) + " m/s"; });
                break;
            case Mode::Music:
                p.AddButton("Play song 1: arpeggio", [this] { PlaySong(0); });
                p.AddButton("Play song 2: ambience", [this] { PlaySong(1); });
                p.AddButton("Play both as a queue (SongCollection)", [this] { PlayQueue(); });
                p.AddButton("MediaPlayer.MoveNext", [] { try { MediaPlayer::MoveNext(); } catch (const std::exception&) {} });
                p.AddButton("MediaPlayer.MovePrevious", [] { try { MediaPlayer::MovePrevious(); } catch (const std::exception&) {} });
                p.AddToggle("IsShuffled", false, [](bool v) { try { MediaPlayer::setIsShuffledProperty(v); } catch (const std::exception&) {} });
                p.AddToggle("IsVisualizationEnabled", true, [this](bool v) { visualize_ = v; try { MediaPlayer::setIsVisualizationEnabledProperty(v); } catch (const std::exception&) {} });
                p.AddButton("Pause / resume", [this] { TogglePause(); });
                p.AddButton("Stop", [] { try { MediaPlayer::Stop(); } catch (const std::exception&) {} });
                p.AddSlider("MediaPlayer.Volume", 0.0f, 1.0f, mediaVolume_, [this](float v) { mediaVolume_ = v; try { MediaPlayer::setVolumeProperty(v); } catch (const std::exception&) {} });
                p.AddToggle("IsRepeating", false, [](bool v) { try { MediaPlayer::setIsRepeatingProperty(v); } catch (const std::exception&) {} });
                p.AddReadout("State", [] { try { const MediaState s = MediaPlayer::getStateProperty(); return std::string(s == MediaState::Playing ? "Playing" : (s == MediaState::Paused ? "Paused" : "Stopped")); } catch (const std::exception& e) { return std::string("unavailable: ") + std::string(e.what()).substr(0, 40); } });
                p.AddReadout("Position", [] { try { return Fmt1(static_cast<float>(MediaPlayer::getPlayPositionProperty().getTotalSecondsProperty())) + " s"; } catch (const std::exception&) { return std::string("-"); } });
                p.AddReadout("Song of the last button", [this] { return SongInfo(); });
                p.AddReadout("ActiveSongChanged / MediaStateChanged events", [this] { return std::to_string(songEvents_) + " / " + std::to_string(stateEvents_); });
                p.AddReadout("MediaPlayer.GameHasControl", [] { try { return std::string(MediaPlayer::getGameHasControlProperty() ? "true" : "false"); } catch (const std::exception&) { return std::string("-"); } });
                break;
            case Mode::Xact:
                p.AddChoice("Cue", {"Tone", "Click", "Arpeggio", "Drums", "Chirp"}, 0, [this](int v) { xCueIndex_ = v; });
                p.AddButton("Play: GetCue then Play", [this] { XactPlay(); });
                p.AddButton("Pause", [this] { if (xCue_ != nullptr) try { xCue_->Pause(); } catch (const std::exception& e) { ReportAudioError("Cue.Pause", e); } });
                p.AddButton("Resume", [this] { if (xCue_ != nullptr) try { xCue_->Resume(); } catch (const std::exception& e) { ReportAudioError("Cue.Resume", e); } });
                p.AddButton("Stop (as authored)", [this] { if (xCue_ != nullptr) try { xCue_->Stop(AudioStopOptions::AsAuthored); } catch (const std::exception& e) { ReportAudioError("Cue.Stop(AsAuthored)", e); } });
                p.AddButton("Stop (immediate)", [this] { if (xCue_ != nullptr) try { xCue_->Stop(AudioStopOptions::Immediate); } catch (const std::exception& e) { ReportAudioError("Cue.Stop(Immediate)", e); } });
                p.AddButton("PlayCue: fire and forget", [this] { XactFire(); });
                for (int i = 0; i < 3; ++i) p.AddSlider(std::string(kXactCategories[i]) + " category volume", 0.0f, 1.0f, catVolume_[static_cast<std::size_t>(i)], [this, i](float v) { XactCategoryVolume(i, v); });
                p.AddToggle("Pause the Music category", false, [this](bool v) { XactPauseMusic(v); });
                // Two variables of museum.xgs, each bound to every sound's volume by an RPC curve (0 to -60 dB over the variable's range).
                p.AddSlider("Cue variable \"Distance\" (Cue.SetVariable, 0..100)", 0.0f, 100.0f, 0.0f, [this](float v) { XactDistance(v); }, "%.0f", 1.0f);
                p.AddSlider("Global variable \"MasterTrim\" (AudioEngine.SetGlobalVariable)", 0.0f, 1.0f, 1.0f, [this](float v) { XactTrim(v); }, "%.2f", 0.05f);
                p.AddReadout("Variables read back", [this] { return xVariables_; });
                p.AddReadout("Cue", [this] { return XactState(); });
                p.AddReadout("Cue flags", [this] { return XactFlags(); });
                p.AddReadout("Banks", [this] { return xStatus_; });
                p.AddReadout("Banks in use / engine", [this] { return XactEngineInfo(); });
                break;
            case Mode::Microphone:
                p.AddButton("Start / stop capture", [this] { ToggleMic(); });
                p.AddReadout("Microphones found", [this] { return micCount_; });
                p.AddReadout("Default microphone", [this] { return MicSummary(); });
                p.AddReadout("State", [this] { return micState_; });
                p.AddReadout("Input level", [this] { return Fmt1(micLevel_ * 100.0f) + " %"; });
                break;
        }
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
        content_ = std::make_unique<Content::ContentManager>(&S().game->getServicesProperty(), S().contentRoot);
        if (S().audio == nullptr || !S().audio->Enabled()) return;
        try {
            if (mode_ == Mode::Layers) {
                for (int i = 0; i < 4; ++i) {
                    effects_[static_cast<std::size_t>(i)] = std::make_unique<SoundEffect>(content_->Load<SoundEffect>(std::string("Audio/") + kLayerFiles[i]));
                    layers_[static_cast<std::size_t>(i)] = std::make_unique<SoundEffectInstance>(effects_[static_cast<std::size_t>(i)]->CreateInstance());
                    layers_[static_cast<std::size_t>(i)]->setIsLoopedProperty(true);
                    layers_[static_cast<std::size_t>(i)]->setVolumeProperty(0.0f);
                    layers_[static_cast<std::size_t>(i)]->Play();
                }
            } else if (mode_ == Mode::Spatial) {
                effects_[0] = std::make_unique<SoundEffect>(content_->Load<SoundEffect>("Audio/tone"));
                spatial_ = std::make_unique<SoundEffectInstance>(effects_[0]->CreateInstance());
                spatial_->setIsLoopedProperty(true);
                spatial_->Play();
            } else if (mode_ == Mode::Music) {
                songToken_ = MediaPlayer::ActiveSongChanged.Add([this](System::Object*, const System::EventArgs&) { ++songEvents_; Museum::Log("audio", "MediaPlayer.ActiveSongChanged (%d)", songEvents_); });
                stateToken_ = MediaPlayer::MediaStateChanged.Add([this](System::Object*, const System::EventArgs&) { ++stateEvents_; Museum::Log("audio", "MediaPlayer.MediaStateChanged (%d)", stateEvents_); });
            } else if (mode_ == Mode::Synth) {
                synth_ = std::make_unique<DynamicSoundEffectInstance>(22050, AudioChannels::Mono);
                neededToken_ = synth_->BufferNeeded.Add([this](System::Object*, const System::EventArgs&) { ++bufferNeeded_; if (bufferNeeded_ == 1) Museum::Log("audio", "DynamicSoundEffectInstance.BufferNeeded raised"); });
            } else if (mode_ == Mode::Generate) {
                Generate();
            } else if (mode_ == Mode::Xact) {
                LoadXact();
            }
        } catch (const std::exception& e) {
            Museum::Log("audio", "%s: %s", Title().c_str(), e.what());
            ReportAudioError("loading the room's sounds", e);
        }
        if (mode_ == Mode::Generate && samples_.empty()) Generate();
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        listenerPos_ = ctx.playerPosition;
        if (mode_ == Mode::Spatial && spatial_) {
            const float a = time_ * speed_;
            const Vector3 centre = R().ToWorld(0.0f, 1.2f, 0.0f);
            const Vector3 pos = centre + Off(std::cos(a) * radius_, 0, std::sin(a) * radius_);
            const Vector3 vel = Vector3(-std::sin(a), 0, std::cos(a)) * (speed_ * radius_);
            emitterPos_ = pos;
            AudioEmitter emitter;
            emitter.setPositionProperty(pos);
            emitter.setVelocityProperty(vel);
            AudioListener listener;
            listener.setPositionProperty(ctx.playerPosition);
            listener.setForwardProperty(Vector3::Normalize(ctx.playerForward + Vector3(1e-5f, 0, 0)));
            try { spatial_->Apply3D(listener, emitter); } catch (const std::exception& e) {
                static bool reported = false;   // per-frame path: report the first failure only
                if (!reported) { reported = true; ReportAudioError("Apply3D", e); }
            }
            distance_ = Vector3::Distance(ctx.playerPosition, pos);
        }
        if (mode_ == Mode::Microphone && mic_ != nullptr && micRunning_) PollMic();
        if (mode_ == Mode::Xact && xEngine_) {
            try { xEngine_->Update(); } catch (const std::exception& e) {
                static bool reported = false;   // per-frame path: report the first failure only
                if (!reported) { reported = true; ReportAudioError("AudioEngine.Update", e); }
            }
        }
        for (float& f : flash_) f = std::max(0.0f, f - ctx.dt * 2.0f);
    }

    void OnLeave() override { RestoreGlobals(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        const SamplerState& smp = SamplerState::LinearClamp;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &smp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](int x, int y, int w, int h, Color c) { sb.Draw(white, Rectangle(x, y, w, h), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.8f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(Title(), 24, 14, Kit::kGold, 1.0f);
        switch (mode_) {
            case Mode::Basics:
                for (int i = 0; i < 4; ++i) {
                    rect(60, 90 + i * 90, static_cast<int>(840.0f * (0.15f + flash_[static_cast<std::size_t>(i)] * volume_)), 56, Color(90 + 40 * i, 170, 255 - 40 * i, 255));
                    text(kBasicNames[i], 70, 104 + static_cast<float>(i) * 90.0f, Color::White, 0.9f);
                }
                text("volume " + Fmt1(volume_) + "   pitch " + Fmt1(pitch_) + "   pan " + Fmt1(pan_), 60, 470, Kit::kText, 0.8f);
                break;
            case Mode::Layers:
                for (int i = 0; i < 4; ++i) {
                    rect(80 + i * 210, 440, 160, 4, Color(90, 100, 130, 255));
                    const int h = static_cast<int>(330.0f * layerVolume_[static_cast<std::size_t>(i)]);
                    rect(110 + i * 210, 440 - h, 100, h, Color(110, 190 - 30 * i, 255 - 40 * i, 255));
                    text(kLayers[i], 90.0f + static_cast<float>(i) * 210.0f, 456, Color::White, 0.8f);
                }
                break;
            case Mode::Spatial: {
                const float scale = 55.0f, cx = 480.0f, cy = 290.0f;
                const Vector3 c = R().ToWorld(0, 0, 0);
                rect(static_cast<int>(cx) - 3, static_cast<int>(cy) - 3, 6, 6, Color(150, 150, 170, 255));
                const auto px = [&](const Vector3& p) { return Vector2(cx + (p.X - c.X) * scale, cy + (p.Z - c.Z) * scale); };
                const Vector2 e = px(emitterPos_), l = px(listenerPos_);
                rect(static_cast<int>(e.X) - 10, static_cast<int>(e.Y) - 10, 20, 20, Color(255, 180, 80, 255));
                rect(static_cast<int>(l.X) - 7, static_cast<int>(l.Y) - 7, 14, 14, Color(90, 220, 140, 255));
                text("emitter (orbiting)", e.X + 14, e.Y - 10, Color(255, 200, 120, 255), 0.7f);
                text("you (the AudioListener)", l.X + 12, l.Y - 8, Color(150, 255, 180, 255), 0.7f);
                text("distance " + Fmt1(distance_) + " m", 24, 480, Kit::kText, 0.9f);
                break;
            }
            case Mode::Synth:
            case Mode::Generate: DrawScope(sb); break;
            case Mode::Master: {
                const float v = SoundEffect::getMasterVolumeProperty();
                rect(60, 200, static_cast<int>(840.0f * v), 80, Color(110, 190, 255, 255));
                rect(60, 200, 840, 3, Color(150, 160, 190, 255));
                text("MasterVolume " + Fmt1(v), 60, 300, Kit::kText, 1.0f);
                text("it scales every SoundEffect and SoundEffectInstance at once", 60, 360, Kit::kDim, 0.75f);
                break;
            }
            case Mode::Music: DrawMusic(sb); break;
            case Mode::Xact: {
                static const char* cues[5] = {"Tone", "Click", "Arpeggio", "Drums", "Chirp"};
                static const int category[5] = {0, 0, 1, 1, 2};
                for (int c = 0; c < 3; ++c) {
                    const int x = 40 + c * 300;
                    rect(x, 80, 280, 380, Color(24, 30, 46, 255));
                    rect(x, 80, 280, 6, c == 0 ? Color(110, 170, 255, 255) : (c == 1 ? Color(255, 190, 90, 255) : Color(130, 235, 150, 255)));
                    text(std::string(kXactCategories[c]) + "  (volume " + Fmt1(catVolume_[static_cast<std::size_t>(c)]) + ")", static_cast<float>(x) + 14.0f, 96.0f, Kit::kGold, 0.8f);
                    int row = 0;
                    for (int i = 0; i < 5; ++i) {
                        if (category[i] != c) continue;
                        const bool selected = i == xCueIndex_;
                        const bool live = selected && xCue_ != nullptr && xActive_ && XactPlaying();
                        rect(x + 14, 150 + row * 70, 252, 54, live ? Color(80, 200, 120, 255) : (selected ? Color(70, 86, 130, 255) : Color(40, 48, 72, 255)));
                        text(cues[i], static_cast<float>(x) + 28.0f, 164.0f + static_cast<float>(row) * 70.0f, live ? Color(10, 30, 16, 255) : Kit::kText, 0.8f);
                        ++row;
                    }
                }
                text(xStatus_, 40, 478, Kit::kDim, 0.65f);
                break;
            }
            case Mode::Microphone: {
                rect(60, 200, static_cast<int>(840.0f * std::min(1.0f, micLevel_ * 4.0f)), 80, Color(110, 255, 160, 255));
                text(micState_, 60, 310, Kit::kText, 0.9f);
                break;
            }
        }
        sb.End();
    }

private:
    static constexpr const char* kLayers[4] = {"Ambience", "Drums", "Arpeggio", "Tone"};
    static constexpr const char* kLayerFiles[4] = {"ambience", "drums", "arpeggio", "tone"};
    static constexpr const char* kBasicNames[4] = {"click", "chirp", "drums", "spark"};
    static constexpr const char* kNotes[8] = {"C", "D", "E", "F", "G", "A", "B", "C'"};

    static std::string Fmt1(float v)
    {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.2f", static_cast<double>(v));
        return buf;
    }
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"SoundEffect.Play: volume, pitch, pan", "SoundEffectInstance layers", "3D audio: emitter and listener", "A synthesiser with DynamicSoundEffectInstance", "Sounds generated in code", "Master volume and 3D settings", "Song and MediaPlayer", "The microphone", "XACT: cues, banks and categories"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Basics: return {"SoundEffect.Play(volume, pitch, pan) is fire-and-forget: volume 0..1, pitch -1..1 octaves (half speed to", "double), pan -1 (left) to 1 (right). Several calls overlap freely. The bars show the requested loudness."};
            case Mode::Layers: return {"A SoundEffectInstance is a playing copy of a sound you can control: loop it, set its volume and pitch,", "pause it. Four loops play together here; the sliders are four volumes on a mixing desk."};
            case Mode::Spatial: return {"Apply3D(listener, emitter) pans and attenuates an instance from the positions of an AudioEmitter and an", "AudioListener (you). DistanceScale, DopplerScale and SpeedOfSound are global; the emitter's velocity drives the Doppler shift."};
            case Mode::Synth: return {"DynamicSoundEffectInstance plays PCM samples you submit: here each note is computed from a waveform", "formula with an attack and release envelope and submitted when you press a button. The scope shows the samples."};
            case Mode::Generate: return {"A SoundEffect can be built from a buffer of PCM bytes instead of a file: here five sound effects are", "synthesised in code, drawn as a waveform and spectrum, then played. No asset files are involved."};
            case Mode::Master: return {"SoundEffect.MasterVolume scales everything the audio engine plays. The static DistanceScale, DopplerScale and", "SpeedOfSound settings shape 3D audio globally; this museum restores them when you leave a room that changes them."};
            case Mode::Music: return {"Song and MediaPlayer play music as a stream, with a queue, pause, repeat and volume of their own. The", "songs are the museum's generated WAV files loaded through the ContentManager."};
            case Mode::Xact: return {"XACT is XNA's audio authoring route: a settings file (categories, variables and RPC curves), a wave bank (the sounds) and a sound bank (named cues).", "AudioEngine loads them, GetCue gives a Cue you can Play, Pause and Stop; categories mix groups. Two variables of the file (Distance per cue, MasterTrim for the whole engine) drive every sound's volume through RPC curves. Written by tools/gen_xact.py."};
            default: return {"Microphone captures 16-bit mono PCM. If the machine has no microphone CNA reports it (the Microphones found", "readout) and nothing is recorded. With one, the level meter shows the signal strength."};
        }
    }

    // --- one-shots ---------------------------------------------------------------------------------
    void PlayOnce(const std::string& name)
    {
        last_ = name + " (volume " + Fmt1(volume_) + ", pitch " + Fmt1(pitch_) + ", pan " + Fmt1(pan_) + ")";
        for (int i = 0; i < 4; ++i) if (name == kBasicNames[i]) flash_[static_cast<std::size_t>(i)] = 1.0f;
        if (S().audio == nullptr || !S().audio->Enabled()) return;
        try {
            if (!cache_.count(name)) cache_[name] = std::make_unique<SoundEffect>(content_->Load<SoundEffect>("Audio/" + name));
            cache_[name]->Play(volume_, pitch_, pan_);
        } catch (const std::exception& e) {
            Museum::Log("audio", "%s", e.what());
            ReportAudioError("SoundEffect.Play", e);
        }
    }
    void SetLayer(int i, float v)
    {
        layerVolume_[static_cast<std::size_t>(i)] = v;
        if (layers_[static_cast<std::size_t>(i)]) layers_[static_cast<std::size_t>(i)]->setVolumeProperty(v);
    }
    void ApplyLayers()
    {
        for (auto& l : layers_) if (l) l->setPitchProperty(bend_ ? -0.5f : 0.0f);
    }
    [[nodiscard]] int Playing() const
    {
        int n = 0;
        for (const auto& l : layers_) if (l && l->getStateProperty() == SoundState::Playing) ++n;
        return n;
    }

    // --- synthesis ---------------------------------------------------------------------------------
    [[nodiscard]] std::vector<float> Tone(float freq, float seconds, int wave, float attackMs, float releaseMs)
    {
        const int n = static_cast<int>(22050.0f * seconds);
        std::vector<float> out(static_cast<std::size_t>(n));
        std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
        float phase = 0.0f;
        for (int i = 0; i < n; ++i) {
            const float t = static_cast<float>(i) / 22050.0f;
            phase += freq / 22050.0f;
            phase -= std::floor(phase);
            float s = 0.0f;
            switch (wave) {
                case 0: s = std::sin(phase * MathHelper::TwoPi); break;
                case 1: s = phase < 0.5f ? 0.6f : -0.6f; break;
                case 2: s = (phase * 2.0f - 1.0f) * 0.7f; break;
                case 3: s = (phase < 0.5f ? phase * 4.0f - 1.0f : 3.0f - phase * 4.0f) * 0.9f; break;
                default: s = noise(rng_) * 0.5f; break;
            }
            const float a = std::min(1.0f, t / std::max(0.001f, attackMs / 1000.0f));
            const float r = std::min(1.0f, (seconds - t) / std::max(0.001f, releaseMs / 1000.0f));
            out[static_cast<std::size_t>(i)] = s * a * r * 0.7f;
        }
        return out;
    }
    // The instance plays 16-bit PCM or float32, not both at once: switching stops it first (CNA refuses to switch while it is playing).
    void SetFloatMode(bool v)
    {
        float32_ = v;
        if (!synth_) return;
        try {
            synth_->Stop();
        } catch (const std::exception& e) {
            Museum::Log("audio", "synth stop: %s", e.what());
            ReportAudioError("synth stop", e);
        }
    }
    void PlayNote(int i)
    {
        static const float semis[8] = {0, 2, 4, 5, 7, 9, 11, 12};
        const float freq = 261.63f * std::pow(2.0f, (octave_ - 4.0f) + semis[i] / 12.0f);
        samples_ = Tone(freq, 0.7f, wave_, attack_, release_);
        flash_[0] = 1.0f;
        if (!synth_) return;
        try {
            if (float32_) synth_->SubmitFloatBufferEXT(samples_);
            else synth_->SubmitBuffer(ToPcm16(samples_));
            if (synth_->getStateProperty() != SoundState::Playing) synth_->Play();
        } catch (const std::exception& e) {
            Museum::Log("audio", "synth: %s", e.what());
            ReportAudioError("synth submit", e);
        }
    }
    void Generate()
    {
        const int rate = 22050;
        std::vector<float> s;
        std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
        switch (gen_) {
            case 0: { const int n = rate / 3; float ph = 0; for (int i = 0; i < n; ++i) { const float t = static_cast<float>(i) / static_cast<float>(n); ph += (1800.0f - 1500.0f * t) / static_cast<float>(rate); s.push_back(std::sin(ph * MathHelper::TwoPi) * (1.0f - t) * 0.7f); } break; }
            case 1: { const int n = rate; float lp = 0; for (int i = 0; i < n; ++i) { const float t = static_cast<float>(i) / static_cast<float>(n); lp += (noise(rng_) - lp) * (0.4f - 0.35f * t); s.push_back(lp * std::exp(-4.0f * t) * 1.8f); } break; }
            case 2: { const int n = rate * 2; for (int i = 0; i < n; ++i) { const float t = static_cast<float>(i) / static_cast<float>(rate); s.push_back((std::sin(t * 880.0f * MathHelper::TwoPi) + 0.5f * std::sin(t * 2217.0f * MathHelper::TwoPi) + 0.25f * std::sin(t * 3580.0f * MathHelper::TwoPi)) * std::exp(-2.2f * t) * 0.4f); } break; }
            case 3: { const int n = rate / 2; for (int i = 0; i < n; ++i) { const float t = static_cast<float>(i) / static_cast<float>(rate); const float f = t < 0.08f ? 987.77f : 1318.5f; s.push_back((std::sin(t * f * MathHelper::TwoPi) > 0 ? 0.35f : -0.35f) * std::exp(-4.5f * (t > 0.08f ? t - 0.08f : 0.0f))); } break; }
            default: { const int n = rate * 2; float lp = 0; for (int i = 0; i < n; ++i) { const float t = static_cast<float>(i) / static_cast<float>(rate); lp += (noise(rng_) - lp) * (0.05f + 0.04f * std::sin(t * 2.0f)); s.push_back(lp * 2.4f * (0.5f + 0.5f * std::sin(t * 1.3f))); } break; }
        }
        samples_ = std::move(s);
        try {
            generated_ = std::make_unique<SoundEffect>(ToPcm16(samples_), rate, AudioChannels::Mono);
        } catch (const std::exception& e) {
            Museum::Log("audio", "generate: %s", e.what());
        }
    }
    void PlayGenerated()
    {
        flash_[0] = 1.0f;
        if (generated_ && S().audio != nullptr && S().audio->Enabled()) try { generated_->Play(); } catch (const std::exception& e) { ReportAudioError("generated SoundEffect.Play", e); }
    }
    void DrawScope(SpriteBatch& sb)
    {
        Texture2D& white = *S().whiteTexture;
        sb.Draw(white, Rectangle(40, 70, 880, 220), Color(10, 14, 24, 255));
        if (samples_.empty()) return;
        const int n = static_cast<int>(samples_.size());
        for (int x = 0; x < 880; ++x) {
            const int i0 = x * n / 880, i1 = std::max(i0 + 1, (x + 1) * n / 880);
            float lo = 0, hi = 0;
            for (int i = i0; i < std::min(i1, n); ++i) { lo = std::min(lo, samples_[static_cast<std::size_t>(i)]); hi = std::max(hi, samples_[static_cast<std::size_t>(i)]); }
            const int y0 = 180 - static_cast<int>(hi * 100.0f), y1 = 180 - static_cast<int>(lo * 100.0f);
            sb.Draw(white, Rectangle(40 + x, y0, 1, std::max(1, y1 - y0)), Color(110, 230, 160, 255));
        }
        // A 48-band magnitude spectrum of the first 2048 samples (a plain DFT: it is only for the picture).
        const int m = std::min(n, 2048);
        for (int band = 0; band < 48; ++band) {
            const float f = 0.4f + static_cast<float>(band) * 0.4f;   // bin index step
            std::complex<float> acc(0, 0);
            const float k = (static_cast<float>(band) + 1.0f) * 6.0f;
            for (int i = 0; i < m; ++i) acc += samples_[static_cast<std::size_t>(i)] * std::polar(1.0f, -MathHelper::TwoPi * k * static_cast<float>(i) / static_cast<float>(m));
            const float mag = std::min(1.0f, std::abs(acc) / static_cast<float>(m) * 8.0f);
            (void)f;
            const int h = static_cast<int>(mag * 190.0f);
            sb.Draw(white, Rectangle(40 + band * 18, 500 - h, 14, h), Color(110, 170, 255, 255));
        }
        sb.DrawString(*S().font, "waveform", Vector2(48, 74), Kit::kDim, 0.0f, Vector2(0, 0), 0.6f, SpriteEffects::None, 0.0f);
        sb.DrawString(*S().font, "spectrum (low to high frequency)", Vector2(48, 310), Kit::kDim, 0.0f, Vector2(0, 0), 0.6f, SpriteEffects::None, 0.0f);
    }

    // --- music and microphone -----------------------------------------------------------------------
    // What a Song knows about itself. A song loaded from content carries no tags: no artist, album or genre, no rating, track 0.
    [[nodiscard]] std::string SongInfo() const
    {
        const Song* song = songs_[static_cast<std::size_t>(std::clamp(lastSong_, 0, 1))].get();
        if (song == nullptr) return "press a Play button";
        try {
            return "\"" + song->getNameProperty() + "\", " + Fmt1(static_cast<float>(song->getDurationProperty().getTotalSecondsProperty())) + " s, track " + std::to_string(song->getTrackNumberProperty()) + ", played " +
                   std::to_string(song->getPlayCountProperty()) + "x, rated " + (song->getIsRatedProperty() ? std::to_string(song->getRatingProperty()) : std::string("no")) + ", protected " + (song->getIsProtectedProperty() ? "yes" : "no") +
                   (song->getArtistProperty() != nullptr ? ", has an artist" : ", no artist/album/genre");
        } catch (const std::exception& e) {
            return std::string("unavailable: ") + std::string(e.what()).substr(0, 40);
        }
    }
    void PlaySong(int i)
    {
        lastSong_ = i;
        try {
            if (!songs_[static_cast<std::size_t>(i)]) songs_[static_cast<std::size_t>(i)] = std::make_unique<Song>(content_->Load<Song>(i == 0 ? "Audio/arpeggio" : "Audio/ambience"));
            MediaPlayer::setVolumeProperty(mediaVolume_);
            MediaPlayer::setIsVisualizationEnabledProperty(visualize_);
            MediaPlayer::Play(songs_[static_cast<std::size_t>(i)].get());
        } catch (const std::exception& e) {
            Museum::Log("audio", "song: %s", e.what());
        }
    }
    // A queue: Play(SongCollection) plays the songs in order (or shuffled); MoveNext and MovePrevious step through it.
    void PlayQueue()
    {
        try {
            for (int i = 0; i < 2; ++i)
                if (!songs_[static_cast<std::size_t>(i)]) songs_[static_cast<std::size_t>(i)] = std::make_unique<Song>(content_->Load<Song>(i == 0 ? "Audio/arpeggio" : "Audio/ambience"));
            queue_ = std::make_unique<SongCollection>(std::vector<Song*>{songs_[0].get(), songs_[1].get()});
            MediaPlayer::setVolumeProperty(mediaVolume_);
            MediaPlayer::setIsVisualizationEnabledProperty(visualize_);
            MediaPlayer::Play(*queue_);
        } catch (const std::exception& e) {
            Museum::Log("audio", "queue: %s", e.what());
        }
    }
    // The frequency bars and waveform MediaPlayer measures from what it is playing (VisualizationData, 256 values each).
    void DrawMusic(SpriteBatch& sb)
    {
        Texture2D& white = *S().whiteTexture;
        const auto text = [&](const std::string& t, float x, float y, Color c, float sc) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), sc, SpriteEffects::None, 0.0f); };
        text("Songs are streamed by MediaPlayer, one at a time, independently of SoundEffect.", 40, 70, Kit::kText, 0.7f);
        try {
            std::string active = "nothing queued";
            if (MediaPlayer::getQueueProperty().getCountProperty() > 0)
                active = "queue " + std::to_string(MediaPlayer::getQueueProperty().getActiveSongIndexProperty() + 1) + " of " + std::to_string(MediaPlayer::getQueueProperty().getCountProperty()) + ": " +
                         (MediaPlayer::getQueueProperty().getActiveSongProperty() != nullptr ? MediaPlayer::getQueueProperty().getActiveSongProperty()->getNameProperty() : "?");
            text("MediaPlayer.Queue: " + active + (MediaPlayer::getIsShuffledProperty() ? "   (shuffled)" : ""), 40, 100, Kit::kDim, 0.6f);
            if (visualize_) {
                VisualizationData data;
                MediaPlayer::GetVisualizationData(data);
                const auto& f = data.getFrequenciesProperty();
                const auto& w = data.getSamplesProperty();
                sb.Draw(white, Rectangle(40, 140, 880, 160), Color(18, 24, 38, 255));
                // The values are small; a slowly falling peak scales them to the box so quiet music still draws.
                float peakF = 0.0f, peakW = 0.0f;
                for (int i = 0; i < 256; ++i) {
                    peakF = std::max(peakF, f[static_cast<std::size_t>(i)]);
                    peakW = std::max(peakW, std::abs(w[static_cast<std::size_t>(i)]));
                }
                visPeakF_ = std::max(std::max(peakF, 0.002f), visPeakF_ * 0.995f);
                visPeakW_ = std::max(std::max(peakW, 0.002f), visPeakW_ * 0.995f);
                for (int i = 0; i < 64; ++i) {
                    float m = 0.0f;
                    for (int j = 0; j < 4; ++j) m = std::max(m, f[static_cast<std::size_t>(i * 4 + j)]);
                    const int h = static_cast<int>(std::min(1.0f, m / visPeakF_) * 150.0f);
                    sb.Draw(white, Rectangle(48 + i * 13, 296 - h, 11, h), Color(110, 190, 255, 255));
                }
                text("frequencies", 48, 142, Kit::kDim, 0.5f);
                sb.Draw(white, Rectangle(40, 320, 880, 140), Color(18, 24, 38, 255));
                for (int i = 0; i < 255; ++i) {
                    const float a = w[static_cast<std::size_t>(i)], b = w[static_cast<std::size_t>(i + 1)];
                    const int y0 = 390 - static_cast<int>(std::clamp(a / visPeakW_, -1.0f, 1.0f) * 60.0f), y1 = 390 - static_cast<int>(std::clamp(b / visPeakW_, -1.0f, 1.0f) * 60.0f);
                    sb.Draw(white, Rectangle(40 + i * 880 / 256, std::min(y0, y1), 4, std::abs(y1 - y0) + 2), Color(255, 214, 120, 255));
                }
                text("samples", 48, 322, Kit::kDim, 0.5f);
            } else {
                text("IsVisualizationEnabled is off: MediaPlayer does not analyse the audio.", 40, 160, Kit::kDim, 0.7f);
            }
        } catch (const std::exception&) {}
    }
    void TogglePause()
    {
        try {
            if (MediaPlayer::getStateProperty() == MediaState::Playing) MediaPlayer::Pause();
            else MediaPlayer::Resume();
        } catch (const std::exception&) {}
    }
    void ToggleMic()
    {
        try {
            mic_ = Microphone::getDefaultProperty();
            micCount_ = std::to_string(Microphone::getAllProperty().size());
            if (mic_ == nullptr) { micState_ = "no microphone connected"; return; }
            if (micRunning_) { mic_->Stop(); micRunning_ = false; micState_ = "stopped"; }
            else { mic_->Start(); micRunning_ = true; micState_ = "capturing"; }
        } catch (const std::exception& e) {
            micState_ = std::string("unavailable: ") + std::string(e.what()).substr(0, 36);
            micRunning_ = false;
        }
    }
    [[nodiscard]] std::string MicSummary() const
    {
        try {
            Microphone* m = Microphone::getDefaultProperty();
            if (m == nullptr) return "none: the queries below need one";
            return std::to_string(m->getSampleRateProperty()) + " Hz, " + std::to_string(static_cast<int>(m->getBufferDurationProperty().getTotalMillisecondsProperty())) + " ms buffer, " +
                   std::to_string(m->GetSampleSizeInBytes(System::TimeSpan::FromMilliseconds(100.0))) + " bytes per 100 ms" + (m->getIsHeadsetProperty() ? ", headset" : "");
        } catch (const std::exception& e) {
            return std::string("unavailable: ") + std::string(e.what()).substr(0, 40);
        }
    }
    void PollMic()
    {
        try {
            Bytes buf(4096);
            const int got = mic_->GetData(buf);
            if (got < 2) return;
            double sum = 0;
            for (int i = 0; i + 1 < got; i += 2) { const std::int16_t v = static_cast<std::int16_t>(buf[static_cast<std::size_t>(i)] | (buf[static_cast<std::size_t>(i) + 1] << 8)); sum += static_cast<double>(v) * v; }
            micLevel_ = static_cast<float>(std::sqrt(sum / (got / 2)) / 32768.0);
        } catch (const std::exception& e) {
            micState_ = std::string("capture failed: ") + std::string(e.what()).substr(0, 36);
        }
    }

    static constexpr const char* kXactCategories[3] = {"Default", "Music", "Effects"};

    void LoadXact()
    {
        const std::string root = S().contentRoot + "/Xact/";
        try {
            xEngine_ = std::make_unique<AudioEngine>(root + "museum.xgs");
            xWaves_ = std::make_unique<WaveBank>(xEngine_.get(), root + "museum.xwb");
            xBank_ = std::make_unique<SoundBank>(xEngine_.get(), root + "museum.xsb");
            xStatus_ = "museum.xgs (3 categories), museum.xwb (5 waves), museum.xsb (5 cues)";
        } catch (const std::exception& e) {
            xBank_.reset();
            xWaves_.reset();
            xEngine_.reset();
            xStatus_ = std::string("not available here: ") + std::string(e.what()).substr(0, 60);
            Museum::Log("audio", "XACT: %s", e.what());
        }
    }
    void XactPlay()
    {
        if (!xBank_) return;
        static const char* names[5] = {"Tone", "Click", "Arpeggio", "Drums", "Chirp"};
        try {
            xCue_ = xBank_->GetCue(names[xCueIndex_]);
            xCue_->SetVariable("Distance", xDistance_);   // a new cue starts from the slider, not from the file's initial value
            xCue_->Play();
            xActive_ = true;
            XactReadVariables();
        } catch (const std::exception& e) {
            xStatus_ = std::string("Play failed: ") + std::string(e.what()).substr(0, 50);
            ReportAudioError("Cue.Play", e);
        }
    }
    void XactFire()
    {
        if (!xBank_) return;
        static const char* names[5] = {"Tone", "Click", "Arpeggio", "Drums", "Chirp"};
        try { xBank_->PlayCue(names[xCueIndex_]); } catch (const std::exception& e) { ReportAudioError("SoundBank.PlayCue", e); }
    }
    // Variables the .xgs declares; RPC curves in the file turn them into volume. The value is clamped to the declared range and read back.
    void XactDistance(float v)
    {
        try {
            if (xCue_ != nullptr) xCue_->SetVariable("Distance", v);
            xDistance_ = v;
            XactReadVariables();
        } catch (const std::exception& e) {
            xVariables_ = std::string("Distance: ") + std::string(e.what()).substr(0, 50);
        }
    }
    void XactTrim(float v)
    {
        try {
            if (xEngine_) xEngine_->SetGlobalVariable("MasterTrim", v);
            XactReadVariables();
        } catch (const std::exception& e) {
            xVariables_ = std::string("MasterTrim: ") + std::string(e.what()).substr(0, 50);
        }
    }
    void XactReadVariables()
    {
        try {
            std::string text = "MasterTrim " + Fmt1(xEngine_ ? xEngine_->GetGlobalVariable("MasterTrim") : 0.0f);
            text += xCue_ != nullptr ? ", Distance " + Fmt1(xCue_->GetVariable("Distance")) : std::string(", Distance (play a cue first)");
            xVariables_ = text;
            Museum::Log("audio", "XACT variables: %s", text.c_str());
        } catch (const std::exception& e) {
            xVariables_ = std::string("unavailable: ") + std::string(e.what()).substr(0, 50);
        }
    }
    [[nodiscard]] std::string XactFlags() const
    {
        if (xCue_ == nullptr) return "-";
        try {
            return std::string("created ") + (xCue_->getIsCreatedProperty() ? "y" : "n") + ", preparing " + (xCue_->getIsPreparingProperty() ? "y" : "n") + ", prepared " + (xCue_->getIsPreparedProperty() ? "y" : "n") +
                   ", playing " + (xCue_->getIsPlayingProperty() ? "y" : "n") + ", stopping " + (xCue_->getIsStoppingProperty() ? "y" : "n") + ", stopped " + (xCue_->getIsStoppedProperty() ? "y" : "n");
        } catch (const std::exception&) {
            return "?";
        }
    }
    [[nodiscard]] std::string XactEngineInfo() const
    {
        if (!xEngine_ || !xBank_ || !xWaves_) return "-";
        try {
            std::string renderer = "no renderer";
            if (!xEngine_->getRendererDetailsProperty().empty()) renderer = xEngine_->getRendererDetailsProperty()[0].getFriendlyNameProperty();
            return std::string("sound bank ") + (xBank_->getIsInUseProperty() ? "in use" : "idle") + ", wave bank " + (xWaves_->getIsInUseProperty() ? "in use" : "idle") + ", content version " + std::to_string(AudioEngine::ContentVersion) + ", " + renderer;
        } catch (const std::exception& e) {
            static bool reported = false;   // per-frame path: report the first failure only
            if (!reported) { reported = true; ReportAudioError("XACT engine readout", e); }
            return "?";
        }
    }
    void XactCategoryVolume(int i, float v)
    {
        catVolume_[static_cast<std::size_t>(i)] = v;
        if (xEngine_) try { xEngine_->GetCategory(kXactCategories[i]).SetVolume(v); } catch (const std::exception& e) { ReportAudioError("AudioCategory.SetVolume", e); }
    }
    void XactPauseMusic(bool pause)
    {
        if (!xEngine_) return;
        try {
            AudioCategory music = xEngine_->GetCategory("Music");
            if (pause) music.Pause(); else music.Resume();
        } catch (const std::exception& e) {
            ReportAudioError("AudioCategory pause/resume", e);
        }
    }
    [[nodiscard]] bool XactPlaying() const
    {
        try { return xCue_ != nullptr && xCue_->getIsPlayingProperty(); } catch (const std::exception&) { return false; }
    }
    [[nodiscard]] std::string XactState() const
    {
        if (!xEngine_) return "XACT not available";
        if (xCue_ == nullptr) return "no cue yet";
        try {
            return xCue_->getNameProperty() + ": " + (xCue_->getIsStoppedProperty() ? "stopped" : (xCue_->getIsPausedProperty() ? "paused" : (xCue_->getIsPlayingProperty() ? "playing" : (xCue_->getIsPreparedProperty() ? "prepared" : "created"))));
        } catch (const std::exception&) {
            return "?";
        }
    }

    // For the paths this room reaches only with audio on (and, for XACT, after the banks loaded): a failure there is a
    // museum or CNA defect and is reported. A missing device is CNA's documented refusal (NoAudioHardwareException), not one.
    void ReportAudioError(const char* context, const std::exception& e) const
    {
        if (dynamic_cast<const NoAudioHardwareException*>(&e) != nullptr) return;
        ReportError(std::string(context) + ": " + e.what());
    }

    void RestoreGlobals()
    {
        SoundEffect::setDistanceScaleProperty(1.0f);
        SoundEffect::setDopplerScaleProperty(1.0f);
        SoundEffect::setSpeedOfSoundProperty(343.0f);
        if (mode_ == Mode::Music) try { MediaPlayer::Stop(); } catch (const std::exception&) {}
    }

    Mode mode_;
    std::mt19937 rng_;
    // XACT: declared engine first, so the sound bank and wave bank are destroyed before it.
    std::unique_ptr<AudioEngine> xEngine_;
    std::unique_ptr<WaveBank> xWaves_;
    std::unique_ptr<SoundBank> xBank_;
    float xDistance_ = 0.0f;
    std::string xVariables_ = "-";
    Cue* xCue_ = nullptr;
    int xCueIndex_ = 0;
    bool xActive_ = false;
    std::array<float, 3> catVolume_{1.0f, 1.0f, 1.0f};
    std::string xStatus_ = "-";
    std::unique_ptr<Content::ContentManager> content_;
    std::array<std::unique_ptr<SoundEffect>, 4> effects_;
    std::array<std::unique_ptr<SoundEffectInstance>, 4> layers_;
    std::unique_ptr<SoundEffectInstance> spatial_;
    std::unique_ptr<DynamicSoundEffectInstance> synth_;
    std::unique_ptr<SoundEffect> generated_;
    std::map<std::string, std::unique_ptr<SoundEffect>> cache_;
    std::array<std::unique_ptr<Song>, 2> songs_;
    int lastSong_ = 0, songEvents_ = 0, stateEvents_ = 0, bufferNeeded_ = 0;
    System::EventHandler<System::EventArgs>::Token songToken_ = 0, stateToken_ = 0, neededToken_ = 0;
    std::unique_ptr<SongCollection> queue_;
    bool float32_ = false;
    bool visualize_ = true;
    float visPeakF_ = 0.01f, visPeakW_ = 0.01f;
    Microphone* mic_ = nullptr;
    std::vector<float> samples_;
    std::array<float, 4> layerVolume_{0.4f, 0.0f, 0.0f, 0.0f}, flash_{};
    Vector3 emitterPos_, listenerPos_;
    std::string last_ = "-", micState_ = "idle", micCount_ = "?";
    float time_ = 0.0f, volume_ = 0.8f, pitch_ = 0.0f, pan_ = 0.0f, radius_ = 3.0f, speed_ = 0.8f, distance_ = 0.0f, distanceScale_ = 1.0f, dopplerScale_ = 1.0f, soundSpeed_ = 343.0f;
    float attack_ = 15.0f, release_ = 250.0f, octave_ = 4.0f, master_ = 1.0f, mediaVolume_ = 0.5f, micLevel_ = 0.0f;
    bool bend_ = false, micRunning_ = false;
    int wave_ = 0, gen_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeAudioLab(const RoomDef& room, Services& services)
{
    return std::make_unique<AudioLab>(room, services);
}
