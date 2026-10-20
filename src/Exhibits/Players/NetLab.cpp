// Networking laboratory: Microsoft.Xna.Framework.Net as CNA implements it. Two rooms: PacketWriter and PacketReader (what a
// packet is, byte by byte), and a Local NetworkSession between the two signed-in profiles, with simulated latency and loss.
// Everything stays in this process: a Local session never touches the network, which is also why it works under test.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <deque>
#include <exception>
#include <map>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"

#ifdef MUSEUM_NETWORKING
#include "Microsoft/Xna/Framework/GamerServices/Gamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/FriendCollection.hpp"
#include "Microsoft/Xna/Framework/GamerServices/Guide.hpp"
#include "Microsoft/Xna/Framework/GamerServices/InviteAcceptedEventArgs.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamerCollection.hpp"
#include "Microsoft/Xna/Framework/Net/GameEndedEventArgs.hpp"
#include "Microsoft/Xna/Framework/Net/GameStartedEventArgs.hpp"
#include "Microsoft/Xna/Framework/Net/GamerJoinedEventArgs.hpp"
#include "Microsoft/Xna/Framework/Net/AvailableNetworkSession.hpp"
#include "Microsoft/Xna/Framework/Net/AvailableNetworkSessionCollection.hpp"
#include "Microsoft/Xna/Framework/Net/GamerLeftEventArgs.hpp"
#include "Microsoft/Xna/Framework/Net/NetworkSessionProperties.hpp"
#include "Microsoft/Xna/Framework/Net/QualityOfService.hpp"
#include "Microsoft/Xna/Framework/Net/LocalNetworkGamer.hpp"
#include "Microsoft/Xna/Framework/Net/NetworkSession.hpp"
#include "Microsoft/Xna/Framework/Net/HostChangedEventArgs.hpp"
#include "Microsoft/Xna/Framework/Net/NetworkSessionEndReason.hpp"
#include "Microsoft/Xna/Framework/Net/NetworkSessionEndedEventArgs.hpp"
#include "Microsoft/Xna/Framework/Net/NetworkSessionJoinError.hpp"
#include "Microsoft/Xna/Framework/Net/NetworkSessionJoinException.hpp"
#include "Microsoft/Xna/Framework/Net/NetworkMachine.hpp"
#include "Microsoft/Xna/Framework/Net/PacketReader.hpp"
#include "Microsoft/Xna/Framework/Net/PacketWriter.hpp"
#include "Microsoft/Xna/Framework/Net/SendDataOptions.hpp"
#include "System/IO/MemoryStream.hpp"
#include "System/TimeSpan.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Packets, Session };

Mode ParseMode(const std::string& s) { return s == "session" ? Mode::Session : Mode::Packets; }

#ifdef MUSEUM_NETWORKING
using namespace Microsoft::Xna::Framework::Net;

const char* StateName(NetworkSessionState s)
{
    switch (s) {
        case NetworkSessionState::Lobby: return "Lobby";
        case NetworkSessionState::Playing: return "Playing";
        default: return "Ended";
    }
}
#endif

class NetLab final : public ScreenExhibit {
public:
    NetLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))) {}

    ~NetLab() override
    {
        CloseSession();
#ifdef MUSEUM_NETWORKING
        if (inviteSubscribed_) {
            try { NetworkSession::InviteAccepted.Remove(inviteToken_); } catch (const std::exception&) {}
        }
#endif
    }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_NETWORKING
        if (mode_ == Mode::Packets) {
            p.AddToggle("int32 sequence number", true, [this](bool v) { fields_[0] = v; dirty_ = true; });
            p.AddToggle("bool", true, [this](bool v) { fields_[1] = v; dirty_ = true; });
            p.AddToggle("float (Single)", true, [this](bool v) { fields_[2] = v; dirty_ = true; });
            p.AddToggle("double", false, [this](bool v) { fields_[3] = v; dirty_ = true; });
            p.AddToggle("string", true, [this](bool v) { fields_[4] = v; dirty_ = true; });
            p.AddToggle("Vector3", true, [this](bool v) { fields_[5] = v; dirty_ = true; });
            p.AddToggle("Quaternion", false, [this](bool v) { fields_[6] = v; dirty_ = true; });
            p.AddToggle("Matrix", false, [this](bool v) { fields_[7] = v; dirty_ = true; });
            p.AddSlider("String length", 0.0f, 40.0f, static_cast<float>(stringLength_), [this](float v) { stringLength_ = static_cast<int>(v); dirty_ = true; }, "%.0f", 1.0f);
            p.AddReadout("Packet size", [this] { return std::to_string(size_) + " bytes"; });
            p.AddReadout("Read back", [this] { return roundTrip_; });
        } else {
            p.AddButton("NetworkSession.StartGame", [this] { NetworkSession* n = Primary(); if (n != nullptr && n->getSessionStateProperty() == NetworkSessionState::Lobby) n->StartGame(); });
            p.AddButton("NetworkSession.EndGame", [this] { NetworkSession* n = Primary(); if (n != nullptr && n->getSessionStateProperty() == NetworkSessionState::Playing) n->EndGame(); });
            p.AddButton("NetworkSession.AddLocalGamer (second profile)", [this] { AddSecondLocalGamer(); });
            p.AddToggle("Both players ready (IsReady)", false, [this](bool v) { SetReady(v); });
            p.AddReadout("SessionState", [this] { NetworkSession* n = Primary(); return n != nullptr ? std::string(StateName(n->getSessionStateProperty())) + (n->getIsHostProperty() ? ", this machine hosts" : "") : std::string("no session"); });
            p.AddReadout("IsEveryoneReady", [this] { NetworkSession* n = Primary(); return n != nullptr ? std::string(n->getIsEveryoneReadyProperty() ? "true" : "false") : std::string("-"); });
            p.AddReadout("Gamers (local / all)", [this] { NetworkSession* n = Primary(); return n != nullptr ? std::to_string(n->getLocalGamersProperty().getCountProperty()) + " / " + std::to_string(n->getAllGamersProperty().getCountProperty()) : std::string("-"); });
            p.AddToggle("AllowHostMigration", false, [this](bool v) { NetworkSession* n = Primary(); if (n != nullptr) { try { n->setAllowHostMigrationProperty(v); } catch (const std::exception& e) { Log(std::string("AllowHostMigration refused: ") + std::string(e.what()).substr(0, 48)); } } });
            p.AddReadout("SessionType / MaxGamers / private slots", [this] { return SessionShape(); });
            p.AddReadout("Host / remote / previous gamers", [this] { return GamerRoles(); });
            p.AddReadout("Gamer 0..1: Id, slot, voice, talking, muted, left", [this] { return GamerDetails(); });
            p.AddReadout("NetworkMachine.Gamers / Session / guest", [this] { return MachineDetails(); });
            p.AddButton("NetworkMachine.RemoveFromSession (first remote)", [this] { RemoveRemoteMachine(); });
            p.AddButton("NetworkSession.ResetReady", [this] { NetworkSession* n = Primary(); if (n != nullptr) { try { n->ResetReady(); Museum::Log("net", "ResetReady; gamers: %s", GamerDetails().c_str()); } catch (const std::exception& e) { Log(std::string("ResetReady refused: ") + std::string(e.what()).substr(0, 48)); } } });
            p.AddButton("Guide.ShowSignIn: an account for PlayerMatch", [this] { try { Microsoft::Xna::Framework::GamerServices::Guide::ShowSignIn(1, true); Museum::Log("online", "ShowSignIn(1, true) opened"); } catch (const std::exception& e) { status_ = std::string(e.what()).substr(0, 50); } });
            p.AddChoice("Local gamers in a new session (maxLocalGamers)", {"2", "1"}, 0, [this](int v) { localGamers_ = v == 1 ? 1 : 2; });
            // Private slots are taken only by invited gamers (JoinInvited); an ordinary join finds the public ones full.
            p.AddChoice("MaxGamers in a new session", {"8", "2", "3", "4"}, 0, [this](int v) { maxGamers_ = v == 0 ? 8 : v + 1; });
            p.AddChoice("PrivateGamerSlots in a new session", {"0", "1", "2"}, 0, [this](int v) { privateSlots_ = v; });
            p.AddToggle("Deliver packets: a SystemLink session (visible on your network)", false, [this](bool v) { SetTransport(v); });
            p.AddSlider("SimulatedLatency (ms, on packets this museum sends)", 0.0f, 500.0f, 0.0f, [this](float v) { latencyMs_ = v; ApplyImpairment(); }, "%.0f", 10.0f);
            p.AddSlider("SimulatedPacketLoss", 0.0f, 0.5f, 0.0f, [this](float v) { loss_ = v; ApplyImpairment(); }, "%.2f", 0.05f);
            p.AddSlider("SessionProperties[0] (game mode)", 0.0f, 9.0f, static_cast<float>(gameMode_), [this](float v) { gameMode_ = static_cast<int>(v); ApplySessionSettings(); }, "%.0f", 1.0f);
            p.AddToggle("AllowJoinInProgress", false, [this](bool v) { joinInProgress_ = v; ApplySessionSettings(); });
            p.AddChoice("Search for game mode", {"any", "this session's", "one nobody hosts"}, 0, [this](int v) { searchMode_ = v; });
            p.AddButton("Guide.ShowGameInvite (your friends)", [this] { InviteFriends(); });
            p.AddButton("LocalNetworkGamer.SendPartyInvites (your party, to this session)", [this] { SendPartyInvites(); });
            p.AddButton("NetworkSession.BeginFind (SystemLink)", [this] { BeginFind(); });
            p.AddButton("NetworkSession.BeginJoin (the first session found)", [this] { BeginJoin(); });
            p.AddReadout("Find", [this] { return found_; });
            p.AddToggle("Over the account service (PlayerMatch), not SystemLink", false, [this](bool v) { online_ = v; if (host_ != nullptr) { SetTransport(false); SetTransport(true); } });
            p.AddSlider("Packets per second", 2.0f, 60.0f, rate_, [this](float v) { rate_ = v; }, "%.0f", 1.0f);
            p.AddButton("Burst: 100 packets with each SendDataOptions once another museum joined", [this] { burstArmed_ = true; burstArmedAt_ = -1.0f; Log("burst armed: sent 2 s after a remote gamer is in the session"); });
            p.AddReadout("Transport", [this] { return transportStatus_; });
        }
