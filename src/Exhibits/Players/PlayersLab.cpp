// Players laboratory: CNA's GamerServices. Who is signed in (two local, offline profiles, set up by Main.cpp), what the
// framework says about them (presence, privileges, defaults, profile), the museum's own achievements, and the Guide.
// Without CNA's networking and GamerServices (-DMUSEUM_NETWORKING=OFF) every room here says so instead.
#include <algorithm>
#include <cstdio>
#include <limits>
#include <memory>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Exhibits/Players/MuseumAchievements.hpp"

#ifdef MUSEUM_NETWORKING
#include "Microsoft/Xna/Framework/GamerServices/AchievementCollection.hpp"
#include "Microsoft/Xna/Framework/GamerServices/Gamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/GamerServicesDispatcher.hpp"
#include "Microsoft/Xna/Framework/GamerServices/GamerPresence.hpp"
#include "Microsoft/Xna/Framework/GamerServices/GamerProfile.hpp"
#include "Microsoft/Xna/Framework/GamerServices/Guide.hpp"
#include "Microsoft/Xna/Framework/GamerServices/LeaderboardEntry.hpp"
#include "Microsoft/Xna/Framework/GamerServices/LeaderboardIdentity.hpp"
#include "Microsoft/Xna/Framework/GamerServices/LeaderboardKey.hpp"
#include "Microsoft/Xna/Framework/GamerServices/LeaderboardOutcome.hpp"
#include "Microsoft/Xna/Framework/GamerServices/LeaderboardReader.hpp"
#include "Microsoft/Xna/Framework/GamerServices/LeaderboardWriter.hpp"
#include "Microsoft/Xna/Framework/GamerServices/MessageBoxIcon.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamerCollection.hpp"
#include "Microsoft/Xna/Framework/Net/LocalNetworkGamer.hpp"
#include "Microsoft/Xna/Framework/Net/NetworkSession.hpp"
#include "Microsoft/Xna/Framework/Net/NetworkSessionType.hpp"
#include "Microsoft/Xna/Framework/Net/WriteLeaderboardsEventArgs.hpp"
#include "Microsoft/Xna/Framework/PlayerIndex.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Players, Achievements, Guide, Leaderboards };

Mode ParseMode(const std::string& s)
{
    if (s == "achievements") return Mode::Achievements;
    if (s == "guide") return Mode::Guide;
    if (s == "leaderboards") return Mode::Leaderboards;
    return Mode::Players;
}

#ifdef MUSEUM_NETWORKING
using namespace Microsoft::Xna::Framework::GamerServices;
using namespace Microsoft::Xna::Framework::Net;

// Every GamerPresenceMode XNA defines (60); the first eight are the ones the room starts with.
constexpr GamerPresenceMode kPresences[] = {
    GamerPresenceMode::None, GamerPresenceMode::SinglePlayer, GamerPresenceMode::Multiplayer, GamerPresenceMode::LocalCoOp,
    GamerPresenceMode::ExplorationMode, GamerPresenceMode::PuzzleMode, GamerPresenceMode::AtMenu, GamerPresenceMode::Paused,
    GamerPresenceMode::LocalVersus, GamerPresenceMode::OnlineCoOp, GamerPresenceMode::OnlineVersus, GamerPresenceMode::VersusComputer,
    GamerPresenceMode::Stage, GamerPresenceMode::Level, GamerPresenceMode::CoOpStage, GamerPresenceMode::CoOpLevel, GamerPresenceMode::ArcadeMode,
    GamerPresenceMode::CampaignMode, GamerPresenceMode::ChallengeMode, GamerPresenceMode::PracticeMode, GamerPresenceMode::ScenarioMode,
    GamerPresenceMode::StoryMode, GamerPresenceMode::SurvivalMode, GamerPresenceMode::TutorialMode, GamerPresenceMode::DifficultyEasy,
    GamerPresenceMode::DifficultyMedium, GamerPresenceMode::DifficultyHard, GamerPresenceMode::DifficultyExtreme, GamerPresenceMode::Score,
    GamerPresenceMode::VersusScore, GamerPresenceMode::Winning, GamerPresenceMode::Losing, GamerPresenceMode::ScoreIsTied,
    GamerPresenceMode::Outnumbered, GamerPresenceMode::OnARoll, GamerPresenceMode::InCombat, GamerPresenceMode::BattlingBoss,
    GamerPresenceMode::TimeAttack, GamerPresenceMode::TryingForRecord, GamerPresenceMode::FreePlay, GamerPresenceMode::WastingTime,
    GamerPresenceMode::StuckOnAHardBit, GamerPresenceMode::NearlyFinished, GamerPresenceMode::LookingForGames, GamerPresenceMode::WaitingForPlayers,
    GamerPresenceMode::WaitingInLobby, GamerPresenceMode::SettingUpMatch, GamerPresenceMode::PlayingWithFriends, GamerPresenceMode::StartingGame,
    GamerPresenceMode::GameOver, GamerPresenceMode::WonTheGame, GamerPresenceMode::ConfiguringSettings, GamerPresenceMode::CustomizingPlayer,
    GamerPresenceMode::EditingLevel, GamerPresenceMode::InGameStore, GamerPresenceMode::WatchingCutscene, GamerPresenceMode::WatchingCredits,
    GamerPresenceMode::PlayingMinigame, GamerPresenceMode::FoundSecret, GamerPresenceMode::CornflowerBlue,};
