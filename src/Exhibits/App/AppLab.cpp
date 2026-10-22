// The application model on display: the Game loop and GameTime, GraphicsDevice and adapter information,
// the back buffer and viewport, ContentManager lifetime and GameComponents. Several rooms, one class.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <string>

#include "CNA/AssemblyInfo.hpp"
#include "CNA/ProjectGraphicsProfile.hpp"
#include "CNA/GraphicsCapability.hpp"
#include "CNA/RendererCapabilityProfile.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Microsoft/Xna/Framework/IUpdateable.hpp"
#include "Microsoft/Xna/Framework/IGameComponent.hpp"
#include "Microsoft/Xna/Framework/IDrawable.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceInformation.hpp"
#include "Microsoft/Xna/Framework/PreparingDeviceSettingsEventArgs.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Content/ContentLoadException.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/DrawableGameComponent.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GameComponentCollection.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsAdapter.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDeviceStatus.hpp"
#include "Microsoft/Xna/Framework/Graphics/PresentInterval.hpp"
#include "Microsoft/Xna/Framework/Graphics/PresentationParameters.hpp"
#include "Rendering/LiveScreen.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Lifecycle, GameTimeLab, Device, Viewport, Content, Components };

Mode ParseMode(const std::string& s)
{
    if (s == "gametime") return Mode::GameTimeLab;
    if (s == "device") return Mode::Device;
    if (s == "viewport") return Mode::Viewport;
    if (s == "content") return Mode::Content;
    if (s == "components") return Mode::Components;
    return Mode::Lifecycle;
}

// A GameComponent that draws a labelled badge on the screen, to show UpdateOrder / DrawOrder at work.
class BadgeComponent final : public DrawableGameComponent {
public:
    BadgeComponent(Game& game, Services& s, std::string name, Color color, int x, int y)
        : DrawableGameComponent(game), s_(s), name_(std::move(name)), color_(color), x_(x), y_(y)
    {
    }
    void Initialize() override
    {
        DrawableGameComponent::Initialize();
        initialized = true;
    }
    void Update(GameTime& gameTime) override
    {
        GameComponent::Update(gameTime);
        ++updates;
        lastUpdateOrder = updateSerial++;
    }
    void Draw(const GameTime& gameTime) override
    {
        DrawableGameComponent::Draw(gameTime);
        if (s_.sprites == nullptr || s_.font == nullptr) return;
        const SamplerState sampler = SamplerState::LinearClamp;
        s_.sprites->Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &sampler, nullptr, nullptr);
        s_.sprites->Draw(*s_.whiteTexture, Rectangle(x_, y_, 250, 110), color_);
        s_.sprites->Draw(*s_.whiteTexture, Rectangle(x_, y_, 250, 4), Color(255, 255, 255, 230));
        const std::string line1 = name_;
        const std::string line2 = "DrawOrder " + std::to_string(getDrawOrderProperty()) + "   UpdateOrder " + std::to_string(getUpdateOrderProperty());
        const std::string line3 = "Update calls " + std::to_string(updates);
        s_.sprites->DrawString(*s_.font, line1, Vector2(static_cast<float>(x_ + 12), static_cast<float>(y_ + 12)), Color::White, 0.0f, Vector2(0, 0), 0.9f, SpriteEffects::None, 0.0f);
        s_.sprites->DrawString(*s_.font, line2, Vector2(static_cast<float>(x_ + 12), static_cast<float>(y_ + 46)), Color::White, 0.0f, Vector2(0, 0), 0.6f, SpriteEffects::None, 0.0f);
        s_.sprites->DrawString(*s_.font, line3, Vector2(static_cast<float>(x_ + 12), static_cast<float>(y_ + 72)), Color::White, 0.0f, Vector2(0, 0), 0.6f, SpriteEffects::None, 0.0f);
        s_.sprites->End();
    }
    int updates = 0;
    int lastUpdateOrder = 0;
    bool initialized = false;
    inline static int updateSerial = 0;

private:
    Services& s_;
    std::string name_;
    Color color_;
    int x_, y_;
};

class AppLab final : public Exhibit {
public:
    AppLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    ~AppLab() override
    {
        try {
            if (S().graphics != nullptr && resetToken_ != 0) {
                S().graphics->DeviceResetting.Remove(resettingToken_);
                S().graphics->DeviceReset.Remove(resetToken_);
                if (S().device != nullptr) S().device->DeviceLost.Remove(lostToken_);
                S().graphics->PreparingDeviceSettings.Remove(preparingToken_);
            }
            if (S().game != nullptr && addedToken_ != 0) {
                S().game->getComponentsProperty().ComponentAdded.Remove(addedToken_);
                S().game->getComponentsProperty().ComponentRemoved.Remove(removedToken_);
            }
        } catch (...) {
        }
        if (S().game != nullptr)
            for (auto& c : components_) (void)S().game->getComponentsProperty().Remove(c.get());
    }