#else
        p.AddReadout("Net", [] { return std::string("not built into this museum"); });
#endif
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
#ifdef MUSEUM_NETWORKING
        if (mode_ == Mode::Session) {
            OpenLobby();
            // A player who accepts an invitation in the Guide raises InviteAccepted at the next Update; the game then joins with JoinInvited.
            inviteToken_ = NetworkSession::InviteAccepted.Add([this](System::Object*, const Microsoft::Xna::Framework::GamerServices::InviteAcceptedEventArgs& e) {
                inviteAccepted_ = true;
                Log("InviteAccepted: " + (e.getGamerProperty() != nullptr ? e.getGamerProperty()->getGamertagProperty() : std::string("?")) +
                    (e.getIsCurrentSessionProperty() ? ", current session" : ", joining with JoinInvited"));
            });
            inviteSubscribed_ = true;
        }
#endif
    }

    void OnLeave() override
    {
#ifdef MUSEUM_NETWORKING
        CloseSession();   // a session holds its gamers; the next visit opens a new one
#endif
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
#ifdef MUSEUM_NETWORKING
        time_ += ctx.dt;
        windowClock_ += ctx.dt;
        if (windowClock_ >= 1.0f) {   // arrivals per second from the other machine, to show what simulated loss does
            remoteRate_ = static_cast<float>(remoteWindow_) / windowClock_;
            remoteWindow_ = 0;
            windowClock_ = 0.0f;
        }
        if (mode_ == Mode::Packets) {
            if (dirty_) BuildPacket();
        } else {
            if (lobby_ == nullptr && host_ == nullptr && !tried_ && ctx.playerInRoom) OpenLobby();
            if (lobby_ != nullptr) lobby_->Update();
            Pump(ctx.dt);
            PollFind();
            PollJoin();
            WatchVoice();
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
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(Title(), 24, 12, Kit::kGold, 1.0f);
#ifdef MUSEUM_NETWORKING
        if (mode_ == Mode::Packets) DrawPacket(rect, text);
        else DrawSession(rect, text);
#else
        text("This museum was built without CNA's networking (-DMUSEUM_NETWORKING=OFF).", 24, 90, Kit::Rgba(255, 190, 150), 0.7f);
        (void)rect;
#endif
        sb.End();
    }

private:
    [[nodiscard]] std::string Title() const { return mode_ == Mode::Packets ? "PacketWriter and PacketReader" : "A network session on this machine"; }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        if (mode_ == Mode::Packets)
            return {"Everything a game sends is bytes. PacketWriter writes XNA's types (Vector3, Quaternion, Matrix, strings...) in a fixed order; PacketReader must read them in", "the same order. The packet below is rebuilt whenever you change a field; its size is what every send costs on the wire, before headers."};
        return {"A Local session begins with one signed-in profile; AddLocalGamer admits the second and raises GamerJoined. It runs the lobby (StartGame, EndGame, IsReady...) but delivers no packets, as in XNA. A SystemLink session delivers them,",
                "and announces itself on your network, so it stays off until you switch it on. Only one session can exist per process; SimulatedLatency and SimulatedPacketLoss act on the packets this machine sends, as in XNA (reliable ones are resent, never lost): join a second museum (BeginFind, BeginJoin) and raise them there to see what arrives here thin out and come late."};
    }