constexpr const char* kPresenceNames[] = {
    "None", "SinglePlayer", "Multiplayer", "LocalCoOp", "ExplorationMode", "PuzzleMode", "AtMenu", "Paused", "LocalVersus", "OnlineCoOp",
    "OnlineVersus", "VersusComputer", "Stage", "Level", "CoOpStage", "CoOpLevel", "ArcadeMode", "CampaignMode", "ChallengeMode", "PracticeMode",
    "ScenarioMode", "StoryMode", "SurvivalMode", "TutorialMode", "DifficultyEasy", "DifficultyMedium", "DifficultyHard", "DifficultyExtreme",
    "Score", "VersusScore", "Winning", "Losing", "ScoreIsTied", "Outnumbered", "OnARoll", "InCombat", "BattlingBoss", "TimeAttack",
    "TryingForRecord", "FreePlay", "WastingTime", "StuckOnAHardBit", "NearlyFinished", "LookingForGames", "WaitingForPlayers", "WaitingInLobby",
    "SettingUpMatch", "PlayingWithFriends", "StartingGame", "GameOver", "WonTheGame", "ConfiguringSettings", "CustomizingPlayer", "EditingLevel",
    "InGameStore", "WatchingCutscene", "WatchingCredits", "PlayingMinigame", "FoundSecret", "CornflowerBlue",};

const char* PrivilegeName(GamerPrivilegeSetting s)
{
    switch (s) {
        case GamerPrivilegeSetting::Blocked: return "Blocked";
        case GamerPrivilegeSetting::FriendsOnly: return "FriendsOnly";
        default: return "Everyone";
    }
}
const char* ZoneName(GamerZone z)
{
    switch (z) {
        case GamerZone::Family: return "Family";
        case GamerZone::Pro: return "Pro";
        case GamerZone::Recreation: return "Recreation";
        default: return "Unknown";
    }
}
const char* PresenceName(GamerPresenceMode m)
{
    for (std::size_t i = 0; i < std::size(kPresences); ++i)
        if (kPresences[i] == m) return kPresenceNames[i];
    return "(other)";
}
std::string YesNo(bool v) { return v ? "yes" : "no"; }
#endif

class PlayersLab final : public ScreenExhibit {
public:
    PlayersLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))) {}

    ~PlayersLab() override { Unsubscribe(); }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_NETWORKING
        switch (mode_) {
            case Mode::Players:
                p.AddReadout("Gamer.SignedInGamers.Count", [this] { return std::to_string(Count()); });
                p.AddChoice("Presence of the first player (all modes)", std::vector<std::string>(std::begin(kPresenceNames), std::end(kPresenceNames)), 4,
                            [this](int v) { SetPresence(v); });
                p.AddChoice("Presence text for the service (SetPresenceModeStringEXT)", {"(not set)", "Browsing the museum", "Reading a plaque on floor 4", "In the shader lab"}, 0, [this](int v) { SetPresenceText(v); });
                p.AddReadout("Presence text sent", [this] { return presenceText_; });
                p.AddSlider("Presence value", 0.0f, 9.0f, 0.0f, [this](float v) { SetPresenceValue(static_cast<int>(v)); }, "%.0f", 1.0f);
                p.AddReadout("SignedIn / SignedOut events", [this] { return std::to_string(signedIn_) + " / " + std::to_string(signedOut_) + " since this room loaded"; });
                p.AddReadout("Profiles", [] { return std::string("local and offline (no CNA account service)"); });
                break;
            case Mode::Achievements:
                p.AddChoice("Achievement", {"first-steps", "explorer", "cartographer", "every-floor", "elevator", "completionist"}, 0, [this](int v) { selected_ = v; });
                p.AddButton("AwardAchievement(key)", [this] { Award(selected_); });
                p.AddButton("Award them all", [this] { for (int i = 0; i < 6; ++i) Award(i); });
                p.AddReadout("Earned", [this] { return std::to_string(earned_) + " of " + std::to_string(shown_.size()) + ", " + std::to_string(score_) + " points"; });
                p.AddReadout("Catalog", [this] { return std::string(shown_.empty() ? "none found next to the executable" : "GamerServices/Achievements.json"); });
                p.AddReadout("Last award", [this] { return lastAward_; });
                break;
            case Mode::Guide:
                p.AddToggle("Guide.SimulateTrialMode", false, [](bool v) { Guide::setSimulateTrialModeProperty(v); });
                p.AddChoice("Guide.NotificationPosition", {"TopLeft", "TopCenter", "TopRight", "CenterLeft", "Center", "CenterRight", "BottomLeft", "BottomCenter", "BottomRight"}, 2,
                            [](int v) { Guide::setNotificationPositionProperty(static_cast<NotificationPosition>(v)); });
                screenSaverStart_ = Guide::getIsScreenSaverEnabledProperty();
                p.AddToggle("Guide.IsScreenSaverEnabled", screenSaverStart_, [](bool v) { Guide::setIsScreenSaverEnabledProperty(v); });
                p.AddButton("Guide.DelayNotifications(30 s)", [this] { Guide::DelayNotifications(System::TimeSpan::FromSeconds(30.0)); answer_ = "invitation prompts held back for 30 s"; });
                p.AddButton("Guide.BeginShowMessageBox", [this] { ShowMessageBox(); });
                p.AddButton("Guide.BeginShowKeyboardInput", [this] { ShowKeyboard(); });
                p.AddReadout("Guide.IsTrialMode", [] { return std::string(Guide::getIsTrialModeProperty() ? "true" : "false"); });
                p.AddReadout("GamerServicesDispatcher.IsInitialized", [] { return std::string(Microsoft::Xna::Framework::GamerServices::GamerServicesDispatcher::getIsInitializedProperty() ? "true" : "false"); });
                p.AddReadout("GamerServicesDispatcher.WindowHandle", [] { return std::string(Microsoft::Xna::Framework::GamerServices::GamerServicesDispatcher::getWindowHandleProperty() != 0 ? "set" : "0 (not set)"); });
                p.AddReadout("Guide.IsScreenSaverEnabled", [] { return std::string(Guide::getIsScreenSaverEnabledProperty() ? "true (the desktop may blank)" : "false"); });
                p.AddReadout("Guide.IsVisible", [] { return std::string(Guide::getIsVisibleProperty() ? "true (it owns the keyboard)" : "false"); });
                p.AddReadout("Answer", [this] { return answer_; });
                break;
            case Mode::Leaderboards:
                p.AddChoice("LeaderboardKey", {"BestScoreLifeTime", "BestScoreRecent", "BestTimeLifeTime", "BestTimeRecent"}, 0, [this](int v) { boardKey_ = v; dirty_ = true; });
                p.AddChoice("Gamer to write", {"first signed-in", "second signed-in"}, 0, [this](int v) { writer_ = v; });
                p.AddButton("Guide.ShowSignIn: an account scores on the service", [this] { try { Guide::ShowSignIn(1, true); Museum::Log("online", "ShowSignIn(1, true) opened"); } catch (const std::exception& e) { boardError_ = std::string(e.what()).substr(0, 60); } });
                p.AddButton("LeaderboardWriter: add 10 to the rating", [this] { AddRating(10); });
                p.AddButton("LeaderboardWriter: add 100 to the rating", [this] { AddRating(100); });
                p.AddButton("LeaderboardWriter: reset the rating", [this] { AddRating(std::numeric_limits<long long>::min()); });
                p.AddChoice("Page size", {"1", "2", "5"}, 0, [this](int v) { pageSize_ = v == 0 ? 1 : v == 1 ? 2 : 5; dirty_ = true; });
                p.AddToggle("Read around the first gamer (pivot)", false, [this](bool v) { pivot_ = v; dirty_ = true; });
                p.AddButton("LeaderboardReader.PageDown", [this] { Page(true); });
                p.AddButton("LeaderboardReader.PageUp", [this] { Page(false); });
                p.AddButton("Log the board from the top (LeaderboardReader.Read)", [this] { LogBoard(); });
                p.AddReadout("PageStart / TotalLeaderboardSize", [this] { return std::to_string(pageStart_) + " / " + std::to_string(total_); });
                p.AddReadout("CanPageDown / CanPageUp", [this] { return YesNo(canDown_) + " / " + YesNo(canUp_); });
                p.AddReadout("Final Write*Leaderboard events", [this] { return leaderboardEvents_; });
                p.AddReadout("Store", [] { return std::string("CNA's local store, kept between runs"); });
                break;
        }