    void BuildStatic(RoomBuilder& b) override
    {
        FrameStats* st = S().mutableStats;
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Lifecycle:
                p.AddHeading("Counters since the museum started");
                p.AddReadout("Initialize / LoadContent", [] { return std::string("1 / 1"); });
                p.AddReadout("Update calls", [this] { return std::to_string(S().stats->updates); });
                p.AddReadout("Draw calls", [this] { return std::to_string(S().stats->draws); });
                p.AddReadout("Updates / Draws per second", [this] {
                    char buf[64];
                    std::snprintf(buf, sizeof buf, "%.0f / %.0f", static_cast<double>(S().stats->updatesPerSecond), static_cast<double>(S().stats->drawsPerSecond));
                    return std::string(buf);
                });
                p.AddReadout("TotalGameTime", [this] { return Fmt(static_cast<float>(S().stats->totalSeconds), " s"); });
                p.AddReadout("ElapsedGameTime", [this] { return Fmt(S().stats->elapsedMilliseconds, " ms"); });
                p.AddReadout("IsRunningSlowly", [this] { return std::string(S().stats->runningSlowly ? "TRUE" : "false"); });
                p.AddReadout("Game.Exiting", [] { return std::string("raised once when you quit (see the log)"); });
                p.AddReadout("LaunchParameters", [this] {
                    if (S().game == nullptr) return std::string("-");
                    const auto& lp = S().game->getLaunchParametersProperty();
                    return std::to_string(lp.size()) + " entries" + (lp.empty() ? "" : " (e.g. " + lp.begin()->first + ")");
                });
                p.AddHeading("Change the loop");
                p.AddToggle("IsFixedTimeStep", S().game != nullptr && S().game->getIsFixedTimeStepProperty(), [this](bool v) {
                    if (S().game != nullptr) S().game->setIsFixedTimeStepProperty(v);
                });
                p.AddChoice("TargetElapsedTime", {"1/30 s", "1/60 s", "1/120 s"}, 1, [this](int v) {
                    if (S().game != nullptr) S().game->setTargetElapsedTimeProperty(System::TimeSpan::FromSeconds(v == 0 ? 1.0 / 30 : (v == 1 ? 1.0 / 60 : 1.0 / 120)));
                });
                p.AddButton("Hitch: 40 ms Updates for 60 frames", [st] { if (st != nullptr) st->hitchFrames = 60; });
                break;
            case Mode::GameTimeLab:
                p.AddSlider("Extra delay per Update", 0.0f, 80.0f, delay_, [this, st](float v) { delay_ = v; if (st != nullptr) st->injectedDelayMs = v; }, "%.0f ms", 1.0f);
                p.AddButton("One 400 ms stall, nothing done about it", [st] { if (st != nullptr) { st->oneShotStallMs = 400.0f; st->resetElapsedAfterStall = false; } });
                p.AddButton("One 400 ms stall, then Game.ResetElapsedTime()", [st] { if (st != nullptr) { st->oneShotStallMs = 400.0f; st->resetElapsedAfterStall = true; } });
                p.AddButton("Game.SuppressDraw(): skip the next Draw", [st] { if (st != nullptr) st->suppressDrawNext = true; });
                p.AddSlider("Game.InactiveSleepTime (window in the background)", 0.0f, 100.0f, static_cast<float>(S().game->getInactiveSleepTimeProperty().getTotalMillisecondsProperty()), [this](float v) { S().game->setInactiveSleepTimeProperty(System::TimeSpan::FromMilliseconds(static_cast<double>(v))); }, "%.0f ms", 1.0f);
                p.AddReadout("Longest frame, last 2 seconds", [this] { return Fmt(*std::max_element(S().stats->frameHistory, S().stats->frameHistory + 120), " ms"); });
                p.AddReadout("Draws suppressed so far", [this] { return std::to_string(S().stats->drawsSuppressed); });
                p.AddHeading("Two balls cross the track");
                p.AddReadout("Time-based speed", [] { return std::string("2.0 m per second"); });
                p.AddReadout("Frame-based speed", [] { return std::string("0.0333 m per Update"); });
                p.AddReadout("Updates per second", [this] { return Fmt(S().stats->updatesPerSecond); });
                p.AddReadout("Frame time", [this] { return Fmt(S().stats->elapsedMilliseconds, " ms"); });
                break;
            case Mode::Viewport: {
                const GraphicsDeviceManager* g = S().graphics;
                p.AddChoice("Back buffer size", {"960 x 540", "1280 x 720", "1600 x 900", "1920 x 1080"}, 1, [this](int v) {
                    static const int w[] = {960, 1280, 1600, 1920}, h[] = {540, 720, 900, 1080};
                    pendingW_ = w[v];
                    pendingH_ = h[v];
                });
                p.AddToggle("Fullscreen", g != nullptr && g->getIsFullScreenProperty(), [this](bool) { pendingToggleFull_ = true; });
                p.AddToggle("Vertical sync", true, [this](bool v) { pendingVsync_ = v ? 1 : 0; });
                p.AddButton("GraphicsDevice.Present() now", [this] { pendingPresent_ = true; });
                p.AddHeading("Presentation parameters (live)");
                p.AddReadout("Back buffer", [this] { return BackBufferText(); });
                p.AddReadout("Depth / stencil", [this] { return std::to_string(static_cast<int>(S().device->getPresentationParametersProperty().getDepthStencilFormatProperty())) + " (DepthFormat)"; });
                p.AddReadout("DeviceResetting / DeviceReset events", [this] { return std::to_string(resettingCount_) + " / " + std::to_string(resetCount_); });
                p.AddReadout("DeviceLost events", [this] { return std::to_string(lostCount_) + " (never here)"; });
                p.AddReadout("Manual Present()", [this] { return presentStatus_; });
                p.AddReadout("Texture slots bound: pixel (16) / vertex (4)", [this] {
                    const auto bound = [](TextureCollection& slots, int count) {
                        int n = 0;
                        for (int i = 0; i < count; ++i) {
                            try { if (slots[i] != nullptr) ++n; } catch (const std::exception&) { break; }
                        }
                        return n;
                    };
                    return std::to_string(bound(S().device->getTexturesProperty(), 16)) + " / " + std::to_string(bound(S().device->getVertexTexturesProperty(), 4));
                });
                p.AddReadout("PreparingDeviceSettings", [this] { return std::to_string(preparingCount_) + (preparingCount_ > 0 ? ", last asked for " + preparingNote_ : std::string(" (fires on ApplyChanges)")); });
                p.AddReadout("PresentInterval", [this] {
                    static const char* names[] = {"Default", "One (vsync)", "Two", "Immediate"};
                    return std::string(names[std::clamp(static_cast<int>(S().device->getPresentationParametersProperty().getPresentationIntervalProperty()), 0, 3)]);
                });
                p.AddReadout("GraphicsDeviceStatus", [this] {
                    static const char* names[] = {"Normal", "Lost", "NotReset"};
                    return std::string(names[std::clamp(static_cast<int>(S().device->getGraphicsDeviceStatusProperty()), 0, 2)]);
                });
                p.AddReadout("Viewport", [this] { return ViewportText(); });
                p.AddReadout("Window client bounds", [this] {
                    const Rectangle r = S().game->getWindowProperty().getClientBoundsProperty();
                    return std::to_string(r.Width) + " x " + std::to_string(r.Height);
                });
                p.AddReadout("Aspect ratio", [this] { return Fmt(S().device->getViewportProperty().getAspectRatioProperty()); });
                // The depth range a Viewport maps clip-space depth into: Project returns Z between MinDepth and MaxDepth.
                p.AddSlider("Viewport.MinDepth", 0.0f, 0.9f, minDepth_, [this](float v) { minDepth_ = v; if (maxDepth_ < minDepth_ + 0.05f) maxDepth_ = minDepth_ + 0.05f; }, "%.2f");
                p.AddSlider("Viewport.MaxDepth", 0.1f, 1.0f, maxDepth_, [this](float v) { maxDepth_ = std::max(v, minDepth_ + 0.05f); }, "%.2f");
                p.AddReadout("Project() of a point on the near / far plane", [this] { return DepthRangeText(); });
                break;
            }
            case Mode::Content:
                p.AddButton("Load 12 textures (ContentManager.Load)", [this] { LoadSet(); });
                p.AddButton("Load them again (cache hit)", [this] { LoadSet(); });
                p.AddButton("Unload everything (ContentManager.Unload)", [this] { UnloadSet(); });
                p.AddButton("Load a file that does not exist", [this] { LoadMissing(); });
                p.AddHeading("What happened");
                p.AddReadout("Textures held", [this] { return std::to_string(held_.size()); });
                p.AddReadout("Last Load time", [this] { return lastLoad_; });
                p.AddReadout("Failure message", [this] { return lastError_; });
                break;
            case Mode::Components:
                p.AddReadout("Game.Components added / removed", [this] { return std::to_string(addedCount_) + " / " + std::to_string(removedCount_) + ", event component " + (lastComponentEventHadComponent_ ? "non-null" : "-"); });
                p.AddToggle("Red component: Visible", true, [this](bool v) { SetVisible(0, v); });
                p.AddToggle("Green component: Visible", true, [this](bool v) { SetVisible(1, v); });
                p.AddToggle("Blue component: Visible", true, [this](bool v) { SetVisible(2, v); });
                p.AddSlider("Red DrawOrder", 0.0f, 10.0f, 1.0f, [this](float v) { SetDrawOrder(0, static_cast<int>(v)); }, "%.0f", 1.0f);
                p.AddSlider("Green DrawOrder", 0.0f, 10.0f, 2.0f, [this](float v) { SetDrawOrder(1, static_cast<int>(v)); }, "%.0f", 1.0f);
                p.AddSlider("Blue DrawOrder", 0.0f, 10.0f, 3.0f, [this](float v) { SetDrawOrder(2, static_cast<int>(v)); }, "%.0f", 1.0f);
                p.AddToggle("Green component: Enabled (Update)", true, [this](bool v) { if (components_.size() > 1) components_[1]->setEnabledProperty(v); });
                p.AddHeading("Game.Components and Game.Services");
                p.AddReadout("Components registered", [this] { return std::to_string(S().game != nullptr ? S().game->getComponentsProperty().getCountProperty() : 0); });
                p.AddReadout("IGameComponent.Initialize", [this] {
                    return std::to_string(std::count_if(components_.begin(), components_.end(), [](const auto& c) { return c->initialized; })) + " of " + std::to_string(components_.size());
                });
                p.AddReadout("IGameComponent: also IUpdateable / IDrawable", [this] {
                    if (S().game == nullptr) return std::string("-");
                    int updateable = 0, drawable = 0, total = 0;
                    for (IGameComponent* c : S().game->getComponentsProperty()) {
                        ++total;
                        updateable += dynamic_cast<IUpdateable*>(c) != nullptr;
                        drawable += dynamic_cast<IDrawable*>(c) != nullptr;
                    }
                    return std::to_string(total) + " components: " + std::to_string(updateable) + " / " + std::to_string(drawable);
                });
                break;
            case Mode::Device: break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);

