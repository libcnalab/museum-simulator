// Push notifications (Microsoft.Phone.Notification): a channel that is its own endpoint. The room opens an
// HttpNotificationChannel, learns its loopback URI from ChannelUriUpdated, finds it again by name, posts raw and
// toast messages to it (RawPushNotificationMessage and ToastPushNotificationMessage, declared in PushNotificationSender.hpp),
// and raises what arrived on the game's own thread through
// DispatchPendingNotificationsEXT. Closing, reopening and sending to a closed channel are part of the round:
// a closed channel must refuse new messages; what had already arrived is still raised by the next dispatch.
//
// Everything stays on 127.0.0.1, so the sweep may run it. Each step logs one [museum][notify] line with the
// exact value it saw, which execution evidence keeps as the witness.
#include <exception>
#include <memory>
#include <string>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#ifdef MUSEUM_PHONE
#include "Microsoft/Phone/Notification/HttpNotificationChannel.hpp"
#include "Microsoft/Phone/Notification/PushNotificationSender.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

#ifdef MUSEUM_PHONE
namespace Notify = Microsoft::Phone::Notification;
#endif

constexpr const char* kChannel = "museum-notify-room";

class NotificationLab final : public ScreenExhibit {
public:
    NotificationLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("HttpNotificationChannel", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddButton("Open the channel", [this] { Open(); });
        p.AddButton("Send a raw notification", [this] { SendRaw(); });
        p.AddButton("Send a toast notification", [this] { SendToast(); });
        p.AddButton("BindToShellToast", [this] { BindToast(); });
        p.AddButton("Close, then send to the closed channel", [this] { CloseAndSendLate(); });
        p.AddButton("Send to an address nobody listens on", [this] { SendNowhere(); });
        p.AddReadout("Channel URI", [this] { return uri_; });
        p.AddReadout("Find(name)", [this] { return FindText(); });
        p.AddReadout("Received (raw / toast)", [this] { return std::to_string(raw_) + " / " + std::to_string(toast_); });
        p.AddReadout("Last body", [this] { return lastBody_; });
        p.AddReadout("Last send", [this] { return lastSend_; });
        AddInfoSign(b, "Push notifications without a push service",
                    {"A channel opens its own HTTP listener and hands out that address; a sender posts to it. What arrives is queued on the listener",
                     "thread and raised on the game's thread by DispatchPendingNotificationsEXT. There is no shell here, so a toast arrives as data only."});
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
#ifdef MUSEUM_PHONE
        if (channel_ != nullptr) {
            try {
                channel_->DispatchPendingNotificationsEXT();
            } catch (const std::exception& e) {
                ReportError(std::string("DispatchPendingNotificationsEXT: ") + e.what());
            }
        }
#endif
    }