#else
        p.AddReadout("GamerServices", [] { return std::string("not built into this museum"); });
#endif
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
#ifdef MUSEUM_NETWORKING
        if (mode_ == Mode::Players) {
            inToken_ = SignedInGamer::SignedIn.Add([this](System::Object*, const SignedInEventArgs& e) { ++signedIn_; LogSignEvent("SignedIn", e.getGamerProperty()); });
            outToken_ = SignedInGamer::SignedOut.Add([this](System::Object*, const SignedOutEventArgs& e) { ++signedOut_; LogSignEvent("SignedOut", e.getGamerProperty()); });
            subscribed_ = true;
        }
        Refresh();
#endif
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
#ifdef MUSEUM_NETWORKING
        refresh_ -= ctx.dt;
        const bool tick = refresh_ <= 0.0f;
        if (tick) {
            refresh_ = 0.5f;
            Refresh();
        }
        Poll();
        if (mode_ == Mode::Leaderboards && (dirty_ || tick)) {
            ReadBoard(dirty_);
            dirty_ = false;
        }
#endif
    }

    void OnLeave() override
    {
#ifdef MUSEUM_NETWORKING
        // A dialog left open would own the keyboard in every other room.
        if (Guide::getHasPendingMessageBoxEXTProperty()) Guide::ResetPendingMessageBoxForTestingEXT();
        if (Guide::getHasPendingKeyboardInputEXTProperty()) Guide::SimulateKeyboardInputCancelEXT();
        Guide::setSimulateTrialModeProperty(false);
        if (mode_ == Mode::Guide) Guide::setIsScreenSaverEnabledProperty(screenSaverStart_);
#endif
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(Title(), 24, 12, Kit::kGold, 1.0f);
#ifdef MUSEUM_NETWORKING
        switch (mode_) {
            case Mode::Players: DrawPlayers(rect, text); break;
            case Mode::Achievements: DrawAchievements(rect, text); break;
            case Mode::Guide: DrawGuide(rect, text); break;
            case Mode::Leaderboards: DrawBoard(rect, text); break;
        }
#else
        text("This museum was built without CNA's GamerServices and networking (-DMUSEUM_NETWORKING=OFF).", 24, 90, Kit::Rgba(255, 190, 150), 0.7f);
        text("Configure with -DMUSEUM_NETWORKING=ON to see signed-in players, achievements, the Guide and network sessions.", 24, 130, Kit::kDim, 0.6f);
        (void)rect;
#endif
        sb.End();
    }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Signed-in players", "Achievements", "The Guide", "Leaderboards"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Players: return {"GamerServices models the people playing: Gamer.SignedInGamers lists them, each SignedInGamer carries presence, privileges and defaults,", "and GetProfile returns the gamer score and reputation. CNA signs in two local, offline profiles at start-up so the rooms next door have two players to connect."};
            case Mode::Achievements: return {"An achievement is a key the game awards to the signed-in gamer; the name, text and points come from a catalog the title ships", "(GamerServices/Achievements.json). The museum awards these itself as you explore, and the awards persist in CNA's local store between runs."};
            case Mode::Leaderboards: return {"A leaderboard ranks gamers by a rating. LeaderboardWriter hands a gamer its own entry to set; LeaderboardReader pages through the ranked list or reads", "the page around one gamer. CNA keeps the ratings in its local store, so they are still here next time; the two signed-in profiles are the only gamers it can rank offline."};
            default: return {"The Guide is the platform's overlay: message boxes, text input, sign-in. While it is visible it owns the keyboard, controller and mouse and", "the game reads them released; the museum also stops moving you. Trial mode can be simulated to test a demo build's limits."};
        }
    }

