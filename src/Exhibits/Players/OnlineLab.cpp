// Online laboratory: CNA's account service, seen through GamerServices. Accounts, friends, messages and parties are the Guide's own
// pages (Guide.ShowSignIn, ShowFriends, ShowMessages, ShowParty ...), which the game opens and the platform draws; what the game reads is
// SignedInGamer.GetFriends and the profile. The museum runs offline unless a service is configured (tools/online_stack.sh starts one on
// loopback); without it these rooms say how to start one and the Guide pages refuse with GamerServicesNotAvailableException.
#include <algorithm>
#include <cstdio>
#include <exception>
#include <functional>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"

#ifdef MUSEUM_NETWORKING
#include "CNA/GamerServices/Configuration.hpp"
#include "Microsoft/Xna/Framework/GamerServices/FriendCollection.hpp"
#include "Microsoft/Xna/Framework/GamerServices/FriendGamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/Gamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/GamerProfile.hpp"
#include "Microsoft/Xna/Framework/GamerServices/Guide.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamerCollection.hpp"
#include "Microsoft/Xna/Framework/PlayerIndex.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Account, Friends, Messages, Party };

Mode ParseMode(const std::string& s)
{
    if (s == "friends") return Mode::Friends;
    if (s == "messages") return Mode::Messages;
    if (s == "party") return Mode::Party;
    return Mode::Account;
}

#ifdef MUSEUM_NETWORKING
using namespace Microsoft::Xna::Framework::GamerServices;
#endif

