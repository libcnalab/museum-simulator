#pragma once

#include <functional>
#include <string>

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

class AudioSystem;
class EffectBank;
class SignFactory;
class TextureLibrary;
class ModelLibrary;
class Shapes;
class SkyDome;
class TimeOfDay;
class MuseumPlan;
namespace Microsoft::Xna::Framework { class GraphicsDeviceManager; }
struct SceneView;

/** Per-frame numbers shown on the Information Hall screens and the F3 overlay. */
struct FrameStats {
    float fps = 0.0f;
    float frameMilliseconds = 0.0f;
    long long frame = 0;
    int backBufferWidth = 0;
    int backBufferHeight = 0;
    bool fullscreen = false;
    int roomsDrawn = 0;
    int activeVoices = 0;

    // The application model, as seen by the game loop (updated by MuseumGame).
    long long updates = 0;           // Game.Update calls
    long long draws = 0;             // Game.Draw calls
    float updatesPerSecond = 0.0f;
    float drawsPerSecond = 0.0f;
    double totalSeconds = 0.0;       // GameTime.TotalGameTime
    float elapsedMilliseconds = 0.0f;// GameTime.ElapsedGameTime of the latest Update
    bool runningSlowly = false;      // GameTime.IsRunningSlowly
    bool fixedStep = false;          // Game.IsFixedTimeStep
    float targetMilliseconds = 0.0f; // Game.TargetElapsedTime
    float frameHistory[120] = {};    // recent ElapsedGameTime values, oldest first
    bool typingCapture = false;      // text-entry exhibit: the keyboard types instead of walking (Esc releases it)
    float injectedDelayMs = 0.0f;    // lab control: burn this long inside every Update
    int hitchFrames = 0;             // lab control: frames of extra delay left
    float oneShotStallMs = 0.0f;     // lab control: one Update burns this long, once
    bool resetElapsedAfterStall = false;   // ... and then calls Game.ResetElapsedTime()
    bool suppressDrawNext = false;   // lab control: the next Update calls Game.SuppressDraw()
    int drawsSuppressed = 0;         // how many times it did
    int cellsBuilt = 0, cellsLoaded = 0;   // lazily built world (see MuseumWorld)
    long long cellsBuiltTotal = 0, cellsDestroyedTotal = 0;
};

/** Everything an exhibit may use. Pointers are owned by MuseumGame and outlive every exhibit. */
struct Services {
    Microsoft::Xna::Framework::Game* game = nullptr;
    Microsoft::Xna::Framework::Graphics::GraphicsDevice* device = nullptr;
    Microsoft::Xna::Framework::Content::ContentManager* content = nullptr;
    TextureLibrary* textures = nullptr;
    ModelLibrary* models = nullptr;
    EffectBank* effects = nullptr;
    Shapes* shapes = nullptr;
    SkyDome* sky = nullptr;
    TimeOfDay* timeOfDay = nullptr;
    SignFactory* signs = nullptr;
    AudioSystem* audio = nullptr;
    Microsoft::Xna::Framework::Graphics::SpriteBatch* sprites = nullptr;
    Microsoft::Xna::Framework::Graphics::SpriteFont* font = nullptr;
    Microsoft::Xna::Framework::Graphics::SpriteFont* titleFont = nullptr;
    const FrameStats* stats = nullptr;
    const MuseumPlan* museum = nullptr;
    Microsoft::Xna::Framework::GraphicsDeviceManager* graphics = nullptr;
    FrameStats* mutableStats = nullptr;      // labs that change the game loop's behaviour write here
    const SceneView* mainView = nullptr;   // the visitor's camera for this frame (valid during Draw/DrawOverlay)
    Microsoft::Xna::Framework::Graphics::Texture2D* whiteTexture = nullptr;
    std::function<void(int floor)> travelToFloor;   // elevator: ride to a floor's elevator car
    std::function<void(const std::string& name)> showView;   // teleport to a named reference view
    std::function<void(const Microsoft::Xna::Framework::Vector3& eye, float yaw, float pitch)> focusView;   // glide to a viewing spot
    std::function<void()> openDirectory;            // the full-screen directory and map
    // Runs ACTION after SECONDS of game time, also while the museum is paused (a minimised window pauses it). ACTION must not
    // capture an exhibit, which may be unloaded meanwhile; the game outlives every room.
    std::function<void(float seconds, std::function<void()> action)> after;
    std::string contentRoot;
    // What the window title and the device manager's profile were before the museum set anything: the defaults XNA takes
    // from the assembly title and the project profile (declared in Main.cpp).
    std::string startupWindowTitle;
    int startupGraphicsProfile = -1;
    bool supportsThreeD = true;
    bool supportsWireframe = true;
};

/** A camera, its matrices and the frustum built from them. */
struct SceneView {
    Microsoft::Xna::Framework::Matrix view;
    Microsoft::Xna::Framework::Matrix projection;
    Microsoft::Xna::Framework::Vector3 eye;
    Microsoft::Xna::Framework::BoundingFrustum frustum;

    SceneView(const Microsoft::Xna::Framework::Matrix& v, const Microsoft::Xna::Framework::Matrix& p,
              const Microsoft::Xna::Framework::Vector3& e)
        : view(v), projection(p), eye(e), frustum(v * p)
    {
    }
};

/** Passed to every draw call. `offscreen` is true while rendering into a RenderTarget2D. */
struct DrawContext {
    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device;
    EffectBank& fx;
    const SceneView& view;
    double time;
    bool offscreen;
};