#ifdef MUSEUM_NETWORKING
    [[nodiscard]] static int Count()
    {
        auto* gamers = Gamer::getSignedInGamersProperty();
        return gamers != nullptr ? gamers->getCountProperty() : 0;
    }
    [[nodiscard]] static SignedInGamer* First()
    {
        auto* gamers = Gamer::getSignedInGamersProperty();
        return gamers != nullptr && gamers->getCountProperty() > 0 ? (*gamers)[0] : nullptr;
    }

    void SetPresence(int v)
    {
        if (SignedInGamer* g = First()) g->getPresenceProperty().setPresenceModeProperty(kPresences[std::clamp(v, 0, static_cast<int>(std::size(kPresences)) - 1)]);
    }
    // A free-form presence string for an online service (friends on the Online Service wing see it); offline it only changes the gamer.
    void SetPresenceText(int v)
    {
        static const char* const texts[] = {"", "Browsing the museum", "Reading a plaque on floor 4", "In the shader lab"};
        SignedInGamer* g = First();
        if (g == nullptr) { presenceText_ = "no signed-in gamer"; return; }
        v = std::clamp(v, 0, 3);
        if (v == 0) { presenceText_ = "(not set)"; return; }
        g->getPresenceProperty().SetPresenceModeStringEXT(texts[v]);
        presenceText_ = std::string("\"") + texts[v] + "\"";
    }
    void SetPresenceValue(int v)
    {
        if (SignedInGamer* g = First()) g->getPresenceProperty().setPresenceValueProperty(v);
    }
    void Unsubscribe()
    {
        if (!subscribed_) return;
        try {
            SignedInGamer::SignedIn.Remove(inToken_);
            SignedInGamer::SignedOut.Remove(outToken_);
        } catch (const std::exception&) {}
        subscribed_ = false;
    }

    // Everything the tables show is read here, twice a second, never inside the draw.
    struct Card {
        std::string tag, display, index, presence, privileges, defaults, profile;
        bool guest = false, live = false;
    };
    // The events are written to the museum log too, so a scripted sign-out through the Guide leaves a witness.
    static void LogSignEvent(const char* event, SignedInGamer* gamer)
    {
        std::string tag = "?";
        try { if (gamer != nullptr) tag = gamer->getGamertagProperty(); } catch (const std::exception&) { tag = "(disposed gamer)"; }
        Museum::Log("players", "SignedInGamer.%s: %s", event, tag.c_str());
    }

    void Refresh()
    {
        try {
            cards_.clear();
            auto* gamers = Gamer::getSignedInGamersProperty();
            for (int i = 0; gamers != nullptr && i < gamers->getCountProperty(); ++i) {
                SignedInGamer* g = (*gamers)[i];
                Card c;
                c.tag = g->getGamertagProperty();
                c.display = g->getDisplayNameProperty();
                c.index = "PlayerIndex " + std::to_string(static_cast<int>(g->getPlayerIndexProperty()));
                c.guest = g->getIsGuestProperty();
                c.live = g->getIsSignedInToLiveProperty();
                c.presence = std::string(PresenceName(g->getPresenceProperty().getPresenceModeProperty())) + ", value " + std::to_string(g->getPresenceProperty().getPresenceValueProperty());
                const GamerPrivileges& pr = g->getPrivilegesProperty();
                c.privileges = std::string("communication ") + PrivilegeName(pr.getAllowCommunicationProperty()) + ", online sessions " + YesNo(pr.getAllowOnlineSessionsProperty()) + ", profile viewing " + PrivilegeName(pr.getAllowProfileViewingProperty());
                const GameDefaults& d = g->getGameDefaultsProperty();
                c.defaults = std::string("difficulty ") + std::to_string(static_cast<int>(d.getGameDifficultyProperty())) + ", invert Y " + YesNo(d.getInvertYAxisProperty()) + ", auto-aim " + YesNo(d.getAutoAimProperty()) +
                             ", sensitivity " + (d.getControllerSensitivityProperty() == ControllerSensitivity::Low ? "low" : d.getControllerSensitivityProperty() == ControllerSensitivity::Medium ? "medium" : "high") +
                             ", racing: camera " + std::to_string(static_cast<int>(d.getRacingCameraAngleProperty())) + ", auto-centre " + YesNo(d.getAutoCenterProperty()) + ", manual gearbox " + YesNo(d.getManualTransmissionProperty()) +
                             ", right stick moves " + YesNo(d.getMoveWithRightThumbStickProperty()) + ", accelerate/brake on buttons " + YesNo(d.getAccelerateWithButtonsProperty()) + "/" + YesNo(d.getBrakeWithButtonsProperty()) +
                             ", colours " + (d.getPrimaryColorProperty() ? "set" : "unset") + "/" + (d.getSecondaryColorProperty() ? "set" : "unset");
                if (GamerProfile* profile = g->GetProfile()) {
                    char buf[160];
                    std::snprintf(buf, sizeof buf, "score %d, %d achievements, reputation %.1f, zone %s, %d titles", profile->getGamerScoreProperty(), profile->getTotalAchievementsProperty(),
                                  static_cast<double>(profile->getReputationProperty()), ZoneName(profile->getGamerZoneProperty()), profile->getTitlesPlayedProperty());
                    c.profile = buf;
                    c.profile += ", motto \"" + profile->getMottoProperty() + "\", region " + profile->getRegionProperty().getEnglishNameProperty();
                    std::unique_ptr<System::IO::Stream> picture(profile->GetGamerPicture());
                    c.profile += picture ? ", has a picture" : ", no picture";
                    delete profile;
                }
                cards_.push_back(std::move(c));
            }
            // The achievements room's table.
            shown_.clear();
            earned_ = 0;
            score_ = 0;
            if (SignedInGamer* g = First(); g != nullptr && mode_ == Mode::Achievements) {
                AchievementCollection list = g->GetAchievements();
                for (int i = 0; i < list.getCountProperty(); ++i) {
                    const Achievement a = list[i];
                    shown_.push_back({a.getKeyProperty(), a.getNameProperty(), a.getDescriptionProperty(), a.getGamerScoreProperty(), a.getIsEarnedProperty(), a.getDisplayBeforeEarnedProperty()});
                    if (a.getIsEarnedProperty()) {
                        ++earned_;
                        score_ += a.getGamerScoreProperty();
                    }
                }
            }
        } catch (const std::exception& e) {
            lastError_ = e.what();
            // The profiles are local: reading them should not fail, and lastError_ is not on any panel. Runs twice a second.
            static bool reported = false;
            if (!reported) { reported = true; ReportError(std::string("profile refresh: ") + e.what()); }
        }
    }

    SignedInGamer* Nth(int n)
    {
        auto* gamers = Gamer::getSignedInGamersProperty();
        if (gamers == nullptr || gamers->getCountProperty() == 0) return nullptr;
        return (*gamers)[std::min(n, gamers->getCountProperty() - 1)];
    }
    [[nodiscard]] LeaderboardIdentity BoardId() const
    {
        static constexpr LeaderboardKey keys[] = {LeaderboardKey::BestScoreLifeTime, LeaderboardKey::BestScoreRecent, LeaderboardKey::BestTimeLifeTime, LeaderboardKey::BestTimeRecent};
        return LeaderboardIdentity::Create(keys[std::clamp(boardKey_, 0, 3)]);
    }
    void AddRating(long long delta)
    {
        try {
            onlineRoundStage_.clear();
            SignedInGamer* g = Nth(writer_);
            if (g == nullptr) return;
            if (g->getIsSignedInToLiveProperty()) {
                PlayOnlineRound(g, delta);
                return;
            }
            LeaderboardEntry* entry = g->getLeaderboardWriterProperty().GetLeaderboard(BoardId());
            SetMuseumColumns(*entry, delta);
            entry->setRatingProperty(delta == std::numeric_limits<long long>::min() ? 0 : entry->getRatingProperty() + delta);
            dirty_ = true;
        } catch (const std::exception& e) {
            boardError_ = (onlineRoundStage_.empty() ? std::string() : onlineRoundStage_ + ": ") +
                std::string(e.what()).substr(0, 60);
            Museum::Log("online", "leaderboard write at %s: %s",
                        onlineRoundStage_.empty() ? "offline update" : onlineRoundStage_.c_str(), e.what());
        }
    }
    // An account's rating reaches the service only when a game session ends: a LocalWithLeaderboards session is created, started, the
    // gamer's entry is set and EndGame commits it (the writes are transient until then).
    void PlayOnlineRound(SignedInGamer* g, long long delta)
    {
        onlineRoundStage_ = "reading current service rating";
        long long current = 0;
        {
            LeaderboardReader read = LeaderboardReader::Read(BoardId(), std::vector<Gamer*>{g}, g, 1);
            const auto entries = read.getEntriesProperty();
            for (int i = 0; i < entries.getCountProperty(); ++i)
                if (entries[i].getGamerProperty() != nullptr && entries[i].getGamerProperty()->getGamertagProperty() == g->getGamertagProperty()) current = entries[i].getRatingProperty();
        }
        onlineRoundStage_ = "creating LocalWithLeaderboards session";
        int arbitrated = 0, unarbitrated = 0, trueSkill = 0;
        std::string finalGamer;
        bool finalLeaving = false;
        const auto observe = [&](int& count, const WriteLeaderboardsEventArgs& args) {
            ++count;
            finalGamer = args.getGamerProperty() != nullptr
                ? args.getGamerProperty()->getGamertagProperty() : std::string("?");
            finalLeaving = args.getIsLeavingProperty();
        };
        // Declare the session after the captured state so its destructor cannot outlive anything
        // the final-write handlers reference if a later service operation throws.
        std::unique_ptr<NetworkSession> session(NetworkSession::Create(NetworkSessionType::LocalWithLeaderboards, 1, 2));
        onlineRoundStage_ = "subscribing final write events";
        session->WriteArbitratedLeaderboard.Add(
            [&](System::Object*, const WriteLeaderboardsEventArgs& args) { observe(arbitrated, args); });
        session->WriteUnarbitratedLeaderboard.Add(
            [&](System::Object*, const WriteLeaderboardsEventArgs& args) { observe(unarbitrated, args); });
        session->WriteTrueSkill.Add(
            [&](System::Object*, const WriteLeaderboardsEventArgs& args) { observe(trueSkill, args); });
        for (int i = 0; i < session->getLocalGamersProperty().getCountProperty(); ++i) session->getLocalGamersProperty()[i]->setIsReadyProperty(true);
        onlineRoundStage_ = "starting service round";
        session->StartGame();
        for (int i = 0; i < 3; ++i) session->Update();   // the state changes to Playing on an update
        onlineRoundStage_ = "writing leaderboard entry";
        LeaderboardEntry* entry = g->getLeaderboardWriterProperty().GetLeaderboard(BoardId());
        SetMuseumColumns(*entry, delta);
        entry->setRatingProperty(delta == std::numeric_limits<long long>::min() ? 0 : current + delta);
        onlineRoundStage_ = "ending service round";
        session->EndGame();
        onlineRoundStage_ = "finalizing service round";
        session->Update();
        leaderboardEvents_ = "arbitrated " + std::to_string(arbitrated) +
            ", unarbitrated " + std::to_string(unarbitrated) +
            ", TrueSkill " + std::to_string(trueSkill) + "; gamer " + finalGamer +
            ", IsLeaving " + (finalLeaving ? "true" : "false");
        boardError_ = "round played in a LocalWithLeaderboards session; EndGame sent the rating to the service";
        Museum::Log("online", "round played: %s rated %lld; %s", g->getGamertagProperty().c_str(),
                    current + delta, leaderboardEvents_.c_str());
        onlineRoundStage_ = "complete";
        dirty_ = true;
    }
    void Page(bool down)
    {
        try {
            if (!reader_) return;
            if (down && reader_->getCanPageDownProperty()) reader_->PageDown();
            if (!down && reader_->getCanPageUpProperty()) reader_->PageUp();
            ReadBoard(false);
        } catch (const std::exception& e) {
            boardError_ = std::string(e.what()).substr(0, 60);
        }
    }
    // A scripted run reads the ranking from the log: a fresh read from the top, one line with every entry of the first page.
    void LogBoard()
    {
        const bool pivot = pivot_;
        const int size = pageSize_;
        pivot_ = false;
        pageSize_ = 10;
        ReadBoard(true);
        pivot_ = pivot;
        pageSize_ = size;
        std::string line;
        for (const auto& row : rows_) line += (line.empty() ? "" : "; ") + std::to_string(row.rank) + ". " + row.tag + " " + std::to_string(row.rating);
        Museum::Log("online", "leaderboard from the top: %s (total %d)%s", line.empty() ? "empty" : line.c_str(), total_, boardError_.empty() ? "" : (" error: " + boardError_).c_str());
    }
    // A new LeaderboardReader.Read starts at the top (or around the pivot); paging moves the same reader.
    void ReadBoard(bool fresh)
    {
        try {
            if (fresh || !reader_) {
                reader_ = std::make_unique<LeaderboardReader>(pivot_ && Nth(0) != nullptr ? LeaderboardReader::Read(BoardId(), Nth(0), pageSize_) : LeaderboardReader::Read(BoardId(), 0, pageSize_));
            }
            rows_.clear();
            const auto entries = reader_->getEntriesProperty();
            for (int i = 0; i < entries.getCountProperty(); ++i) {
                const LeaderboardEntry& e = entries[i];
                rows_.push_back({e.getRankingEXTProperty(), e.getGamerProperty() != nullptr ? e.getGamerProperty()->getGamertagProperty() : std::string("?"), e.getRatingProperty(), DescribeColumns(e)});
            }
            pageStart_ = reader_->getPageStartProperty();
            total_ = reader_->getTotalLeaderboardSizeProperty();
            canDown_ = reader_->getCanPageDownProperty();
            canUp_ = reader_->getCanPageUpProperty();
            boardError_.clear();
        } catch (const std::exception& e) {
            boardError_ = std::string(e.what()).substr(0, 60);
        }
    }

    static void SetMuseumColumns(LeaderboardEntry& entry, long long delta)
    {
        auto& columns = entry.getColumnsProperty();
        const int runs = columns.ContainsKey("MuseumRuns") ? columns.GetValueInt32("MuseumRuns") : 0;
        columns.SetValue("MuseumRuns", runs + 1);
        columns.SetValue("Outcome", delta == std::numeric_limits<long long>::min()
            ? LeaderboardOutcome::None : (delta >= 100 ? LeaderboardOutcome::Win : LeaderboardOutcome::Tie));
        columns.SetValue("Exhibit", std::string("cna-3d-museum"));
    }

    [[nodiscard]] static std::string DescribeColumns(const LeaderboardEntry& entry)
    {
        const auto& columns = entry.getColumnsProperty();
        if (columns.getCountProperty() == 0) return "Columns: none";
        const std::vector<std::string> keys = columns.Keys();
        const std::vector<std::any> values = columns.Values();
        std::string result = "Columns (Keys/Values " + std::to_string(keys.size()) + "/" +
            std::to_string(values.size()) + "): ";
        if (columns.ContainsKey("MuseumRuns"))
            result += "MuseumRuns=" + std::to_string(columns.GetValueInt32("MuseumRuns"));
        if (columns.ContainsKey("Outcome")) {
            const LeaderboardOutcome outcome = columns.GetValueOutcome("Outcome");
            result += std::string(result.back() == ' ' ? "" : ", ") + "Outcome="
                + (outcome == LeaderboardOutcome::Win ? "Win" : outcome == LeaderboardOutcome::Loss ? "Loss" : outcome == LeaderboardOutcome::Tie ? "Tie" : "None");
        }
        return result;
    }

    void Award(int index)
    {
        static const char* keys[] = {"first-steps", "explorer", "cartographer", "every-floor", "elevator", "completionist"};
        SignedInGamer* g = First();
        if (g == nullptr) {
            lastAward_ = "nobody is signed in";
            return;
        }
        try {
            g->AwardAchievement(keys[std::clamp(index, 0, 5)]);
            lastAward_ = std::string("awarded ") + keys[std::clamp(index, 0, 5)];
        } catch (const std::exception& e) {
            lastAward_ = std::string(e.what()).substr(0, 48);
        }
        Refresh();
    }

    void ShowMessageBox()
    {
        if (Guide::getIsVisibleProperty() || box_ != nullptr) return;
        try {
            box_ = Guide::BeginShowMessageBox("The Guide", "Do you like the museum so far?", {"Yes", "Very much", "Ask me later"}, 1, MessageBoxIcon::Alert, {}, std::any{});
            answer_ = "the message box is open: click a button (arrows, Enter) or press Escape";
        } catch (const std::exception& e) {
            answer_ = std::string(e.what()).substr(0, 56);
        }
    }
    void ShowKeyboard()
    {
        if (Guide::getIsVisibleProperty() || keyboard_ != nullptr) return;
        try {
            keyboard_ = Guide::BeginShowKeyboardInput(PlayerIndex::One, "Name your exhibit", "Type something and press Enter", "Hello", {}, std::any{});
            answer_ = "the keyboard is open: type, then Enter (Escape cancels)";
        } catch (const std::exception& e) {
            answer_ = std::string(e.what()).substr(0, 56);
        }
    }
    // The async results complete in the Guide's own time; Update picks them up.
    void Poll()
    {
        try {
            if (box_ != nullptr && box_->getIsCompletedProperty()) {
                const std::optional<int> pick = Guide::EndShowMessageBox(box_);
                delete box_;
                box_ = nullptr;
                static const char* names[] = {"Yes", "Very much", "Ask me later"};
                answer_ = pick ? std::string("EndShowMessageBox returned ") + std::to_string(*pick) + " (" + names[std::clamp(*pick, 0, 2)] + ")" : std::string("EndShowMessageBox returned null: the box was cancelled");
            }
            if (keyboard_ != nullptr && keyboard_->getIsCompletedProperty()) {
                const bool cancelled = Guide::WasKeyboardInputCanceledEXT(keyboard_);
                const std::string typed = Guide::EndShowKeyboardInput(keyboard_);
                delete keyboard_;
                keyboard_ = nullptr;
                answer_ = cancelled ? std::string("EndShowKeyboardInput: cancelled") : "EndShowKeyboardInput returned \"" + typed + "\"";
            }
        } catch (const std::exception& e) {
            answer_ = std::string(e.what()).substr(0, 56);
        }
    }

    template <typename Rect, typename Text>
    void DrawPlayers(Rect& rect, Text& text)
    {
        if (cards_.empty()) {
            text("Nobody is signed in. Start the museum with CNA_GAMER_SERVICES_AUTO_SIGN_IN=Name,Name or sign in through the Guide.", 24, 90, Kit::Rgba(255, 190, 150), 0.62f);
            return;
        }
        for (std::size_t i = 0; i < cards_.size() && i < 4; ++i) {
            const Card& c = cards_[i];
            const bool roomy = cards_.size() <= 2;   // one or two gamers: tall cards, so every line can wrap
            const float x = 24.0f + static_cast<float>(i % 2) * 470.0f, y = 62.0f + (roomy ? 0.0f : static_cast<float>(i / 2) * 230.0f), h = roomy ? 440.0f : 215.0f;
            rect(x, y, 450, h, Color(22, 30, 46, 235));
            rect(x, y, 450, 4, i == 0 ? Kit::kGold : Kit::Rgba(110, 170, 255));
            text(c.tag, x + 12, y + 8, Kit::kText, 0.95f);
            text(c.display + "   " + c.index, x + 12, y + 44, Kit::kDim, 0.55f);
            text(std::string("IsGuest ") + YesNo(c.guest) + "    IsSignedInToLive " + YesNo(c.live), x + 12, y + 70, Kit::kText, 0.55f);
            text("Presence: " + c.presence, x + 12, y + 96, Kit::Rgba(150, 255, 190), 0.55f);
            float ty = y + 124.0f;
            for (const std::string* block : {&c.privileges, &c.defaults, &c.profile}) {
                const char* head = block == &c.privileges ? "Privileges: " : block == &c.defaults ? "GameDefaults: " : "Profile: ";
                for (const std::string& line : Kit::Wrap(*S().font, head + *block, 424.0f, 0.5f)) {
                    if (ty > y + h - 18.0f) break;
                    text(line, x + 12, ty, Kit::kText, 0.5f);
                    ty += 19.0f;
                }
                ty += 8.0f;
            }
        }
    }

    template <typename Rect, typename Text>
    void DrawAchievements(Rect& rect, Text& text)
    {
        if (shown_.empty()) {
            text(First() == nullptr ? "Nobody is signed in: achievements belong to a signed-in gamer." : "No achievement catalog was found next to the executable (GamerServices/Achievements.json).", 24, 90, Kit::Rgba(255, 190, 150), 0.62f);
            return;
        }
        for (std::size_t i = 0; i < shown_.size(); ++i) {
            const Tile& t = shown_[i];
            const float x = 24.0f + static_cast<float>(i % 2) * 470.0f, y = 62.0f + static_cast<float>(i / 2) * 150.0f;
            const bool hidden = !t.earned && !t.display;
            rect(x, y, 450, 136, t.earned ? Color(24, 52, 40, 240) : Color(26, 30, 42, 235));
            rect(x, y, 6, 136, t.earned ? Kit::Rgba(110, 230, 150) : Kit::Rgba(110, 110, 130));
            text(hidden ? "???" : t.name, x + 18, y + 10, t.earned ? Kit::kGold : Kit::kText, 0.85f);
            text(hidden ? "A secret achievement." : t.description, x + 18, y + 50, Kit::kText, 0.55f);
            text("key " + t.key + "    " + std::to_string(t.score) + " points", x + 18, y + 78, Kit::kDim, 0.5f);
            text(t.earned ? "EARNED" : "locked", x + 18, y + 104, t.earned ? Kit::Rgba(110, 230, 150) : Kit::kDim, 0.6f);
        }
    }

    template <typename Rect, typename Text>
    void DrawBoard(Rect& rect, Text& text)
    {
        text(std::string("LeaderboardReader.Read(") + (pivot_ ? "id, gamer, " : "id, 0, ") + std::to_string(pageSize_) + ")   page " + std::to_string(pageStart_ / std::max(pageSize_, 1) + 1), 24, 62, Kit::kText, 0.65f);
        if (!boardError_.empty()) text(boardError_, 24, 96, Kit::Rgba(255, 190, 150), 0.6f);
        long long best = 1;
        for (const Row& r : rows_) best = std::max(best, r.rating);
        for (std::size_t i = 0; i < rows_.size() && i < 5; ++i) {
            const Row& r = rows_[i];
            const float y = 110.0f + static_cast<float>(i) * 70.0f;
            rect(24, y, 912, 60, Color(22, 30, 46, 235));
            const float w = 620.0f * static_cast<float>(r.rating) / static_cast<float>(best);
            rect(300, y + 36, std::max(w, 2.0f), 16, r.rank == 1 ? Kit::kGold : Kit::Rgba(110, 170, 255));
            text("#" + std::to_string(r.rank), 36, y + 10, r.rank == 1 ? Kit::kGold : Kit::kText, 0.85f);
            text(r.tag, 110, y + 10, Kit::kText, 0.85f);
            text("Rating " + std::to_string(r.rating), 300, y + 8, Kit::kDim, 0.55f);
            text(r.columns, 610, y + 8, Kit::kDim, 0.48f);
        }
        if (rows_.empty()) text("This page has no entries: ranked gamers are those with a rating above zero.", 24, 120, Kit::kDim, 0.6f);
    }

    template <typename Rect, typename Text>
    void DrawGuide(Rect& rect, Text& text)
    {
        text("Guide.IsVisible: " + std::string(Guide::getIsVisibleProperty() ? "true" : "false") + "     Guide.IsTrialMode: " + std::string(Guide::getIsTrialModeProperty() ? "true" : "false"), 24, 66, Kit::kText, 0.65f);
        text("Message box: " + std::string(box_ != nullptr ? "pending" : "none") + "     Keyboard input: " + std::string(keyboard_ != nullptr ? "pending" : "none"), 24, 100, Kit::kText, 0.65f);
        text(answer_, 24, 140, Kit::Rgba(150, 255, 190), 0.6f);
        rect(24, 190, 912, 4, Kit::kGold);
        text("The Guide draws its own dialogs over the game after every frame: this screen sits behind them.", 24, 210, Kit::kDim, 0.55f);
        text("Press the buttons on the control panel to the left; Escape cancels a dialog.", 24, 240, Kit::kDim, 0.55f);
        if (Guide::getIsTrialModeProperty()) text("TRIAL MODE: a demo build would lock the full game here.", 24, 290, Kit::Rgba(255, 200, 120), 0.8f);
    }

    struct Row {
        int rank;
        std::string tag;
        long long rating;
        std::string columns;
    };
    std::unique_ptr<LeaderboardReader> reader_;
    std::string leaderboardEvents_ = "play a service-backed round to raise them";
    std::string onlineRoundStage_;
    std::vector<Row> rows_;
    int boardKey_ = 0, writer_ = 0, pageSize_ = 1, pageStart_ = 0, total_ = 0;
    bool pivot_ = false, dirty_ = true, canDown_ = false, canUp_ = false;
    std::string boardError_;
    struct Tile {
        std::string key, name, description;
        int score = 0;
        bool earned = false, display = true;
    };
    std::vector<Card> cards_;
    std::vector<Tile> shown_;
    int earned_ = 0, score_ = 0, selected_ = 0, signedIn_ = 0, signedOut_ = 0;
    std::string lastAward_ = "-", answer_ = "-", lastError_, presenceText_ = "(not set)";
    float refresh_ = 0.0f;
    bool subscribed_ = false;
    std::size_t inToken_ = 0, outToken_ = 0;
    System::IAsyncResult* box_ = nullptr;
    System::IAsyncResult* keyboard_ = nullptr;
#else
    void Unsubscribe() {}
#endif
    Mode mode_;
    bool screenSaverStart_ = false;
};

}  // namespace

std::unique_ptr<Exhibit> MakePlayersLab(const RoomDef& room, Services& services)
{
    return std::make_unique<PlayersLab>(room, services);
}