class OnlineLab final : public ScreenExhibit {
public:
    OnlineLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_NETWORKING
        p.AddReadout("Service", [this] { return service_; });
        p.AddButton("Guide.ShowSignIn(1, onlineOnly)", [this] { Open("ShowSignIn(1, true)", [] { Guide::ShowSignIn(1, true); }); });
        switch (mode_) {
            case Mode::Account:
                p.AddButton("Guide.ShowSignIn(2, onlineOnly)", [this] { Open("ShowSignIn(2, true)", [] { Guide::ShowSignIn(2, true); }); });
                p.AddButton("Guide.ShowAchievementsEXT", [this] { Open("ShowAchievementsEXT", [] { Guide::ShowAchievementsEXT(PlayerIndex::One); }); });
                p.AddButton("Guide.ShowMarketplace (game content)", [this] { Open("ShowMarketplace", [] { Guide::ShowMarketplace(PlayerIndex::One); }); });
                p.AddReadout("Signed in", [this] { return signedIn_; });
                break;
            case Mode::Friends:
                p.AddButton("Guide.ShowFriends", [this] { Open("ShowFriends", [] { Guide::ShowFriends(PlayerIndex::One); }); });
                p.AddButton("Guide.ShowPlayers (recent)", [this] { Open("ShowPlayers", [] { Guide::ShowPlayers(PlayerIndex::One); }); });
                p.AddButton("Guide.ShowGamerCard(first friend)", [this] { WithFriend("ShowGamerCard", [](FriendGamer* f) { Guide::ShowGamerCard(PlayerIndex::One, f); }); });
                p.AddButton("Guide.ShowPlayerReview(first friend)", [this] { WithFriend("ShowPlayerReview", [](FriendGamer* f) { Guide::ShowPlayerReview(PlayerIndex::One, f); }); });
                p.AddButton("Guide.ShowFriendRequest(first friend)", [this] { WithFriend("ShowFriendRequest", [](FriendGamer* f) { Guide::ShowFriendRequest(PlayerIndex::One, f); }); });
                p.AddButton("Log the friends (SignedInGamer.GetFriends)", [this] { LogFriends(); });
                p.AddReadout("GetFriends().Count", [this] { return std::to_string(friends_.size()); });
                p.AddReadout("FriendCollection.IsDisposed", [this] { return friendCollectionState_; });
                break;
            case Mode::Messages:
                p.AddButton("Guide.ShowMessages", [this] { Open("ShowMessages", [] { Guide::ShowMessages(PlayerIndex::One); }); });
                p.AddButton("Guide.ShowComposeMessage (to the friends)", [this] { Compose(true); });
                p.AddButton("Guide.ShowComposeMessage (asks for a gamertag)", [this] { Compose(false); });
                p.AddReadout("Friends to write to", [this] { return std::to_string(friends_.size()); });
                break;
            case Mode::Party:
                p.AddButton("Guide.ShowParty", [this] { Open("ShowParty", [] { Guide::ShowParty(PlayerIndex::One); }); });
                p.AddButton("Guide.ShowPartySessions", [this] { Open("ShowPartySessions", [] { Guide::ShowPartySessions(PlayerIndex::One); }); });
                p.AddReadout("SignedInGamer.PartySize", [this] { return partySize_; });
                break;
        }
        p.AddReadout("Last Guide call", [this] { return note_; });
#else
        p.AddReadout("GamerServices", [] { return std::string("not built into this museum"); });
#endif
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
#ifdef MUSEUM_NETWORKING
        try {
            const CNA::GamerServices::Configuration cfg = CNA::GamerServices::resolveConfiguration();
            configured_ = !cfg.endpoint.empty();
            service_ = configured_ ? cfg.endpoint + "  title " + cfg.gameId : "none configured: the museum is offline";
        } catch (const std::exception& e) {
            service_ = std::string("configuration refused: ") + std::string(e.what()).substr(0, 60);
        }
        Refresh();
#endif
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
#ifdef MUSEUM_NETWORKING
        refresh_ -= ctx.dt;
        if (refresh_ <= 0.0f) {
            refresh_ = 2.0f;   // the friend list and profile come from the service: not every frame
            Refresh();
        }
#endif
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(Title(), 24, 12, Kit::kGold, 1.0f);
#ifdef MUSEUM_NETWORKING
        if (!configured_) {
            rect(24, 62, 912, 190, Color(40, 28, 22, 240));
            text("No account service is configured.", 44, 76, Kit::Rgba(255, 190, 150), 0.8f);
            text("The museum signs two offline profiles in, and the Guide's service pages refuse without an account.", 44, 116, Kit::kText, 0.55f);
            text("To try the online features: tools/online_stack.sh start    then    eval \"$(tools/online_stack.sh env)\"", 44, 152, Kit::kText, 0.55f);
            text("and start the museum from that shell. Accounts: visitor, curator, archivist (see the header of the script).", 44, 182, Kit::kText, 0.55f);
        } else {
            DrawPlayers(rect, text);
            if (mode_ == Mode::Friends || mode_ == Mode::Messages) DrawFriends(rect, text);
            if (mode_ == Mode::Party) DrawParty(rect, text);
        }
        text("last Guide call: " + note_, 24, 500, Kit::kDim, 0.5f);
#else
        text("This museum was built without CNA's GamerServices (-DMUSEUM_NETWORKING=OFF).", 24, 90, Kit::Rgba(255, 190, 150), 0.7f);
        (void)rect;
#endif
        sb.End();
    }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Accounts and the Guide", "Friends and presence", "Messages", "Parties"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Account: return {"With an account service CNA signs in real accounts through the Guide: the game calls Guide.ShowSignIn and the platform asks for a name and a password. The gamer is then", "IsSignedInToLive, has a profile (gamer score, zone, reputation) and its own avatar, and the service keeps its achievements and leaderboards."};
            case Mode::Friends: return {"Friends live on the service: mutual requests, presence (online, away, busy, playing, joinable) and invitations. The game reads SignedInGamer.GetFriends; adding, reviewing", "and looking at a gamer is done by the Guide's pages, which the buttons open. The Guide owns the keyboard while it is up (Escape closes it)."};
            case Mode::Messages: return {"Messages go through the service: the Guide shows the player's inbox (read, reply, delete) and composes a message to friends or to a gamertag typed there.", "Both pages are the platform's; the game only opens them."};
            default: return {"A party groups up to eight friends across games: members, invitations and the joinable sessions of the members (ShowPartySessions). SignedInGamer.PartySize says how many are in it;", "the Guide's party page invites friends and leaves."};
        }
    }