#ifdef MUSEUM_NETWORKING
    // ---- packets ------------------------------------------------------------------------------------------------------
    void BuildPacket()
    {
        dirty_ = false;
        try {
            PacketWriter w;
            const int seq = 1234567;
            const float f = 3.14159f;
            const Vector3 v(1.5f, -2.25f, 8.0f);
            const Quaternion q = Quaternion::CreateFromYawPitchRoll(0.4f, 0.2f, -0.1f);
            const Matrix m = Matrix::CreateRotationY(0.5f) * Matrix::CreateTranslation(1, 2, 3);
            const std::string str(static_cast<std::size_t>(stringLength_), 'x');
            if (fields_[0]) w.Write(seq);
            if (fields_[1]) w.Write(true);
            if (fields_[2]) w.Write(f);
            if (fields_[3]) w.Write(2.718281828459045);
            if (fields_[4]) w.Write(str);
            if (fields_[5]) w.Write(v);
            if (fields_[6]) w.Write(q);
            if (fields_[7]) w.Write(m);
            const int writerLength = w.getLengthProperty();
            const int writerPosition = w.getPositionProperty();
            w.setPositionProperty(writerPosition);
            auto* memory = dynamic_cast<System::IO::MemoryStream*>(w.getBaseStreamProperty());
            bytes_ = memory != nullptr ? memory->ToArray() : std::vector<SharpRuntime::bytecs>{};
            size_ = static_cast<int>(bytes_.size());
            // The reader gets the same bytes (what LocalNetworkGamer.ReceiveData hands it) and reads the fields back in order.
            PacketReader r;
            r.setPositionProperty(0);
            // An empty packet (every field switched off) has no buffer at all: Stream.Write rejects a null buffer even for 0 bytes.
            if (size_ > 0) r.getBaseStreamProperty()->Write(bytes_.data(), 0, size_);
            r.setPositionProperty(0);
            const int readerLength = r.getLengthProperty();
            bool ok = true;
            if (fields_[0]) ok = ok && r.ReadInt32() == seq;
            if (fields_[1]) ok = ok && r.ReadBoolean();
            if (fields_[2]) ok = ok && r.ReadSingle() == f;
            if (fields_[3]) ok = ok && r.ReadDouble() == 2.718281828459045;
            if (fields_[4]) ok = ok && r.ReadString() == str;
            if (fields_[5]) { const Vector3 b = r.ReadVector3(); ok = ok && b.X == v.X && b.Y == v.Y && b.Z == v.Z; }
            if (fields_[6]) { const Quaternion b = r.ReadQuaternion(); ok = ok && b.X == q.X && b.W == q.W; }
            if (fields_[7]) { const Matrix b = r.ReadMatrix(); ok = ok && b.M11 == m.M11 && b.M43 == m.M43; }
            roundTrip_ = ok ? "every field equal; writer " + std::to_string(writerLength) + " bytes at " + std::to_string(writerPosition) + ", reader " + std::to_string(readerLength) + " bytes, consumed " + std::to_string(r.getPositionProperty()) : "MISMATCH";
        } catch (const std::exception& e) {
            roundTrip_ = std::string("error: ") + std::string(e.what()).substr(0, 40);
            ReportError(std::string("packet round trip: ") + e.what());   // in-memory PacketWriter/PacketReader: no transport can refuse it
        }
    }

    template <typename Rect, typename Text>
    void DrawPacket(Rect& rect, Text& text)
    {
        static const char* names[8] = {"int32", "bool", "Single", "double", "string", "Vector3", "Quaternion", "Matrix"};
        static const int sizes[8] = {4, 1, 4, 8, 0, 12, 16, 64};
        text("packet: " + std::to_string(size_) + " bytes      " + roundTrip_, 24, 62, Kit::kText, 0.62f);
        float y = 100.0f;
        for (int i = 0; i < 8; ++i) {
            rect(24, y + 4, 12, 12, fields_[static_cast<std::size_t>(i)] ? Kit::Rgba(110, 230, 150) : Kit::Rgba(90, 90, 100));
            const int n = i == 4 ? stringLength_ + 1 : sizes[i];
            text(std::string(names[i]) + (fields_[static_cast<std::size_t>(i)] ? "   " + std::to_string(n) + " bytes" : "   (not written)"), 44, y - 2, fields_[static_cast<std::size_t>(i)] ? Kit::kText : Kit::kDim, 0.55f);
            y += 27.0f;
        }
        // The bytes themselves, sixteen to a row.
        text("the bytes, in hex:", 420, 100, Kit::kDim, 0.55f);
        for (std::size_t row = 0; row < 11 && row * 16 < bytes_.size(); ++row) {
            std::string line;
            char cell[8];
            for (std::size_t i = row * 16; i < std::min(bytes_.size(), row * 16 + 16); ++i) {
                std::snprintf(cell, sizeof cell, "%02X ", static_cast<unsigned>(static_cast<unsigned char>(bytes_[i])));
                line += cell;
            }
            text(line, 420, 126.0f + static_cast<float>(row) * 24.0f, Kit::Rgba(190, 235, 200), 0.5f);
        }
        if (bytes_.size() > 176) text("... " + std::to_string(bytes_.size() - 176) + " more", 420, 392, Kit::kDim, 0.5f);
        text("a Matrix costs 64 bytes: sending a whole transform every frame is expensive, a position and a yaw are not.", 24, 340, Kit::kDim, 0.5f);
    }

    // ---- session ------------------------------------------------------------------------------------------------------
    // The lobby is a Local session holding both profiles: its state machine, readiness and events are real, but a Local session
    // sends packets nowhere (as in XNA). Packets move only when the real transport is switched on: a SystemLink host for the
    // Visitor and a client for the Guest, both in this process, talking over this machine's loopback.
    void OpenLobby()
    {
        if (lobby_ != nullptr) return;
        tried_ = true;
        try {
            auto* signedIn = GamerServices::Gamer::getSignedInGamersProperty();
            if (signedIn == nullptr || signedIn->getCountProperty() == 0) {
                status_ = "nobody is signed in yet";
                tried_ = false;
                return;
            }
            lobby_ = NetworkSession::Create(NetworkSessionType::Local,
                std::vector<GamerServices::SignedInGamer*>{(*signedIn)[0]}, 8, 0,
                NetworkSessionProperties{});
            started_ = lobby_->GameStarted.Add([this](System::Object*, const GameStartedEventArgs&) { Log("GameStarted"); });
            ended_ = lobby_->GameEnded.Add([this](System::Object*, const GameEndedEventArgs&) { Log("GameEnded"); });
            joined_ = lobby_->GamerJoined.Add([this](System::Object*, const GamerJoinedEventArgs&) { Log("GamerJoined"); });
            left_ = lobby_->GamerLeft.Add([this](System::Object*, const GamerLeftEventArgs&) { Log("GamerLeft"); });
            sessionEnded_ = lobby_->SessionEnded.Add([this](System::Object*, const NetworkSessionEndedEventArgs& e) { Log(std::string("SessionEnded: ") + EndReasonName(e.getEndReasonProperty())); });
            subscribed_ = true;
            Log("Create(Local, explicit 1 local, 8 max): " + std::to_string(lobby_->getLocalGamersProperty().getCountProperty()) + " local gamer(s)");
            status_ = "lobby open";
        } catch (const std::exception& e) {
            status_ = std::string("cannot create a session: ") + std::string(e.what()).substr(0, 60);
            Museum::Log("net", "%s", status_.c_str());
        }
    }

    static void Dispose(NetworkSession*& s)
    {
        if (s == nullptr) return;
        try { s->Dispose(); } catch (const std::exception&) {}
        delete s;
        s = nullptr;
    }

    void CloseTransport()
    {
        UnhookHost();
        Dispose(host_);
        phase_ = Phase::Off;
        transportStatus_ = "off: a Local session delivers no packets";
    }

    void DisposeLobby()
    {
        if (lobby_ != nullptr) {
            try {
                if (subscribed_) {
                    lobby_->GameStarted.Remove(started_);
                    lobby_->GameEnded.Remove(ended_);
                    lobby_->GamerJoined.Remove(joined_);
                    lobby_->GamerLeft.Remove(left_);
                    lobby_->SessionEnded.Remove(sessionEnded_);
                }
            } catch (const std::exception&) {}
            Dispose(lobby_);
        }
        subscribed_ = false;
    }

    void CloseSession()
    {
        if (find_ != nullptr && find_->getIsCompletedProperty()) {   // a search still running is picked up on the next visit
            try {
                (void)NetworkSession::EndFind(find_);
            } catch (const std::exception&) {}
            delete find_;
            find_ = nullptr;
        }
        CloseTransport();
        DisposeLobby();
        tried_ = false;
        ResetStats();
    }

    void ResetStats()
    {
        sent_ = received_ = remoteReceived_ = 0;
        remote_trail_.clear();
        trail_.clear();
        received_trail_.clear();
    }

    // The room's event list, also written to the museum log (one line per event) so a scripted run keeps the witness.
    void Log(const std::string& line)
    {
        log_.push_back(line);
        while (log_.size() > 9) log_.pop_front();
        Museum::Log("net", "%s", line.c_str());
    }

    void SetTransport(bool on)
    {
        if (!on) {
            CloseTransport();
            tried_ = false;
            OpenLobby();
            return;
        }
        try {
            ResetStats();
            DisposeLobby();   // the same profiles cannot be in a Local lobby and a SystemLink session at once (one session per process, as in XNA)
            // Create(type, maxLocalGamers, maxGamers) for the default shape; the five-argument form once a slot count was chosen.
            host_ = maxGamers_ == 8 && privateSlots_ == 0 ? NetworkSession::Create(NetType(), localGamers_, 8)
                                                          : NetworkSession::Create(NetType(), localGamers_, maxGamers_, privateSlots_, NetworkSessionProperties{});
            phase_ = Phase::Connected;
            HookHost();
            transportStatus_ = online_ ? "PlayerMatch session through the account service: packets travel by its relay" : "SystemLink session, both profiles local: packets flow in memory";
            Log(std::string("Create(") + (online_ ? "PlayerMatch" : "SystemLink") + ", " + std::to_string(localGamers_) + " local, " + std::to_string(maxGamers_) + " max, " +
                std::to_string(privateSlots_) + " private): " + std::to_string(host_->getLocalGamersProperty().getCountProperty()) + " local gamer(s)");
            if (wantReady_) SetReady(true);
        } catch (const std::exception& e) {
            CloseTransport();
            tried_ = false;
            OpenLobby();
            transportStatus_ = std::string("unavailable here: ") + std::string(e.what()).substr(0, 56);
            Museum::Log("net", "%s", transportStatus_.c_str());
        }
    }

    [[nodiscard]] NetworkSessionType NetType() const { return online_ ? NetworkSessionType::PlayerMatch : NetworkSessionType::SystemLink; }

    void AddSecondLocalGamer()
    {
        NetworkSession* n = Primary();
        if (n == nullptr) { Log("AddLocalGamer: no session"); return; }
        try {
            auto* signedIn = GamerServices::Gamer::getSignedInGamersProperty();
            for (int candidate = 0; signedIn != nullptr && candidate < signedIn->getCountProperty(); ++candidate) {
                GamerServices::SignedInGamer* gamer = (*signedIn)[candidate];
                bool alreadyLocal = false;
                for (int i = 0; i < n->getLocalGamersProperty().getCountProperty(); ++i)
                    alreadyLocal = alreadyLocal || n->getLocalGamersProperty()[i]->getSignedInGamerProperty() == gamer;
                if (alreadyLocal) continue;
                n->AddLocalGamer(gamer);
                n->Update();
                Log("AddLocalGamer: " + gamer->getGamertagProperty() + ", now " +
                    std::to_string(n->getLocalGamersProperty().getCountProperty()) + " local");
                return;
            }
            Log("AddLocalGamer: every signed-in profile is already in this session");
        } catch (const std::exception& e) {
            Log(std::string("AddLocalGamer refused: ") + std::string(e.what()).substr(0, 48));
        }
    }

    // The settings a host edits on a live session: its properties (what a search can filter on) and whether players may join once it has started.
    void ApplySessionSettings()
    {
        NetworkSession* n = Primary();
        if (n == nullptr) return;
        try {
            n->getSessionPropertiesProperty()[0] = gameMode_;
            n->setAllowJoinInProgressProperty(joinInProgress_);
            Log("SessionProperties[0] = " + std::to_string(gameMode_) + ", AllowJoinInProgress " + (joinInProgress_ ? "true" : "false"));
        } catch (const std::exception& e) {
            Log(std::string("session settings: ") + std::string(e.what()).substr(0, 50));
        }
    }

    // Both act on the packets this machine sends (XNA's NetworkSimulator sits in SendData); the in-memory delivery between the two
    // local profiles skips them.
    void ApplyImpairment()
    {
        NetworkSession* n = Primary();
        if (n == nullptr) return;
        try {
            n->setSimulatedLatencyProperty(System::TimeSpan::FromMilliseconds(static_cast<double>(latencyMs_)));
            n->setSimulatedPacketLossProperty(loss_);
        } catch (const std::exception& e) {
            Log(std::string("impairment: ") + std::string(e.what()).substr(0, 50));
        }
    }

    // A search is asynchronous: BeginFind returns at once, and the result is read when it completes.
    void BeginFind()
    {
        if (find_ != nullptr) return;
        try {
            // As in XNA, a process that is in a session cannot search: the room's own session is closed for the search and reopened after it.
            if (Primary() != nullptr) {
                CloseSession();
                phase_ = Phase::Off;
                tried_ = true;   // the room's own session reopens when the search has finished, not before
                transportStatus_ = "closed for the search: a process in a session cannot search (it reopens afterwards)";
                Log("session closed so that Find can run");
            }
            NetworkSessionProperties props;
            if (searchMode_ == 1) props[0] = gameMode_;
            if (searchMode_ == 2) props[0] = (gameMode_ + 5) % 10;
            find_ = NetworkSession::BeginFind(NetType(), localGamers_, props, {}, std::any{});   // sessions with that many open public slots
            found_ = "searching the local network...";
        } catch (const std::exception& e) {
            found_ = std::string("BeginFind: ") + std::string(e.what()).substr(0, 56);
        }
    }
    void PollFind()
    {
        if (find_ == nullptr || !find_->getIsCompletedProperty()) return;
        try {
            AvailableNetworkSessionCollection owned = NetworkSession::EndFind(find_);
            const AvailableNetworkSessionCollection& list = owned;
            delete find_;
            find_ = nullptr;
            found_ = std::to_string(list.getCountProperty()) + " session(s) found; collection " + (list.getIsDisposedProperty() ? "disposed" : "live") + (list.getCountProperty() == 0 ? (searchMode_ == 0 ? " (nobody on this network hosts a SystemLink session: start a second museum and switch packet delivery on there)" : " (no session matches those search properties)") : "");
            tried_ = false;   // the room's Local session reopens
            foundLines_.clear();
            available_.clear();
            for (int i = 0; i < list.getCountProperty(); ++i) available_.push_back(list[i]);
            for (int i = 0; i < list.getCountProperty() && i < 3; ++i) {
                const AvailableNetworkSession& a = list[i];
                char buf[220];
                const std::optional<int> mode = a.getSessionPropertiesProperty()[0];
                std::snprintf(buf, sizeof buf, "host %s: %d gamer(s), %d public / %d private slots open, mode %s, round trip %.0f ms%s", a.getHostGamertagProperty().c_str(), a.getCurrentGamerCountProperty(),
                              a.getOpenPublicGamerSlotsProperty(), a.getOpenPrivateGamerSlotsProperty(), mode ? std::to_string(*mode).c_str() : "-",
                              a.getQualityOfServiceProperty().getIsAvailableProperty() ? a.getQualityOfServiceProperty().getAverageRoundtripTimeProperty().getTotalMillisecondsProperty() : -1.0,
                              a.getQualityOfServiceProperty().getIsAvailableProperty() ? "" : " (QualityOfService not available)");
                std::string line = buf;
                const QualityOfService& q = a.getQualityOfServiceProperty();
                if (q.getIsAvailableProperty()) {
                    char qos[120];
                    std::snprintf(qos, sizeof qos, ", min %.0f ms, %d B/s down / %d up", q.getMinimumRoundtripTimeProperty().getTotalMillisecondsProperty(), q.getBytesPerSecondDownstreamProperty(), q.getBytesPerSecondUpstreamProperty());
                    line += qos;
                }
                foundLines_.push_back(line);
            }
            Museum::Log("net", "%s", found_.c_str());
            for (const std::string& line : foundLines_) Museum::Log("net", "found: %s", line.c_str());
        } catch (const std::exception& e) {
            delete find_;
            find_ = nullptr;
            found_ = std::string("EndFind: ") + std::string(e.what()).substr(0, 56);
        }
    }

    [[nodiscard]] std::string SessionShape()
    {
        NetworkSession* n = Primary();
        if (n == nullptr) return "no session";
        static const char* const types[] = {"Local", "SystemLink", "PlayerMatch", "Ranked"};
        const int t = static_cast<int>(n->getSessionTypeProperty());
        return std::string(t >= 0 && t < 4 ? types[t] : "?") + " / " + std::to_string(n->getMaxGamersProperty()) + " / " + std::to_string(n->getPrivateGamerSlotsProperty()) + ", migration " + (n->getAllowHostMigrationProperty() ? "allowed" : "off");
    }
    // What a NetworkGamer says about itself, looked up with NetworkSession.FindGamerById to show that the Id is the key.
    [[nodiscard]] std::string GamerDetails()
    {
        NetworkSession* n = Primary();
        if (n == nullptr) return "no session";
        std::string text;
        const auto& all = n->getAllGamersProperty();
        for (int i = 0; i < all.getCountProperty() && i < 2; ++i) {
            const NetworkGamer* g = all[i];
            const NetworkGamer* found = n->FindGamerById(g->getIdProperty());
            if (!text.empty()) text += "  |  ";
            text += "#" + std::to_string(static_cast<int>(g->getIdProperty())) + (found == g ? " (found by id)" : " (id lookup differs)") + ", " + (g->getIsPrivateSlotProperty() ? "private" : "public") + ", voice " + (g->getHasVoiceProperty() ? "y" : "n") +
                    ", talking " + (g->getIsTalkingProperty() ? "y" : "n") + ", muted " + (g->getIsMutedByLocalUserProperty() ? "y" : "n") + ", left " + (g->getHasLeftSessionProperty() ? "y" : "n");
        }
        return text.empty() ? "no gamers" : text;
    }
    // Voice is routed by CNA itself; the room only watches who has voice and who is talking, and logs every change
    // of a remote gamer's talking state (the witness for a voice round between two museums).
    void WatchVoice()
    {
        NetworkSession* n = host_ != nullptr ? host_ : nullptr;
        if (n == nullptr) return;
        const auto& remote = n->getRemoteGamersProperty();
        for (int i = 0; i < remote.getCountProperty(); ++i) {
            const NetworkGamer* g = remote[i];
            const bool talking = g->getIsTalkingProperty();
            const std::string key = g->getGamertagProperty() + "#" + std::to_string(static_cast<int>(g->getIdProperty()));
            if (talking != talking_[key]) {
                talking_[key] = talking;
                Log(std::string("voice: ") + key + (talking ? " started talking" : " stopped talking") + " (HasVoice " + (g->getHasVoiceProperty() ? "true" : "false") + ")");
            }
        }
    }
    [[nodiscard]] std::string GamerRoles()
    {
        NetworkSession* n = Primary();
        if (n == nullptr) return "no session";
        const NetworkGamer* host = n->getHostProperty();
        return std::string(host != nullptr ? host->getGamertagProperty() : std::string("none")) + " / " + std::to_string(n->getRemoteGamersProperty().getCountProperty()) + " / " + std::to_string(n->getPreviousGamersProperty().getCountProperty());
    }
    [[nodiscard]] std::string MachineDetails()
    {
        NetworkSession* n = Primary();
        if (n == nullptr || n->getAllGamersProperty().getCountProperty() == 0) return "no gamer machine";
        const NetworkGamer* g = n->getAllGamersProperty()[0];
        const NetworkMachine& machine = g->getMachineProperty();
        return std::to_string(machine.getGamersProperty().getCountProperty()) + " gamer(s), session " +
               (g->getSessionProperty() == n ? "same" : "DIFFERENT") + ", guest " +
               (g->getIsGuestProperty() ? "yes" : "no");
    }
    void RemoveRemoteMachine()
    {
        NetworkSession* n = Primary();
        if (n == nullptr) { Log("RemoveFromSession: no session"); return; }
        try {
            const auto& all = n->getAllGamersProperty();
            for (int i = 0; i < all.getCountProperty(); ++i) {
                NetworkGamer* g = all[i];
                if (g != nullptr && !g->getIsLocalProperty()) {
                    const std::string name = g->getGamertagProperty();
                    g->getMachineProperty().RemoveFromSession();
                    Log("RemoveFromSession: removed " + name + "'s machine");
                    return;
                }
            }
            Log("RemoveFromSession: no remote machine (join from a second museum first)");
        } catch (const std::exception& e) {
            Log(std::string("RemoveFromSession refused: ") + std::string(e.what()).substr(0, 48));
        }
    }
    [[nodiscard]] bool HasRemoteGamers() const
    {
        if (host_ == nullptr) return false;
        try {
            for (int i = 0; i < host_->getAllGamersProperty().getCountProperty(); ++i)
                if (!host_->getAllGamersProperty()[i]->getIsLocalProperty()) return true;
        } catch (const std::exception&) {}
        return false;
    }

    // Joining is asynchronous too. The joined session replaces the room's own: its gamers are the two local profiles here and whoever the host has.
    void BeginJoin()
    {
        if (find_ != nullptr || join_ != nullptr) return;
        if (available_.empty()) {
            found_ = "nothing to join: press BeginFind first (and start a second museum with packet delivery on)";
            Museum::Log("net", "%s", found_.c_str());
            return;
        }
        try {
            CloseSession();
            tried_ = true;
            ResetStats();
            join_ = NetworkSession::BeginJoin(&available_[0], {}, std::any{});
            found_ = "joining " + available_[0].getHostGamertagProperty() + "'s session...";
            transportStatus_ = "joining";
        } catch (const std::exception& e) {
            found_ = std::string("BeginJoin: ") + std::string(e.what()).substr(0, 56);
            tried_ = false;
        }
    }
    [[nodiscard]] static const char* EndReasonName(NetworkSessionEndReason r)
    {
        switch (r) {
            case NetworkSessionEndReason::ClientSignedOut: return "ClientSignedOut";
            case NetworkSessionEndReason::HostEndedSession: return "HostEndedSession";
            case NetworkSessionEndReason::RemovedByHost: return "RemovedByHost";
            default: return "Disconnected";
        }
    }
    [[nodiscard]] static const char* JoinErrorName(NetworkSessionJoinError e)
    {
        switch (e) {
            case NetworkSessionJoinError::SessionNotFound: return "SessionNotFound";
            case NetworkSessionJoinError::SessionNotJoinable: return "SessionNotJoinable";
            default: return "SessionFull";
        }
    }
    // The events a session over a real transport raises: the end of it with its reason, a new host after host migration, players leaving.
    void HookHost()
    {
        if (host_ == nullptr || hostHooked_) return;
        hostEnded_ = host_->SessionEnded.Add([this](System::Object*, const NetworkSessionEndedEventArgs& e) { Log(std::string("SessionEnded: ") + EndReasonName(e.getEndReasonProperty())); phase_ = Phase::Failed; transportStatus_ = std::string("the session ended: ") + EndReasonName(e.getEndReasonProperty()); });
        hostChanged_ = host_->HostChanged.Add([this](System::Object*, const HostChangedEventArgs& e) {
            Log(std::string("HostChanged: ") + (e.getOldHostProperty() != nullptr ? e.getOldHostProperty()->getGamertagProperty() : "?") + " -> " + (e.getNewHostProperty() != nullptr ? e.getNewHostProperty()->getGamertagProperty() : "?"));
        });
        hostLeft_ = host_->GamerLeft.Add([this](System::Object*, const GamerLeftEventArgs&) { Log("GamerLeft"); });
        hostJoined_ = host_->GamerJoined.Add([this](System::Object*, const GamerJoinedEventArgs& e) {
            const NetworkGamer* joined = e.getGamerProperty();
            Log("GamerJoined: " + (joined != nullptr ? joined->getGamertagProperty() : std::string("?")) + (joined != nullptr && joined->getIsLocalProperty() ? " (local" : " (remote") +
                (joined != nullptr && joined->getIsPrivateSlotProperty() ? ", private slot)" : ", public slot)"));
        });
        hostStarted_ = host_->GameStarted.Add([this](System::Object*, const GameStartedEventArgs&) { Log("GameStarted"); });
        hostEndedGame_ = host_->GameEnded.Add([this](System::Object*, const GameEndedEventArgs&) { Log("GameEnded"); });
        hostHooked_ = true;
    }
    void UnhookHost()
    {
        if (host_ == nullptr || !hostHooked_) return;
        try {
            host_->SessionEnded.Remove(hostEnded_);
            host_->HostChanged.Remove(hostChanged_);
            host_->GamerLeft.Remove(hostLeft_);
            host_->GamerJoined.Remove(hostJoined_);
            host_->GameStarted.Remove(hostStarted_);
            host_->GameEnded.Remove(hostEndedGame_);
        } catch (const std::exception&) {}
        hostHooked_ = false;
    }

    // Asks the Guide to invite the friends to this machine's online session.
    // LocalNetworkGamer.SendPartyInvites: every member of the gamer's party is invited to this session (as Guide.ShowGameInvite does for chosen friends).
    void SendPartyInvites()
    {
        NetworkSession* n = Primary();
        if (n == nullptr || n->getLocalGamersProperty().getCountProperty() == 0) { Log("SendPartyInvites: no session"); return; }
        try {
            n->getLocalGamersProperty()[0]->SendPartyInvites();
            Log("SendPartyInvites: sent by " + n->getLocalGamersProperty()[0]->getGamertagProperty());
        } catch (const std::exception& e) {
            Log(std::string("SendPartyInvites refused: ") + std::string(e.what()).substr(0, 64));
        }
    }

    void InviteFriends()
    {
        try {
            std::vector<Microsoft::Xna::Framework::GamerServices::Gamer*> to;
            auto* gamers = Microsoft::Xna::Framework::GamerServices::Gamer::getSignedInGamersProperty();
            Microsoft::Xna::Framework::GamerServices::FriendCollection friends = gamers != nullptr && gamers->getCountProperty() > 0 ? (*gamers)[0]->GetFriends() : Microsoft::Xna::Framework::GamerServices::FriendCollection::CreateInternal({});
            for (int i = 0; i < friends.getCountProperty() && i < 8; ++i) to.push_back(friends[i]);
            Microsoft::Xna::Framework::GamerServices::Guide::ShowGameInvite(Microsoft::Xna::Framework::PlayerIndex::One, to);
            Museum::Log("online", "ShowGameInvite opened");
            Log("Guide.ShowGameInvite opened for " + std::to_string(to.size()) + " friend(s)");
        } catch (const std::exception& e) {
            Log(std::string("ShowGameInvite: ") + std::string(e.what()).substr(0, 60));
        }
    }
    // The invitation was accepted in the Guide: the same asynchronous join, through the invitation.
    void BeginJoinInvited()
    {
        try {
            CloseSession();
            tried_ = true;
            ResetStats();
            joinInvited_ = true;
            join_ = NetworkSession::BeginJoinInvited(2, {}, std::any{});
            found_ = "joining the invitation...";
        } catch (const std::exception& e) {
            found_ = std::string("BeginJoinInvited: ") + std::string(e.what()).substr(0, 56);
            tried_ = false;
            joinInvited_ = false;
        }
    }
    void PollJoin()
    {
        if (inviteAccepted_ && join_ == nullptr && find_ == nullptr) {
            inviteAccepted_ = false;
            BeginJoinInvited();
        }
        if (join_ == nullptr || !join_->getIsCompletedProperty()) return;
        try {
            host_ = joinInvited_ ? NetworkSession::EndJoinInvited(join_) : NetworkSession::EndJoin(join_);
            joinInvited_ = false;
            delete join_;
            join_ = nullptr;
            phase_ = Phase::Connected;
            HookHost();
            transportStatus_ = "joined a session on another museum: packets travel over the network";
            found_ = "joined";
            ApplyImpairment();
            Log("Joined: " + std::to_string(host_->getAllGamersProperty().getCountProperty()) + " gamer(s) in the session; this machine sees AllowHostMigration " +
                (host_->getAllowHostMigrationProperty() ? "true" : "false") + ", AllowJoinInProgress " + (host_->getAllowJoinInProgressProperty() ? "true" : "false") + ", state " + StateName(host_->getSessionStateProperty()));
            if (wantReady_) SetReady(true);
        } catch (const NetworkSessionJoinException& e) {
            delete join_;
            join_ = nullptr;
            joinInvited_ = false;
            found_ = std::string("EndJoin refused: NetworkSessionJoinException, JoinError ") + JoinErrorName(e.getJoinErrorProperty());
            Log(found_);
            tried_ = false;
        } catch (const std::exception& e) {
            delete join_;
            join_ = nullptr;
            found_ = std::string("EndJoin: ") + std::string(e.what()).substr(0, 56);
            tried_ = false;
        }
    }

    // The session the lobby controls act on: the Local lobby, or the SystemLink host while the real transport is on. A signed-in gamer can
    // belong to one session at a time, so the two are never open together.
    [[nodiscard]] NetworkSession* Primary() const { return host_ != nullptr ? host_ : lobby_; }
    void SetReady(bool v)
    {
        wantReady_ = v;
        NetworkSession* n = Primary();
        if (n == nullptr || n->getSessionStateProperty() != NetworkSessionState::Lobby) return;   // IsReady can only change in the lobby
        for (int i = 0; i < n->getLocalGamersProperty().getCountProperty(); ++i) n->getLocalGamersProperty()[i]->setIsReadyProperty(v);
    }

    [[nodiscard]] Vector2 Truth(float t) const { return Vector2(std::sin(t * 1.3f) * 0.85f, std::sin(t * 2.1f + 0.7f) * 0.8f); }

    // One frame: the Visitor sends at `rate_` per second, the Guest (the second local gamer) drains what arrived.
    void Pump(float dt)
    {
        if (host_ == nullptr || phase_ != Phase::Connected) return;
        try {
            host_->Update();
            if (burstLastAt_ >= 0.0f && time_ - burstLastAt_ > 3.0f) ReportBurst();
            if (host_->getLocalGamersProperty().getCountProperty() < 1) return;
            // Two local gamers (the offline profiles) talk to each other; one account online talks to whoever else is in the session.
            LocalNetworkGamer* sender = host_->getLocalGamersProperty()[0];
            LocalNetworkGamer* guest = host_->getLocalGamersProperty().getCountProperty() > 1 ? host_->getLocalGamersProperty()[1] : sender;
            if (burstArmed_ && HasRemoteGamers()) {
                if (burstArmedAt_ < 0.0f) burstArmedAt_ = time_;
                if (time_ - burstArmedAt_ >= 2.0f) {
                    burstArmed_ = false;
                    static constexpr SendDataOptions kOptions[4] = {SendDataOptions::None, SendDataOptions::Reliable, SendDataOptions::InOrder, SendDataOptions::ReliableInOrder};
                    for (int o = 0; o < 4; ++o)
                        for (int i = 0; i < kBurstCount; ++i) {
                            PacketWriter w;
                            w.Write(kBurstMagic);
                            w.Write(o);
                            w.Write(i);
                            sender->SendData(w, kOptions[o]);
                        }
                    Log("burst sent: 4 x " + std::to_string(kBurstCount) + " packets (None, Reliable, InOrder, ReliableInOrder) with SimulatedPacketLoss " + LossText() +
                        ", SimulatedLatency " + std::to_string(static_cast<int>(latencyMs_)) + " ms");
                }
            }
            sendClock_ += dt * rate_;
            while (sendClock_ >= 1.0f) {
                sendClock_ -= 1.0f;
                PacketWriter w;
                const Vector2 p = Truth(time_);
                w.Write(sequence_++);
                w.Write(time_);
                w.Write(p.X);
                w.Write(p.Y);
                sender->SendData(w, SendDataOptions::InOrder);
                ++sent_;
                trail_.push_back(p);
                if (trail_.size() > 60) trail_.pop_front();
            }
            while (guest != sender && sender->getIsDataAvailableProperty()) {   // the sender's own copy: drained so the queue does not grow
                PacketReader r;
                NetworkGamer* from = nullptr;
                sender->ReceiveData(r, from);
            }
            while (guest->getIsDataAvailableProperty()) {
                PacketReader r;
                NetworkGamer* from = nullptr;
                guest->ReceiveData(r, from);
                const int seq = r.ReadInt32();
                if (seq == kBurstMagic) {
                    const int option = r.ReadInt32(), index = r.ReadInt32();
                    if (from == nullptr || from->getIsLocalProperty() || option < 0 || option > 3 || index < 0 || index >= kBurstCount) continue;
                    BurstTally& t = burst_[static_cast<std::size_t>(option)];
                    if (t.seen[static_cast<std::size_t>(index)]) ++t.duplicates;
                    else { t.seen[static_cast<std::size_t>(index)] = true; ++t.received; }
                    if (index < t.last) ++t.outOfOrder;
                    t.last = std::max(t.last, index);
                    burstLastAt_ = time_;
                    burstFrom_ = from->getGamertagProperty();
                    continue;
                }
                const float sentAt = r.ReadSingle();
                const Vector2 p(r.ReadSingle(), r.ReadSingle());
                if (from != nullptr && !from->getIsLocalProperty()) {
                    ++remoteReceived_;
                    ++remoteWindow_;
                    remote_trail_.push_back(p);
                    if (remote_trail_.size() > 60) remote_trail_.pop_front();
                    continue;
                }
                if (guest == sender) continue;   // one local gamer: its own packets coming back are not "received"
                ++received_;
                lastSeq_ = seq;
                ageMs_ = (time_ - sentAt) * 1000.0f;
                received_trail_.push_back(p);
                if (received_trail_.size() > 60) received_trail_.pop_front();
            }
        } catch (const std::exception& e) {
            status_ = std::string("session error: ") + std::string(e.what()).substr(0, 50);
        }
    }

    [[nodiscard]] std::string LossText() const
    {
        char buffer[16];
        std::snprintf(buffer, sizeof buffer, "%.2f", static_cast<double>(loss_));
        return buffer;
    }
    // What arrived of a burst, per option, once no burst packet came for three seconds.
    void ReportBurst()
    {
        static const char* const names[4] = {"None", "Reliable", "InOrder", "ReliableInOrder"};
        std::string line = "burst received from " + burstFrom_ + ":";
        for (int o = 0; o < 4; ++o) {
            const BurstTally& t = burst_[static_cast<std::size_t>(o)];
            line += std::string(o ? ";" : "") + " " + names[o] + " " + std::to_string(t.received) + "/" + std::to_string(kBurstCount) + " (" + std::to_string(t.outOfOrder) + " out of order, " +
                    std::to_string(t.duplicates) + " duplicates)";
        }
        Log(line);
        burst_ = {};
        burstLastAt_ = -1.0f;
    }

    template <typename Rect, typename Text>
    void DrawSession(Rect& rect, Text& text)
    {
        if (lobby_ == nullptr && host_ == nullptr) {
            text(status_, 24, 90, Kit::Rgba(255, 190, 150), 0.65f);
            return;
        }
        const float px = 24.0f, py = 100.0f, size = 300.0f;
        const bool remote = HasRemoteGamers();
        for (int pane = 0; pane < 2; ++pane) {
            const float x = px + static_cast<float>(pane) * 330.0f;
            rect(x, py, size, size, Color(18, 24, 38, 240));
            rect(x, py, size, 3, pane == 0 ? Kit::kGold : Kit::Rgba(110, 170, 255));
            text(pane == 0 ? "the Visitor's dot (what is sent)" : (remote ? "what arrives from the other museum" : "what the Guest receives"), x, py - 28, pane == 0 ? Kit::kGold : Kit::Rgba(150, 190, 255), 0.55f);
            const std::deque<Vector2>& points = pane == 0 ? trail_ : (remote ? remote_trail_ : received_trail_);
            for (std::size_t i = 0; i < points.size(); ++i) {
                const float a = 0.25f + 0.75f * static_cast<float>(i) / static_cast<float>(std::max<std::size_t>(1, points.size()));
                const float cx = x + size * 0.5f + points[i].X * size * 0.5f, cy = py + size * 0.5f + points[i].Y * size * 0.5f;
                const bool newest = i + 1 == points.size();
                const float r = newest ? 8.0f : 4.0f;
                rect(cx - r, cy - r, r * 2, r * 2, newest ? (pane == 0 ? Color(250, 210, 80, 255) : Color(120, 190, 255, 255)) : Kit::Rgba(static_cast<int>(200 * a), static_cast<int>(200 * a), static_cast<int>(230 * a)));
            }
            if (points.empty()) text(phase_ == Phase::Off ? "A Local session delivers no packets: switch packet delivery on." : "waiting...", x + 14, py + size * 0.5f - 10, Kit::kDim, 0.5f);
        }
        const float lost = sent_ > 0 ? 100.0f * static_cast<float>(sent_ - received_) / static_cast<float>(sent_) : 0.0f;
        char line[200];
        std::snprintf(line, sizeof line, "sent %d   received %d   missing %.0f %%   last arrived #%d, %.0f ms old   %d B/s out   %d B/s in", sent_, received_, static_cast<double>(std::max(0.0f, lost)), lastSeq_,
                      static_cast<double>(ageMs_), host_ != nullptr ? host_->getBytesPerSecondSentProperty() : 0, host_ != nullptr ? host_->getBytesPerSecondReceivedProperty() : 0);
        if (host_ != nullptr && host_->getLocalGamersProperty().getCountProperty() < 2) {   // one account: nothing comes back from itself
            std::snprintf(line, sizeof line, "sent %d   %d B/s out   %d B/s in", sent_, host_->getBytesPerSecondSentProperty(), host_->getBytesPerSecondReceivedProperty());
        }
        text(line, 24, 420, Kit::kText, 0.5f);
        if (remote) text("from remote gamers: " + std::to_string(remoteReceived_) + " packets, " + std::to_string(static_cast<int>(remoteRate_ + 0.5f)) + " per second (the other museum sends " + std::to_string(static_cast<int>(rate_)) + ")", 24, 395 + 0.0f, Kit::Rgba(150, 190, 255), 0.5f);
        if (host_ != nullptr) {
            text("gamers in the session", 670, 330, Kit::kDim, 0.55f);
            try {
                float gy = 354.0f;
                for (int i = 0; i < host_->getAllGamersProperty().getCountProperty() && i < 6; ++i) {
                    NetworkGamer* g = host_->getAllGamersProperty()[i];
                    char gl[160];
                    std::snprintf(gl, sizeof gl, "%s  %s%s  round trip %.0f ms", g->getGamertagProperty().c_str(), g->getIsLocalProperty() ? "local" : "REMOTE", g->getIsHostProperty() ? ", host" : "",
                                  g->getRoundtripTimeProperty().getTotalMillisecondsProperty());
                    text(gl, 670, gy, g->getIsLocalProperty() ? Kit::kText : Kit::Rgba(150, 190, 255), 0.45f);
                    gy += 20.0f;
                }
            } catch (const std::exception&) {}
        }
        text("events", 670, 70, Kit::kDim, 0.55f);
        float y = 96.0f;
        for (const std::string& l : log_) {
            text(l, 670, y, Kit::Rgba(190, 235, 200), 0.45f);
            y += 24.0f;
        }
        {
            float fy = 480.0f;
            text("find: " + found_, 24, fy, Kit::kText, 0.5f);
            for (const std::string& l : foundLines_) {
                fy += 20.0f;
                text(l, 40, fy, Kit::Rgba(190, 235, 200), 0.45f);
            }
        }
        text("transport: " + transportStatus_, 24, 450, phase_ == Phase::Failed ? Kit::Rgba(255, 190, 150) : Kit::kDim, 0.5f);
    }

    enum class Phase { Off, Connected, Failed };
    std::array<bool, 8> fields_ = {true, true, true, false, true, true, false, false};
    int stringLength_ = 12, size_ = 0;
    bool dirty_ = true;
    std::vector<SharpRuntime::bytecs> bytes_;
    std::string roundTrip_ = "-";

    bool wantReady_ = false;
    NetworkSession* lobby_ = nullptr;
    NetworkSession* host_ = nullptr;
    Phase phase_ = Phase::Off;
    std::string transportStatus_ = "off: no packets leave this process";
    std::size_t started_ = 0, ended_ = 0, joined_ = 0, left_ = 0, sessionEnded_ = 0;
    bool subscribed_ = false, tried_ = false;
    float time_ = 0.0f, rate_ = 20.0f, sendClock_ = 0.0f, ageMs_ = 0.0f;
    int sent_ = 0, received_ = 0, lastSeq_ = 0, sequence_ = 0;
    std::deque<Vector2> trail_, received_trail_;
    std::deque<std::string> log_;
    std::map<std::string, bool> talking_;
    int localGamers_ = 2;
    int maxGamers_ = 8;
    int privateSlots_ = 0;
    std::string status_ = "-";
    int gameMode_ = 3, searchMode_ = 0, remoteReceived_ = 0;
    std::size_t hostEnded_ = 0, hostChanged_ = 0, hostLeft_ = 0, hostJoined_ = 0, hostStarted_ = 0, hostEndedGame_ = 0;
    // The burst (backlog N06): 100 numbered packets per SendDataOptions value, judged by the museum that receives them.
    static constexpr int kBurstMagic = -0x42525354;   // first int32 of a burst packet; the streamed packets start with a sequence >= 0
    static constexpr int kBurstCount = 100;
    bool burstArmed_ = false;
    float burstArmedAt_ = -1.0f, burstLastAt_ = -1.0f;
    struct BurstTally {
        int received = 0, duplicates = 0, outOfOrder = 0, last = -1;
        std::vector<bool> seen = std::vector<bool>(kBurstCount, false);
    };
    std::array<BurstTally, 4> burst_{};
    std::string burstFrom_;
    bool hostHooked_ = false;
    bool online_ = false, inviteAccepted_ = false, inviteSubscribed_ = false, joinInvited_ = false;
    std::size_t inviteToken_ = 0;
    float latencyMs_ = 0.0f, loss_ = 0.0f, remoteRate_ = 0.0f, windowClock_ = 0.0f;
    int remoteWindow_ = 0;
    std::deque<Vector2> remote_trail_;
    std::vector<AvailableNetworkSession> available_;
    System::IAsyncResult* join_ = nullptr;
    bool joinInProgress_ = false;
    System::IAsyncResult* find_ = nullptr;
    std::string found_ = "not searched yet";
    std::vector<std::string> foundLines_;
#else
    void CloseSession() {}
#endif
    Mode mode_;
};

}  // namespace

std::unique_ptr<Exhibit> MakeNetLab(const RoomDef& room, Services& services)
{
    return std::make_unique<NetLab>(room, services);
}