    void OnLeave() override { Close(); }
    ~NotificationLab() override { Close(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        text("Push Notifications", 24, 12, Kit::kGold, 1.0f);
#ifdef MUSEUM_PHONE
        const std::string lines[] = {"Channel: " + std::string(channel_ != nullptr ? kChannel : "closed") + "   URI: " + uri_, "Find(\"" + std::string(kChannel) + "\"): " + FindText(),
                                     "ChannelUriUpdated raised " + std::to_string(uriEvents_) + " time(s), shell toast bound: " + toastBound_,
                                     "Received raw " + std::to_string(raw_) + ", toast " + std::to_string(toast_) + ", last: " + lastBody_, "Last send: " + lastSend_};
        float y = 70.0f;
        for (const std::string& l : lines) {
            text(l, 30, y, Kit::kText, 0.68f);
            y += 40.0f;
        }
#else
        text("This build of the museum was compiled without CNA's phone module (cna_phone).", 24, 100, Kit::Rgba(255, 190, 150), 0.8f);
#endif
        sb.End();
    }

private:
    [[nodiscard]] std::string FindText() const
    {
#ifdef MUSEUM_PHONE
        const Notify::HttpNotificationChannel* found = Notify::HttpNotificationChannel::Find(kChannel);
        if (found == nullptr) return "null (no open channel of that name)";
        return found == channel_.get() ? "this room's channel" : "another channel";
#else
        return "-";
#endif
    }

    void Open()
    {
#ifdef MUSEUM_PHONE
        try {
            if (channel_ == nullptr) {
                channel_ = std::make_unique<Notify::HttpNotificationChannel>(kChannel, "museum");
                uriToken_ = channel_->ChannelUriUpdated.Add([this](System::Object*, const Notify::NotificationChannelUriEventArgs& e) {
                    ++uriEvents_;
                    uri_ = e.getChannelUriProperty().ToString();
                });
                receivedToken_ = channel_->HttpNotificationReceived.Add([this](System::Object*, const Notify::HttpNotificationEventArgs& e) {
                    const std::string body = e.getBodyAsStringProperty();
                    lastBody_ = body.substr(0, 80);
                    if (body.find("<wp:Toast>") != std::string::npos) ++toast_;   // the toast payload PushNotificationSender writes
                    else ++raw_;
                    Museum::Log("notify", "received %zu byte(s): %s", e.getBodyProperty().size(), lastBody_.c_str());
                });
            }
            channel_->Open();
            const System::Uri* uri = channel_->getChannelUriProperty();
            Museum::Log("notify", "open: ChannelUriUpdated %d time(s), URI %s, Find %s", uriEvents_, uri != nullptr ? uri->ToString().c_str() : "null", FindText().c_str());
        } catch (const std::exception& e) {
            lastSend_ = std::string("Open refused: ") + e.what();
            Museum::Log("notify", "%s", lastSend_.c_str());
        }
#endif
    }

    void SendRaw()
    {
#ifdef MUSEUM_PHONE
        if (channel_ == nullptr || channel_->getChannelUriProperty() == nullptr) Open();
        const System::Uri* uri = channel_ != nullptr ? channel_->getChannelUriProperty() : nullptr;
        if (uri == nullptr) return;
        Notify::RawPushNotificationMessage message(Notify::MessageSendPriority::High);
        const std::string body = "museum raw #" + std::to_string(++sent_);
        message.RawData.assign(body.begin(), body.end());
        const bool accepted = message.SendAsync(*uri);
        lastSend_ = "raw \"" + body + "\": " + (accepted ? "accepted" : "refused");
        Museum::Log("notify", "send %s", lastSend_.c_str());
#endif
    }

    void SendToast()
    {
#ifdef MUSEUM_PHONE
        if (channel_ == nullptr || channel_->getChannelUriProperty() == nullptr) Open();
        const System::Uri* uri = channel_ != nullptr ? channel_->getChannelUriProperty() : nullptr;
        if (uri == nullptr) return;
        Notify::ToastPushNotificationMessage message;
        message.Title = "CNA 3D Museum";
        message.SubTitle = "toast #" + std::to_string(++sent_);
        const bool accepted = message.SendAsync(*uri);
        lastSend_ = "toast \"" + message.SubTitle + "\": " + (accepted ? "accepted" : "refused");
        Museum::Log("notify", "send %s", lastSend_.c_str());
#endif
    }

    void BindToast()
    {
#ifdef MUSEUM_PHONE
        if (channel_ == nullptr) Open();
        if (channel_ == nullptr) return;
        channel_->BindToShellToast();
        toastBound_ = channel_->getIsShellToastBoundProperty() ? "yes (recorded only: no shell displays it)" : "no";
        Museum::Log("notify", "BindToShellToast: IsShellToastBound %s", toastBound_.c_str());
#endif
    }

    void CloseAndSendLate()
    {
#ifdef MUSEUM_PHONE
        if (channel_ == nullptr || channel_->getChannelUriProperty() == nullptr) Open();
        if (channel_ == nullptr || channel_->getChannelUriProperty() == nullptr) return;
        const System::Uri uri = *channel_->getChannelUriProperty();
        channel_->Close();
        Notify::RawPushNotificationMessage late;
        const std::string body = "museum late";
        late.RawData.assign(body.begin(), body.end());
        const bool accepted = late.SendAsync(uri);
        // Messages that arrived while the channel was open but were not dispatched yet are still raised after Close.
        const int before = raw_ + toast_;
        channel_->DispatchPendingNotificationsEXT();
        lastSend_ = std::string("after Close: send ") + (accepted ? "accepted (unexpected)" : "refused") + ", queued before Close and raised after it " + std::to_string(raw_ + toast_ - before) + ", URI " +
                    (channel_->getChannelUriProperty() != nullptr ? "still set" : "null") + ", Find " + FindText();
        Museum::Log("notify", "%s", lastSend_.c_str());
        if (accepted) ReportError("a closed notification channel accepted a message");
#endif
    }

    void SendNowhere()
    {
#ifdef MUSEUM_PHONE
        Notify::RawPushNotificationMessage message;
        message.RawData = {'x'};
        // Port 9 (discard) on loopback: nothing of ours listens there, so the sender must report a refusal.
        const bool accepted = message.SendAsync(System::Uri("http://127.0.0.1:9/notify"));
        lastSend_ = std::string("to http://127.0.0.1:9/notify: ") + (accepted ? "accepted" : "refused");
        Museum::Log("notify", "send %s", lastSend_.c_str());
#endif
    }

    void Close()
    {
#ifdef MUSEUM_PHONE
        if (channel_ == nullptr) return;
        try {
            channel_->Close();
            channel_->ChannelUriUpdated.Remove(uriToken_);
            channel_->HttpNotificationReceived.Remove(receivedToken_);
        } catch (const std::exception& e) {
            Museum::Log("notify", "closing: %s", e.what());
        }
        channel_.reset();
        uri_ = "-";
#endif
    }

#ifdef MUSEUM_PHONE
    std::unique_ptr<Notify::HttpNotificationChannel> channel_;
#endif
    std::size_t uriToken_ = 0, receivedToken_ = 0;
    int uriEvents_ = 0, raw_ = 0, toast_ = 0, sent_ = 0;
    std::string uri_ = "-", lastBody_ = "-", lastSend_ = "-", toastBound_ = "no";
};

}  // namespace

std::unique_ptr<Exhibit> MakeNotificationLab(const RoomDef& room, Services& services)
{
    return std::make_unique<NotificationLab>(room, services);
}