        if (mode_ == Mode::Lifecycle) {
            SignSpec flow = Kit::Plate("The life of a Game", {"Initialize       once, after the device exists", "LoadContent      once: textures, models, sounds", "loop  { Update(gameTime)  ->  Draw(gameTime) }", "UnloadContent    once, when the game ends", "Exiting event, then Dispose"});
            flow.leftAlign = true;
            b.AddWallSign(FarWall(), 0.0f, 2.2f, 4.6f, 1.9f, flow);
        }
        if (mode_ == Mode::Device) BuildDeviceSigns(b);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        if (mode_ == Mode::Viewport && S().graphics != nullptr) {
            resettingToken_ = S().graphics->DeviceResetting.Add([this](System::Object*, const System::EventArgs&) { ++resettingCount_; });
            resetToken_ = S().graphics->DeviceReset.Add([this](System::Object*, const System::EventArgs&) { ++resetCount_; });
            if (S().device != nullptr) lostToken_ = S().device->DeviceLost.Add([this](System::Object*, const System::EventArgs&) { ++lostCount_; });
            preparingToken_ = S().graphics->PreparingDeviceSettings.Add([this](System::Object*, const PreparingDeviceSettingsEventArgs& e) {
                ++preparingCount_;
                const PresentationParameters& pp = e.getGraphicsDeviceInformationProperty().getPresentationParametersProperty();
                preparingNote_ = std::to_string(pp.getBackBufferWidthProperty()) + " x " + std::to_string(pp.getBackBufferHeightProperty()) + ", " + (pp.getIsFullScreenProperty() ? "full screen" : "windowed");
            });
        }
        if (mode_ == Mode::Components && S().game != nullptr) {
            addedToken_ = S().game->getComponentsProperty().ComponentAdded.Add([this](System::Object*, const GameComponentCollectionEventArgs& e) {
                ++addedCount_;
                lastComponentEventHadComponent_ = e.getGameComponentProperty() != nullptr;
            });
            removedToken_ = S().game->getComponentsProperty().ComponentRemoved.Add([this](System::Object*, const GameComponentCollectionEventArgs& e) {
                ++removedCount_;
                lastComponentEventHadComponent_ = e.getGameComponentProperty() != nullptr;
            });
        }
        if (mode_ == Mode::GameTimeLab) graph_ = std::make_unique<LiveScreen>(S(), 640, 320);
        if (mode_ == Mode::Viewport) graph_ = std::make_unique<LiveScreen>(S(), 640, 360);
        if (mode_ == Mode::Content && S().game != nullptr) {
            content_ = std::make_unique<Content::ContentManager>(&S().game->getServicesProperty(), S().contentRoot);
            LoadSet();   // start with something to see: the twelve textures are on the wall when you arrive
        }
        if (mode_ == Mode::Components && S().game != nullptr) {
            Game& g = *S().game;
            const Color cols[3] = {Color(190, 50, 50, 230), Color(50, 160, 70, 230), Color(60, 90, 210, 230)};
            const char* names[3] = {"Red component", "Green component", "Blue component"};
            for (int i = 0; i < 3; ++i) {
                auto c = std::make_shared<BadgeComponent>(g, S(), names[i], cols[i], 30 + i * 70, 330 + i * 40);
                c->setDrawOrderProperty(i + 1);
                c->setVisibleProperty(false);   // shown only while the visitor is in this room
                g.getComponentsProperty().Add(c);
                components_.push_back(c);
            }
        }
        if (mode_ == Mode::GameTimeLab && S().mutableStats != nullptr) S().mutableStats->injectedDelayMs = delay_;
    }

    void OnLeave() override
    {
        if (mode_ == Mode::GameTimeLab && S().mutableStats != nullptr) S().mutableStats->injectedDelayMs = 0.0f;   // never leave the museum slowed down
        for (auto& c : components_) c->setVisibleProperty(false);
    }
    void OnEnter() override
    {
        if (mode_ == Mode::GameTimeLab && S().mutableStats != nullptr) S().mutableStats->injectedDelayMs = delay_;
        for (std::size_t i = 0; i < components_.size(); ++i) components_[i]->setVisibleProperty(visible_[i]);
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        ++frames_;
        if (mode_ == Mode::GameTimeLab) {
            // Time-based: position follows the clock. Frame-based: a fixed step per Update call.
            timeBall_ = std::fmod(timeBall_ + 2.0f * ctx.dt, 8.0f);
            frameBall_ = std::fmod(frameBall_ + 0.0333f, 8.0f);
        }
        if (mode_ == Mode::Viewport && S().graphics != nullptr) {
            GraphicsDeviceManager& g = *S().graphics;
            bool apply = false;
            if (pendingW_ > 0 && (g.getPreferredBackBufferWidthProperty() != pendingW_ || g.getPreferredBackBufferHeightProperty() != pendingH_)) {
                g.setPreferredBackBufferWidthProperty(pendingW_);
                g.setPreferredBackBufferHeightProperty(pendingH_);
                apply = true;
            }
            if (pendingVsync_ >= 0) {
                g.setSynchronizeWithVerticalRetraceProperty(pendingVsync_ == 1);
                pendingVsync_ = -1;
                apply = true;
            }
            if (apply) g.ApplyChanges();
            if (pendingToggleFull_) {
                pendingToggleFull_ = false;
                g.ToggleFullScreen();
            }
            if (pendingPresent_) {
                pendingPresent_ = false;
                try {
                    S().device->Present();
                    presentStatus_ = "succeeded " + std::to_string(++manualPresentCount_) + " time(s)";
                    Museum::Log("app", "manual GraphicsDevice.Present: %s", presentStatus_.c_str());
                } catch (const std::exception& e) {
                    presentStatus_ = std::string("refused: ") + std::string(e.what()).substr(0, 48);
                    Museum::Log("app", "manual GraphicsDevice.Present: %s", presentStatus_.c_str());
                }
            }
        }
        if (mode_ == Mode::Components) {
            for (std::size_t i = 0; i < components_.size(); ++i) visible_[i] = visible_[i];
            if (ctx.playerInRoom != inRoom_) {
                inRoom_ = ctx.playerInRoom;
                for (std::size_t i = 0; i < components_.size(); ++i) components_[i]->setVisibleProperty(inRoom_ && visible_[i]);
            }
        }
    }

    void RenderOffscreen(const DrawContext&) override
    {
        if (mode_ == Mode::Viewport && graph_) {
            DrawViewportDiagram();
            return;
        }
        if (mode_ != Mode::GameTimeLab || !graph_) return;
        graph_->Render(Color(12, 16, 28, 255), [this](SpriteBatch& sb, int w, int h) {
            Texture2D& white = *S().whiteTexture;
            const float* hist = S().stats->frameHistory;
            sb.DrawString(*S().font, "ElapsedGameTime per Update (ms)", Vector2(10, 6), Color(255, 226, 150, 255), 0.0f, Vector2(0, 0), 0.8f, SpriteEffects::None, 0.0f);
            const float top = 40.0f, bottom = static_cast<float>(h) - 26.0f, scale = (bottom - top) / 66.7f;
            for (const float guide : {16.7f, 33.3f, 50.0f}) {
                const int y = static_cast<int>(bottom - guide * scale);
                sb.Draw(white, Rectangle(0, y, w, 1), Color(70, 80, 110, 255));
                sb.DrawString(*S().font, std::to_string(static_cast<int>(guide + 0.5f)), Vector2(4, static_cast<float>(y) - 14.0f), Color(130, 140, 170, 255), 0.0f, Vector2(0, 0), 0.5f, SpriteEffects::None, 0.0f);
            }
            for (int i = 0; i < 120; ++i) {
                const float v = std::min(hist[i], 66.0f);
                const int bh = std::max(1, static_cast<int>(v * scale));
                const Color c = v > 33.0f ? Color(255, 110, 90, 255) : (v > 20.0f ? Color(255, 210, 90, 255) : Color(110, 220, 140, 255));
                sb.Draw(white, Rectangle(40 + i * 5, static_cast<int>(bottom) - bh, 4, bh), c);
            }
        });
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (mode_ == Mode::GameTimeLab) {
            // Two lanes: the time-based ball and the frame-based ball.
            for (int lane = 0; lane < 2; ++lane) {
                const float z = -1.2f + static_cast<float>(lane) * 2.4f;
                const Vector3 a = R().ToWorld(-3.0f, 0.5f, z), b2 = R().ToWorld(3.0f, 0.5f, z);
                gizmos_.Line(a, b2, Color(140, 150, 190, 255));
                const float x = (lane == 0 ? timeBall_ : frameBall_) / 8.0f * 6.0f - 3.0f;
                gizmos_.SolidSphere(ctx, R().ToWorld(x, 0.5f, z), 0.22f, lane == 0 ? kGreen : kRed);
                gizmos_.Label(R().ToWorld(-3.0f, 1.0f, z), lane == 0 ? "time-based" : "frame-based", ToColor(lane == 0 ? kGreen * 1.3f : kRed * 1.3f), 0.55f);
            }
            if (graph_ && graph_->HasContent()) {
                RenderStates::SetOpaque3D(ctx.device);
                { Vector3 o, u, v; FarQuad(0.0f, 1.2f, 4.6f, 2.3f, o, u, v); QuadDraw::Textured(ctx, graph_->Texture(), o, u, v); }
            }
        } else if (mode_ == Mode::Viewport) {
            if (graph_ && graph_->HasContent()) {
                RenderStates::SetOpaque3D(ctx.device);
                Vector3 o, u, v;
                FarQuad(0.0f, 1.0f, 6.4f, 3.6f, o, u, v);
                QuadDraw::Textured(ctx, graph_->Texture(), o, u, v);
            }
        } else if (mode_ == Mode::Content) {
            for (std::size_t i = 0; i < held_.size(); ++i) {
                const int col = static_cast<int>(i % 6), row = static_cast<int>(i / 6);
                RenderStates::SetOpaque3D(ctx.device);
                { Vector3 o, u, v; FarQuad(-2.6f + static_cast<float>(col) * 1.05f, 1.2f + static_cast<float>(row) * 1.05f, 1.0f, 1.0f, o, u, v); QuadDraw::Textured(ctx, held_[i].get(), o, u, v); }
            }
        }
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        switch (mode_) {
            case Mode::Lifecycle: return "Game lifecycle";
            case Mode::GameTimeLab: return "GameTime and frame rate";
            case Mode::Device: return "GraphicsDevice and renderer";
            case Mode::Viewport: return "Back buffer, viewport, fullscreen";
            case Mode::Content: return "ContentManager lifetime";
            default: return "Game components";
        }
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Lifecycle: return {"The museum itself is a CNA Game subclass: Initialize and LoadContent run once, then the loop calls", "Update and Draw. With a fixed time step Update may run several times per Draw to catch up (and", "IsRunningSlowly turns on); a variable step calls each once per frame. Try the hitch button."};
            case Mode::GameTimeLab: return {"Movement should depend on elapsed time, not on how many frames ran. The green ball moves by", "2 m/s * dt and always takes four seconds; the red one moves a fixed step per Update, so a slow machine", "(add delay) drags it. The graph shows ElapsedGameTime for every recent Update."};
            case Mode::Device: return {"These panels are generated from the live GraphicsDevice: the renderer in use, the adapter, what the", "device supports (GraphicsCapability) and the detailed renderer capability profile. They differ", "between renderers: compare screenshots of this room across renderers."};
            case Mode::Viewport: return {"GraphicsDeviceManager holds the preferred back buffer size and fullscreen state; ApplyChanges()", "recreates the back buffer. The Viewport is the part drawn into; the Present button explicitly sends the completed previous frame to the display."};
            case Mode::Content: return {"A ContentManager loads assets by name and caches them until Unload(). This room owns its own manager:", "load a dozen textures, load them again (a cache hit is much faster), unload, or ask for a file that is", "missing: the failure is a ContentLoadException, shown here instead of crashing."};
            default: return {"GameComponents let a Game be built from parts: each has its own Update and (if drawable) Draw, ordered by", "UpdateOrder and DrawOrder. Three badge components are registered with Game.Components while you are here;", "change their DrawOrder to reorder the overlapping badges, hide one, or disable updates."};
        }
    }

    [[nodiscard]] std::string BackBufferText() const
    {
        const PresentationParameters& pp = S().device->getPresentationParametersProperty();
        return std::to_string(pp.getBackBufferWidthProperty()) + " x " + std::to_string(pp.getBackBufferHeightProperty()) + (S().graphics && S().graphics->getIsFullScreenProperty() ? "  fullscreen" : "  windowed");
    }
    // The back buffer, the viewport inside it and the window that shows it, to scale, with SpriteBatch's pixel grid.
    void DrawViewportDiagram()
    {
        // Read the viewport before the live screen's own render target replaces it.
        const Viewport vp = S().device->getViewportProperty();
        graph_->Render(Color(12, 16, 28, 255), [this, vp](SpriteBatch& sb, int w, int h) {
            if (S().font == nullptr) return;
            Texture2D& white = *S().whiteTexture;
            const PresentationParameters& pp = S().device->getPresentationParametersProperty();
            const float bw = static_cast<float>(std::max(1, pp.getBackBufferWidthProperty())), bh = static_cast<float>(std::max(1, pp.getBackBufferHeightProperty()));
            const float scale = std::min((static_cast<float>(w) - 80.0f) / bw, (static_cast<float>(h) - 110.0f) / bh);
            const float ox = (static_cast<float>(w) - bw * scale) * 0.5f, oy = 52.0f;
            const auto rect = [&](float x, float y, float rw, float rh, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), std::max(1, static_cast<int>(rw)), std::max(1, static_cast<int>(rh))), c); };
            const auto text = [&](const std::string& t, float x, float y, Color c, float sc) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), sc, SpriteEffects::None, 0.0f); };
            text("BackBuffer " + std::to_string(pp.getBackBufferWidthProperty()) + " x " + std::to_string(pp.getBackBufferHeightProperty()) + "   (SpriteBatch pixels, origin top-left)", 10, 8, Color(255, 226, 150, 255), 0.7f);
            rect(ox - 2, oy - 2, bw * scale + 4, bh * scale + 4, Color(90, 100, 140, 255));
            rect(ox, oy, bw * scale, bh * scale, Color(26, 34, 56, 255));
            for (int x = 100; x < static_cast<int>(bw); x += 100) rect(ox + static_cast<float>(x) * scale, oy, 1, bh * scale, Color(50, 62, 92, 255));
            for (int y = 100; y < static_cast<int>(bh); y += 100) rect(ox, oy + static_cast<float>(y) * scale, bw * scale, 1, Color(50, 62, 92, 255));
            rect(ox + static_cast<float>(vp.getXProperty()) * scale, oy + static_cast<float>(vp.getYProperty()) * scale, static_cast<float>(vp.getWidthProperty()) * scale, 3, Color(110, 230, 150, 255));
            rect(ox + static_cast<float>(vp.getXProperty()) * scale, oy + static_cast<float>(vp.getYProperty() + vp.getHeightProperty()) * scale - 3, static_cast<float>(vp.getWidthProperty()) * scale, 3, Color(110, 230, 150, 255));
            rect(ox + static_cast<float>(vp.getXProperty()) * scale, oy + static_cast<float>(vp.getYProperty()) * scale, 3, static_cast<float>(vp.getHeightProperty()) * scale, Color(110, 230, 150, 255));
            rect(ox + static_cast<float>(vp.getXProperty() + vp.getWidthProperty()) * scale - 3, oy + static_cast<float>(vp.getYProperty()) * scale, 3, static_cast<float>(vp.getHeightProperty()) * scale, Color(110, 230, 150, 255));
            text("(0, 0)", ox + 6, oy + 4, Color(255, 200, 120, 255), 0.55f);
            text("(" + std::to_string(pp.getBackBufferWidthProperty()) + ", " + std::to_string(pp.getBackBufferHeightProperty()) + ")", ox + bw * scale - 90, oy + bh * scale - 22, Color(255, 200, 120, 255), 0.55f);
            text("Viewport " + std::to_string(vp.getXProperty()) + "," + std::to_string(vp.getYProperty()) + "  " + std::to_string(vp.getWidthProperty()) + " x " + std::to_string(vp.getHeightProperty()) +
                     "   aspect " + Fmt(vp.getAspectRatioProperty()) + "   (green)",
                 ox, oy + bh * scale + 10, Color(130, 235, 165, 255), 0.62f);
            text("grid: 100 pixels", ox, oy + bh * scale + 34, Color(130, 140, 170, 255), 0.55f);
        });
    }

    [[nodiscard]] std::string DepthRangeText() const
    {
        Viewport v = S().device->getViewportProperty();
        v.setMinDepthProperty(minDepth_);
        v.setMaxDepthProperty(maxDepth_);
        const Matrix view = Matrix::CreateLookAt(Vector3(0, 0, 0), Vector3(0, 0, -1), Vector3::Up);
        const Matrix proj = Matrix::CreatePerspectiveFieldOfView(1.0f, v.getAspectRatioProperty(), 1.0f, 100.0f);
        const Vector3 nearPoint = v.Project(Vector3(0, 0, -1.0f), proj, view, Matrix::getIdentityProperty());
        const Vector3 farPoint = v.Project(Vector3(0, 0, -100.0f), proj, view, Matrix::getIdentityProperty());
        return "Z " + Fmt(nearPoint.Z) + " / " + Fmt(farPoint.Z) + "  (range " + Fmt(minDepth_) + " to " + Fmt(maxDepth_) + ")";
    }
    [[nodiscard]] std::string ViewportText() const
    {
        const Viewport v = S().device->getViewportProperty();
        return std::to_string(v.getXProperty()) + "," + std::to_string(v.getYProperty()) + "  " + std::to_string(v.getWidthProperty()) + " x " + std::to_string(v.getHeightProperty());
    }

    void SetVisible(std::size_t i, bool v)
    {
        if (i >= visible_.size()) return;
        visible_[i] = v;
        if (i < components_.size()) components_[i]->setVisibleProperty(inRoom_ && v);
    }
    void SetDrawOrder(std::size_t i, int v)
    {
        if (i < components_.size()) components_[i]->setDrawOrderProperty(v);
    }

    void LoadSet()
    {
        if (!content_) return;
        static const char* names[] = {"brick", "stone", "marble", "wood", "tiles", "carpet", "plaster", "metal", "grass", "cobble", "concrete", "gradient"};
        const auto t0 = std::chrono::steady_clock::now();
        try {
            std::vector<std::unique_ptr<Texture2D>> loaded;
            for (const char* n : names) loaded.push_back(std::make_unique<Texture2D>(content_->Load<Texture2D>(std::string("Textures/") + n)));
            held_ = std::move(loaded);
            lastError_ = "-";
        } catch (const std::exception& e) {
            lastError_ = e.what();
            ReportError(std::string("loading the 12 museum textures: ") + e.what());
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.2f ms for 12 loads", ms);
        lastLoad_ = buf;
    }
    void UnloadSet()
    {
        held_.clear();
        if (content_) content_->Unload();
        lastLoad_ = "unloaded";
    }
    void LoadMissing()
    {
        if (!content_) return;
        try {
            (void)content_->Load<Texture2D>("Textures/this-asset-does-not-exist");
            lastError_ = "unexpectedly loaded";
        } catch (const Content::ContentLoadException& e) {
            lastError_ = std::string("ContentLoadException: ") + e.what();
        } catch (const std::exception& e) {
            lastError_ = std::string("exception: ") + e.what();
        }
        if (lastError_.size() > 60) lastError_.resize(60);
    }

    // The device room: static panels written once from the live device.
    void BuildDeviceSigns(RoomBuilder& b)
    {
        GraphicsDevice& dev = *S().device;
        const auto yes = [](bool v) { return std::string(v ? "✓  " : "×  "); };
        SignSpec info;
        info.leftAlign = true;
        info.pixelWidth = 900;
        info.pixelHeight = 640;
        info.lines.push_back({"Device and adapter", true, Kit::kGold});
        const auto add = [&](const std::string& t) { info.lines.push_back({t, false, Kit::kText}); };
        add("Renderer: " + std::string(dev.GetGraphicsRendererName()));
        add("Profile: " + std::string(dev.getGraphicsProfileProperty() == GraphicsProfile::HiDef ? "HiDef" : "Reach"));
        // The defaults XNA takes from outside the code, seen before the museum set anything (MuseumGame's constructor).
        const auto profileName = [](int p) { return std::string(p == static_cast<int>(GraphicsProfile::HiDef) ? "HiDef" : p == static_cast<int>(GraphicsProfile::Reach) ? "Reach" : "?"); };
        const std::string declaredTitle = CNA::GetAssemblyTitleEXT();
        const std::string declaredProfile = profileName(static_cast<int>(CNA::GetProjectGraphicsProfileEXT()));
        add("AssemblyTitleEXT \"" + declaredTitle + "\" -> first Window.Title \"" + S().startupWindowTitle + "\"");
        add("ProjectGraphicsProfileEXT " + declaredProfile + " -> first GraphicsDeviceManager profile " + profileName(S().startupGraphicsProfile));
        Museum::Log("check", "%s Window.Title before the game sets one is the assembly title: %s (expected \"%s\", got \"%s\")", R().key.c_str(),
                    S().startupWindowTitle == declaredTitle && !declaredTitle.empty() ? "PASS" : "FAIL", declaredTitle.c_str(), S().startupWindowTitle.c_str());
        Museum::Log("check", "%s GraphicsDeviceManager.GraphicsProfile before the game sets one is the project profile: %s (expected %s, got %s)", R().key.c_str(),
                    profileName(S().startupGraphicsProfile) == declaredProfile ? "PASS" : "FAIL", declaredProfile.c_str(), profileName(S().startupGraphicsProfile).c_str());
        const PresentationParameters& pp = dev.getPresentationParametersProperty();
        add("Back buffer: " + std::to_string(pp.getBackBufferWidthProperty()) + " x " + std::to_string(pp.getBackBufferHeightProperty()));
        add("MSAA samples: " + std::to_string(pp.getMultiSampleCountProperty()));
        add("Max texture size: " + std::to_string(dev.GetMaxTextureDimension()));
        try {
            const GraphicsAdapter a = GraphicsAdapter::getDefaultAdapterProperty();
            add("Adapter: " + a.getDescriptionProperty());
            add("Display: " + std::to_string(a.getCurrentDisplayModeProperty().getWidthProperty()) + " x " + std::to_string(a.getCurrentDisplayModeProperty().getHeightProperty()));
        } catch (const std::exception&) {
            add("Adapter: (no display adapter reported)");
        }
        b.AddWallSign(FarWall(), -3.2f, 2.3f, 3.6f, 2.4f, info);

        using CNA::GraphicsCapability;
        SignSpec caps;
        caps.leftAlign = true;
        caps.pixelWidth = 900;
        caps.pixelHeight = 660;
        caps.lines.push_back({"GraphicsCapability queries", true, Kit::kGold});
        const std::pair<GraphicsCapability, const char*> list[] = {
            {GraphicsCapability::ThreeD, "ThreeD"}, {GraphicsCapability::DepthStencilBuffer, "DepthStencilBuffer"}, {GraphicsCapability::MultiSampleAntiAliasing, "MultiSampleAntiAliasing"},
            {GraphicsCapability::MultipleRenderTargets, "MultipleRenderTargets"}, {GraphicsCapability::AnisotropicFiltering, "AnisotropicFiltering"}, {GraphicsCapability::WireFrame, "WireFrame"},
            {GraphicsCapability::OcclusionQuery, "OcclusionQuery"}, {GraphicsCapability::CustomEffects, "CustomEffects"}, {GraphicsCapability::Texture3D, "Texture3D"},
            {GraphicsCapability::MultiStreamVertexInput, "MultiStreamVertexInput"}, {GraphicsCapability::Instancing, "Instancing"}, {GraphicsCapability::StencilBuffer, "StencilBuffer"},
            {GraphicsCapability::AdditiveBlending, "AdditiveBlending"}, {GraphicsCapability::CompiledEffects, "CompiledEffects"}, {GraphicsCapability::FloatRenderTargets, "FloatRenderTargets"},
            {GraphicsCapability::HalfFloatRenderTargets, "HalfFloatRenderTargets"}, {GraphicsCapability::HalfFloatTextureLinearFiltering, "HalfFloatTextureLinearFiltering"}};
        for (const auto& [cap, name] : list) caps.lines.push_back({yes(dev.SupportsCapability(cap)) + name, false, dev.SupportsCapability(cap) ? Color(150, 255, 170, 255) : Color(255, 150, 140, 255)});
        b.AddWallSign(FarWall(), 3.2f, 2.3f, 3.6f, 3.0f, caps);

        const CNA::RendererCapabilityProfile& profile = dev.GetRendererCapabilityProfileEXT();
        SignSpec feat;
        feat.leftAlign = true;
        feat.pixelWidth = 900;
        feat.pixelHeight = 760;
        feat.lines.push_back({"Renderer capability profile (CNA extension)", true, Kit::kCnaExt});
        for (const CNA::RendererFeature f : CNA::AllRendererFeatures()) {
            const auto support = profile.GetFeature(f).support;
            const char* tag = support == CNA::RendererFeatureSupport::Supported ? "✓ " : (support == CNA::RendererFeatureSupport::Restricted ? "~ " : (support == CNA::RendererFeatureSupport::Unsupported ? "× " : "? "));
            feat.lines.push_back({std::string(tag) + std::string(CNA::GetRendererFeatureName(f)), false,
                                  support == CNA::RendererFeatureSupport::Supported ? Color(150, 255, 170, 255) : (support == CNA::RendererFeatureSupport::Restricted ? Color(255, 220, 120, 255) : Color(255, 150, 140, 255))});
        }
        b.AddWallSign(Wall::West, 0.0f, 2.4f, 4.2f, 3.6f, feat);
    }

    Mode mode_;
    std::size_t preparingToken_ = 0;
    int preparingCount_ = 0;
    std::string preparingNote_;
    std::size_t resetToken_ = 0, resettingToken_ = 0, addedToken_ = 0, removedToken_ = 0, lostToken_ = 0;
    float minDepth_ = 0.0f, maxDepth_ = 1.0f;
    int resetCount_ = 0, resettingCount_ = 0, addedCount_ = 0, removedCount_ = 0, lostCount_ = 0;
    bool lastComponentEventHadComponent_ = false;
    Gizmos gizmos_;
    float time_ = 0.0f, delay_ = 0.0f, timeBall_ = 0.0f, frameBall_ = 0.0f;
    long long frames_ = 0;
    std::unique_ptr<LiveScreen> graph_;
    std::unique_ptr<Content::ContentManager> content_;
    std::vector<std::unique_ptr<Texture2D>> held_;
    std::string lastLoad_ = "-", lastError_ = "-";
    std::vector<std::shared_ptr<BadgeComponent>> components_;
    std::array<bool, 3> visible_{true, true, true};
    bool inRoom_ = false;
    int pendingW_ = 0, pendingH_ = 0, pendingVsync_ = -1;
    int manualPresentCount_ = 0;
    bool pendingToggleFull_ = false, pendingPresent_ = false;
    std::string presentStatus_ = "not called manually";
};

}  // namespace

std::unique_ptr<Exhibit> MakeAppLab(const RoomDef& room, Services& services)
{
    return std::make_unique<AppLab>(room, services);
}