#ifdef MUSEUM_NETWORKING
    void Open(const char* what, const std::function<void()>& fn)
    {
        try {
            fn();
            note_ = std::string(what) + ": opened (Escape closes it)";
            Museum::Log("online", "%s opened", what);
        } catch (const std::exception& e) {
            note_ = std::string(what) + ": " + std::string(e.what()).substr(0, 80);
            Museum::Log("online", "%s: %s", what, e.what());
        }
    }
    // A scripted run reads the friend list the service returned from the log: gamertag and presence flags of each friend.
    void LogFriends()
    {
        try {
            SignedInGamer* g = First();
            if (g == nullptr) throw std::runtime_error("nobody is signed in");
            FriendCollection list = g->GetFriends();
            std::string line;
            for (int i = 0; i < list.getCountProperty(); ++i) {
                FriendGamer* f = list[i];
                line += (line.empty() ? "" : "; ") + f->getGamertagProperty() + " (online " + (f->getIsOnlineProperty() ? "yes" : "no") + ", joinable " + (f->getIsJoinableProperty() ? "yes" : "no") + ")";
            }
            Museum::Log("online", "friends of %s: %d: %s", g->getGamertagProperty().c_str(), static_cast<int>(list.getCountProperty()), line.empty() ? "none" : line.c_str());
        } catch (const std::exception& e) {
            Museum::Log("online", "friends: %s", e.what());
        }
    }
    void WithFriend(const char* what, const std::function<void(FriendGamer*)>& fn)
    {
        try {
            SignedInGamer* g = First();
            if (g == nullptr) throw std::runtime_error("nobody is signed in");
            FriendCollection list = g->GetFriends();
            if (list.getCountProperty() == 0) throw std::runtime_error("no friends yet: ShowFriends adds one");
            fn(list[0]);
            note_ = std::string(what) + ": opened";
        } catch (const std::exception& e) {
            note_ = std::string(what) + ": " + std::string(e.what()).substr(0, 80);
        }
    }
    void Compose(bool toFriends)
    {
        try {
            std::vector<Gamer*> to;
            FriendCollection list = First() != nullptr ? First()->GetFriends() : FriendCollection::CreateInternal({});
            if (toFriends)
                for (int i = 0; i < list.getCountProperty() && i < 8; ++i) to.push_back(list[i]);
            Guide::ShowComposeMessage(PlayerIndex::One, "Hello from the CNA 3D Museum", to);
            note_ = "ShowComposeMessage: opened with " + std::to_string(to.size()) + " recipient(s)";
        } catch (const std::exception& e) {
            note_ = std::string("ShowComposeMessage: ") + std::string(e.what()).substr(0, 80);
        }
    }
    [[nodiscard]] static SignedInGamer* First()
    {
        auto* gamers = Gamer::getSignedInGamersProperty();
        return gamers != nullptr && gamers->getCountProperty() > 0 ? (*gamers)[0] : nullptr;
    }

    struct Card {
        std::string tag, state, profile;
    };
    struct Friend {
        std::string tag, presence, flags;
    };
    void Refresh()
    {
        try {
            cards_.clear();
            friends_.clear();
            auto* gamers = Gamer::getSignedInGamersProperty();
            signedIn_ = std::to_string(gamers != nullptr ? gamers->getCountProperty() : 0);
            partySize_ = "-";
            friendCollectionState_ = "no signed-in gamer";
            if (gamers != nullptr && gamers->getCountProperty() > 0) {
                FriendCollection list = (*gamers)[0]->GetFriends();
                friendCollectionState_ = list.getIsDisposedProperty() ? "true" : "false (live)";
            }
            for (int i = 0; gamers != nullptr && i < gamers->getCountProperty(); ++i) {
                SignedInGamer* g = (*gamers)[i];
                Card c;
                c.tag = g->getGamertagProperty();
                c.state = std::string(g->getIsSignedInToLiveProperty() ? "signed in to the service" : "offline profile") + (g->getIsGuestProperty() ? ", guest" : "");
                if (configured_ && g->getIsSignedInToLiveProperty()) {
                    if (GamerProfile* profile = g->GetProfile()) {
                        char buf[120];
                        std::snprintf(buf, sizeof buf, "score %d, %d achievements, reputation %.1f", profile->getGamerScoreProperty(), profile->getTotalAchievementsProperty(), static_cast<double>(profile->getReputationProperty()));
                        c.profile = buf;
                        delete profile;
                    }
                    if (i == 0) {
                        partySize_ = std::to_string(g->getPartySizeProperty());
                        if (partySize_ != loggedPartySize_) Museum::Log("online", "SignedInGamer.PartySize of %s: %s", g->getGamertagProperty().c_str(), partySize_.c_str());
                        loggedPartySize_ = partySize_;
                        FriendCollection list = g->GetFriends();
                        for (int f = 0; f < list.getCountProperty() && f < 8; ++f) {
                            FriendGamer* fr = list[f];
                            Friend row;
                            row.tag = fr->getGamertagProperty();
                            row.presence = fr->getPresenceProperty();
                            row.flags = std::string(fr->getIsOnlineProperty() ? "online " : "offline ") + (fr->getIsAwayProperty() ? "away " : "") + (fr->getIsBusyProperty() ? "busy " : "") + (fr->getIsPlayingProperty() ? "playing " : "") +
                                        (fr->getIsJoinableProperty() ? "joinable " : "") + (fr->getFriendRequestReceivedFromProperty() ? "[request received] " : "") + (fr->getFriendRequestSentToProperty() ? "[request sent] " : "") +
                                        (fr->getInviteReceivedFromProperty() ? "[invited you] " : "") + (fr->getInviteSentToProperty() ? "[you invited] " : "");
                            friends_.push_back(std::move(row));
                        }
                    }
                }
                cards_.push_back(std::move(c));
            }
        } catch (const std::exception& e) {
            note_ = std::string("refresh: ") + std::string(e.what()).substr(0, 70);
        }
    }

    template <typename Rect, typename Text>
    void DrawPlayers(Rect& rect, Text& text)
    {
        if (cards_.empty()) {
            rect(24, 62, 912, 120, Color(22, 30, 46, 235));
            text("Nobody is signed in. Press the sign-in button and type an account name and password at the Guide.", 44, 80, Kit::Rgba(255, 220, 150), 0.62f);
            text("visitor / museum-visitor-pass      curator / museum-curator-pass      archivist / museum-archivist-pass", 44, 118, Kit::kDim, 0.5f);
            return;
        }
        for (std::size_t i = 0; i < cards_.size() && i < 4; ++i) {
            const Card& c = cards_[i];
            const float x = 24.0f + static_cast<float>(i % 2) * 460.0f, y = 62.0f + static_cast<float>(i / 2) * 92.0f;
            rect(x, y, 450, 84, Color(22, 30, 46, 235));
            rect(x, y, 450, 4, i == 0 ? Kit::kGold : Kit::Rgba(110, 170, 255));
            text(c.tag + "   " + c.state, x + 12, y + 8, Kit::kText, 0.62f);
            text(c.profile.empty() ? "no profile read" : c.profile, x + 12, y + 44, Kit::kDim, 0.5f);
        }
    }
    template <typename Rect, typename Text>
    void DrawFriends(Rect& rect, Text& text)
    {
        rect(24, 258, 912, 232, Color(18, 24, 38, 240));
        text("SignedInGamer.GetFriends()", 40, 266, Kit::kGold, 0.6f);
        if (friends_.empty()) text("no friends (or the service has not answered): open the Guide's friends page and add one by gamertag", 40, 300, Kit::kDim, 0.5f);
        float y = 300.0f;
        for (const Friend& f : friends_) {
            text(f.tag + "   " + f.presence, 40, y, Kit::kText, 0.55f);
            text(f.flags, 360, y, Kit::Rgba(150, 255, 190), 0.5f);
            y += 28.0f;
        }
    }
    template <typename Rect, typename Text>
    void DrawParty(Rect& rect, Text& text)
    {
        rect(24, 258, 912, 160, Color(18, 24, 38, 240));
        text("SignedInGamer.PartySize: " + partySize_, 40, 270, Kit::kText, 0.8f);
        text("Open the party page, invite a friend, and sign them in on a second museum to see the size change.", 40, 322, Kit::kDim, 0.5f);
    }

    std::vector<Card> cards_;
    std::vector<Friend> friends_;
    std::string friendCollectionState_ = "-";
    std::string service_ = "-", signedIn_ = "0", partySize_ = "-", loggedPartySize_, note_ = "-";
    bool configured_ = false;
    float refresh_ = 0.0f;
#endif
    Mode mode_;
};

}  // namespace

std::unique_ptr<Exhibit> MakeOnlineLab(const RoomDef& room, Services& services)
{
    return std::make_unique<OnlineLab>(room, services);
}
