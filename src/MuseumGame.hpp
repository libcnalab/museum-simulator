#pragma once

#include <memory>
#include <set>
#include "World/Progress.hpp"
#include <string>
#include <vector>

#include "Audio/AudioSystem.hpp"
#include "Camera/FirstPersonCamera.hpp"
#include "Camera/InputState.hpp"
#include "Camera/PlayerController.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "MuseumOptions.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/ModelLibrary.hpp"
#include "Rendering/SignFactory.hpp"
#include "Rendering/SkyDome.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "UI/DirectoryScreen.hpp"
#include "UI/Hud.hpp"
#include "World/MuseumWorld.hpp"

/**
 * The application: a CNA Game that owns the camera, the world, audio and HUD. Update and
 * Draw only orchestrate; each exhibit does its own work.
 */
#ifdef MUSEUM_NETWORKING
namespace Microsoft::Xna::Framework::GamerServices { class GamerServicesComponent; }
#endif

class MuseumGame : public Microsoft::Xna::Framework::Game {
public:
    explicit MuseumGame(const MuseumOptions& options);
    ~MuseumGame() override;
    GetTypeNameHPP()

protected:
    void LoadContent() override;
    void UnloadContent() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

private:
    void HandleSystemKeys();
    void SetPaused(bool paused);
    void TeleportToCell(int cell);
    void GoToView(const std::string& name);
    std::string pendingView_;   // a teleport asked for from inside an exhibit's callback runs at the start of the next update
    void ResetToEntrance();
    void UpdateFrameStats(float dt);
    void RecordLoopStats(const Microsoft::Xna::Framework::GameTime& gameTime);
    void RunScript();
    void StartRide(int floor);
    void OpenDirectory();
    void ApplyMouseMode();
    void UpdateRide(float dt);
    void SaveScreenshot(const std::string& path);
    void ReportInteractionFailure(const std::string& what);
    void ScriptError(const std::string& what);

    MuseumOptions options_;
    Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
#ifdef MUSEUM_NETWORKING
    // Runs CNA's GamerServices dispatcher (sign-in, the Guide overlay, session updates) every frame; the Players and Networking rooms rely on it.
    std::unique_ptr<Microsoft::Xna::Framework::GamerServices::GamerServicesComponent> gamerServices_;
#endif

    // Everything below holds GPU resources or services and is released in LoadContent's reverse order.
    Services services_;
    FrameStats stats_;
    std::unique_ptr<TextureLibrary> textures_;
    std::unique_ptr<ModelLibrary> models_;
    std::unique_ptr<EffectBank> effects_;
    std::unique_ptr<Shapes> shapes_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> sprites_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteFont> font_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteFont> titleFont_;
    std::unique_ptr<SignFactory> signs_;
    std::unique_ptr<AudioSystem> audio_;
    TimeOfDay timeOfDay_;
    std::unique_ptr<SkyDome> sky_;
    std::unique_ptr<MuseumWorld> world_;
    std::unique_ptr<Hud> hud_;
    std::unique_ptr<DirectoryScreen> directory_;

    FirstPersonCamera camera_;
    std::unique_ptr<PlayerController> player_;
    InputState input_;

    double time_ = 0.0;
    long long frame_ = 0;
    bool paused_ = false;
    bool wasActive_ = true;
    bool guideWasUp_ = false;   // Guide.IsVisible on the previous frame (its changes are logged)
    bool showHelp_ = false;
    bool showStats_ = false;
    bool showMap_ = true;
    bool timeRuns_ = false;   // the museum clock advances on its own (key Y)
    float sensitivityScale_ = 1.0f;
    const MuseumPlan* museum_ = nullptr;
    int shownCell_ = -1;
    std::set<int> discovered_;
    std::unique_ptr<ProgressStore> progress_;
    bool progressDirty_ = false;
    float progressIdleSeconds_ = 0.0f;
    void UpdateProgressAutosave(float dt, bool visitorActive);
    struct Ride {
        bool active = false;
        bool swapped = false;
        int floor = 0;
        float time = 0.0f;
    } ride_;
    // A short glide of the camera to a good viewing spot (an exhibit's "step up to the screen").
    struct Glide {
        bool active = false;
        Microsoft::Xna::Framework::Vector3 from, to;
        float yawFrom = 0.0f, yawTo = 0.0f, pitchFrom = 0.0f, pitchTo = 0.0f, time = 0.0f;
    } glide_;
    void StartGlide(const Microsoft::Xna::Framework::Vector3& eye, float yaw, float pitch);
    float bannerAge_ = 99.0f;
    float fpsAccumulator_ = 0.0f;
    int fpsFrames_ = 0;
    bool probeElapsedAfterStall_ = false;
    int updatesThisSecond_ = 0;
    int drawsThisSecond_ = 0;
    std::string rendererName_;
    /** Runs one stage of the frame; an exception is logged (once per distinct message) and the frame carries on. */
    template <typename Fn>
    void Stage(const char* name, Fn&& fn);
    std::set<std::string> drawErrorsSeen_;
    std::string focusPrompt_;
    bool screenshotDone_ = false;
    int scriptWait_ = 0;
    int awaitFrames_ = 0;   // await: frames left before giving up
    std::uint64_t awaitAfter_ = 0;
    std::string awaitText_;
    std::string pendingShot_;
    std::vector<std::string> scriptSteps_;
    std::size_t scriptIndex_ = 0;
    int scriptErrors_ = 0;   // unknown rooms or commands and malformed arguments in --script (counted in the stats line)
};
