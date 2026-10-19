// Inspector laboratory: CNA's optional Inspector. An Agent serves the engine's diagnostics (metrics, frame history, resource metadata)
// over an authenticated TCP socket on the loopback address; a Client connects, negotiates, and asks for snapshots and events. A real
// Inspector is a separate process (cna-inspector, with a browser page); this room starts the agent and plays the client in the same process
// so every step and every number can be seen. The agent is never started until the visitor presses its button, and it only listens on 127.0.0.1.
// Without CNA's Inspector module (-DMUSEUM_INSPECTOR=OFF) the room says so.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "CNA/Diagnostics/Diagnostics.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "System/IO/MemoryStream.hpp"

#ifdef MUSEUM_INSPECTOR
#include "CNA/Inspector/Agent.hpp"
#include "CNA/Inspector/Client.hpp"
#include "CNA/Inspector/Protocol.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

#ifdef MUSEUM_INSPECTOR
namespace Insp = CNA::Inspector;

std::string CapabilityNames(std::uint64_t bits)
{
    static const struct { Insp::Capability bit; const char* name; } kNames[] = {
        {Insp::Capability::Snapshots, "Snapshots"},         {Insp::Capability::Events, "Events"},
        {Insp::Capability::ResourceMetadata, "ResourceMetadata"}, {Insp::Capability::CpuZones, "CpuZones"},
        {Insp::Capability::Accuracy, "Accuracy"},           {Insp::Capability::Discontinuities, "Discontinuities"},
        {Insp::Capability::ResourcePreviews, "ResourcePreviews"}};
    std::string text;
    for (const auto& n : kNames)
        if ((bits & static_cast<std::uint64_t>(n.bit)) != 0) text += (text.empty() ? "" : ", ") + std::string(n.name);
    return text.empty() ? "none" : text;
}

// The museum's preview source (the agent's IResourcePreviewProvider). It serves one resource: a texture the room made and
// registered with a diagnostics ResourceHandle. The agent calls it on its network thread, where nothing may touch the
// graphics device, so a request is only queued; the room encodes the texture on the game thread (Update) and the next
// poll gets the PNG.
class RoomPreviewProvider final : public Insp::IResourcePreviewProvider {
public:
    explicit RoomPreviewProvider(CNA::Diagnostics::ResourceId id) : id_(id) {}

    [[nodiscard]] Insp::PreviewResponse RequestPreview(const Insp::PreviewRequest& request) override
    {
        Insp::PreviewResponse response;
        if (request.resourceId != id_) {
            response.status = Insp::PreviewStatus::Unavailable;
            response.message = "only this room's own texture has a preview";
            return response;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        response.status = Insp::PreviewStatus::Pending;
        response.ticket = ++nextTicket_;
        pending_.push_back(response.ticket);
        return response;
    }

    [[nodiscard]] Insp::PreviewResponse PollPreview(std::uint64_t ticket) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Insp::PreviewResponse response;
        response.ticket = ticket;
        for (auto it = ready_.begin(); it != ready_.end(); ++it)
            if (it->ticket == ticket) {
                response = std::move(*it);
                ready_.erase(it);
                return response;
            }
        const bool waiting = std::find(pending_.begin(), pending_.end(), ticket) != pending_.end();
        response.status = waiting ? Insp::PreviewStatus::Pending : Insp::PreviewStatus::Failed;
        if (!waiting) response.message = "unknown or already collected ticket";
        return response;
    }

    /** Game thread: the tickets waiting for an image. */
    [[nodiscard]] std::vector<std::uint64_t> TakePending()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::uint64_t> tickets;
        tickets.swap(pending_);
        return tickets;
    }

    void Complete(Insp::PreviewResponse response)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_.push_back(std::move(response));
    }

private:
    CNA::Diagnostics::ResourceId id_;
    std::mutex mutex_;
    std::uint64_t nextTicket_ = 0;
    std::vector<std::uint64_t> pending_;
    std::vector<Insp::PreviewResponse> ready_;
};

const char* PreviewName(Insp::PreviewStatus s)
{
    switch (s) {
        case Insp::PreviewStatus::Pending: return "Pending";
        case Insp::PreviewStatus::Ready: return "Ready";
        case Insp::PreviewStatus::Failed: return "Failed";
        default: return "Unavailable";
    }
}
#endif

class InspectorLab final : public ScreenExhibit {
public:
    InspectorLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("CNA Inspector", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_INSPECTOR
        p.AddToggle("Diagnostics runtime level Full (events), then reconnect", false, [this](bool v) { fullLevel_ = v; (void)CNA::Diagnostics::SetRuntimeMode(v ? CNA::Diagnostics::Mode::Full : CNA::Diagnostics::Mode::Stats); });
        p.AddButton("Agent::Start (127.0.0.1, port chosen by the OS)", [this] { StartAgent(); });
        p.AddButton("Agent::Stop", [this] { StopAgent(); });
        p.AddButton("Client::Connect with the token", [this] { ConnectClient(true); });
        p.AddButton("Client::Connect with a wrong token", [this] { ConnectClient(false); });
        p.AddButton("Client::Ping", [this] { Ping(); });
        p.AddButton("Client::CaptureSnapshot", [this] { Snapshot(); });
        p.AddButton("Client::ReadEvents", [this] { Events(); });
        p.AddButton("Client::RequestPreview of a resource", [this] { Preview(); });
        p.AddButton("Client::RequestPreview of the room's own texture", [this] { PreviewOwn(); });
        p.AddButton("Client::PollPreview", [this] { PollOwn(); });
        p.AddReadout("Agent", [this] { return agent_ && agent_->IsRunning() ? "running on port " + std::to_string(agent_->GetPort()) : std::string("not started"); });
        p.AddReadout("Client", [this] { return client_.IsConnected() ? std::string("connected") : std::string("not connected"); });
        p.AddReadout("Last request", [this] { return last_; });
#else
        p.AddReadout("Inspector", [] { return std::string("not built into this museum"); });
#endif
        AddInfoSign(b, "Watching a running CNA program",
                    {"The Inspector is a development tool outside the engine proper: an Agent in the program serves CNA's diagnostics counters and frame history over an authenticated socket on the loopback address, and a separate process draws them.",
                     "Nothing starts by itself. This room starts the agent when you press the button, then connects a Client to it from the same program so each message and each number is visible."});
    }

    ~InspectorLab() override
    {
#ifdef MUSEUM_INSPECTOR
        client_.Disconnect();
        if (agent_) agent_->Stop();
        if (fullLevel_) (void)CNA::Diagnostics::SetRuntimeMode(CNA::Diagnostics::Mode::Stats);
#endif
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
#ifdef MUSEUM_INSPECTOR
        if (provider_ == nullptr || previewTexture_ == nullptr) return;
        for (const std::uint64_t ticket : provider_->TakePending()) {
            Insp::PreviewResponse response;
            response.ticket = ticket;
            try {
                System::IO::MemoryStream png;
                previewTexture_->SaveAsPng(&png, kPreviewSize, kPreviewSize);
                const auto bytes = png.ToArray();
                response.encodedImage.assign(bytes.begin(), bytes.end());
                response.status = Insp::PreviewStatus::Ready;
                response.width = response.height = kPreviewSize;
                response.mimeType = "image/png";
            } catch (const std::exception& e) {
                response.status = Insp::PreviewStatus::Failed;
                response.message = e.what();
                ReportError(std::string("encoding the preview: ") + e.what());
            }
            provider_->Complete(std::move(response));
        }
#endif
    }

    void OnLeave() override
    {
#ifdef MUSEUM_INSPECTOR
        if (fullLevel_) { fullLevel_ = false; (void)CNA::Diagnostics::SetRuntimeMode(CNA::Diagnostics::Mode::Stats); }
#endif
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Text(sb, "The Inspector, in this process", 24, 6, 0.8f, Kit::kGold);
#ifndef MUSEUM_INSPECTOR
        Text(sb, "CNA's Inspector module is not part of this build (-DMUSEUM_INSPECTOR=OFF).", 34, 80, 0.6f, Kit::Rgba(255, 190, 150));
#else
        float y = 52.0f;
        const auto line = [&](const std::string& s, Color c = Kit::kText, float scale = 0.52f) { Text(sb, s, 34, y, scale, c); y += 22.0f; };
        if (agent_ && agent_->IsRunning()) {
            line("Agent: listening on " + agent_->GetBindAddress() + ":" + std::to_string(agent_->GetPort()) + ", token " + agent_->GetAuthenticationToken().substr(0, 10) + "... (" + std::to_string(agent_->GetAuthenticationToken().size()) + " characters, made by CNA)", Kit::Rgba(150, 230, 160));
        } else {
            line("Agent: not started. Press Agent::Start.", Kit::kDim);
        }
        if (client_.IsConnected()) {
            const Insp::HelloResponse& h = client_.GetSession();
            line("Session: protocol " + std::to_string(h.selectedMajorVersion) + "." + std::to_string(h.selectedMinorVersion) + ", provider v" + std::to_string(h.providerInterfaceVersion) + ", " + (h.localOnly ? "local only" : "remote allowed"), Kit::Rgba(200, 220, 255));
            line("Application " + h.applicationName + ", CNA " + h.cnaVersion + ", " + h.targetPlatform + " / " + h.platformBackend + " / " + h.graphicsRenderer + " / " + h.buildConfiguration);
            line("Capabilities: " + CapabilityNames(h.capabilities));
            line("Limits: payload " + std::to_string(h.maximumPayloadBytes / 1024) + " KB, " + std::to_string(h.maximumEventsPerResponse) + " events per read, preview " + std::to_string(h.maximumPreviewBytes / 1024) + " KB");
            for (const Insp::MetadataEntry& m : h.metadata) line("  " + m.key + " = " + m.value, Kit::kDim, 0.48f);
        } else {
            line("Client: not connected.", Kit::kDim);
        }
        y += 6.0f;
        if (haveSnapshot_) {
            line("Snapshot: " + std::to_string(snapshot_.metricCount) + " metrics, " + std::to_string(snapshot_.frameCount) + " recent frames, " + std::to_string(snapshot_.resourceCount) + " resources; frame " + std::to_string(snapshot_.snapshot.currentFrameNumber), Kit::kGold);
            const auto& m = snapshot_.snapshot.metrics;
            for (std::size_t i = 0; i < m.size() && i < 7; ++i) line("  " + m[i].name.substr(0, 40) + " = " + std::to_string(m[i].value), Kit::kText, 0.5f);
            if (!snapshot_.snapshot.recentFrames.empty()) {
                const auto& f = snapshot_.snapshot.recentFrames.back();
                char buffer[96];
                std::snprintf(buffer, sizeof buffer, "  last frame #%llu: %.2f ms (%.0f fps)", static_cast<unsigned long long>(f.frameNumber), static_cast<double>(f.durationNs) / 1.0e6, f.framesPerSecond);
                line(buffer, Kit::kText, 0.5f);
            }
        }
        if (haveEvents_) line(events_, Kit::Rgba(210, 190, 255));
        y += 6.0f;
        Text(sb, "Requests so far", 34, y, 0.55f, Kit::kGold);
        y += 24.0f;
        for (const std::string& l : log_) line(l, Kit::kDim, 0.48f);
#endif
        sb.End();
    }

private:
    void Text(SpriteBatch& sb, const std::string& s, float x, float y, float scale, Color c)
    {
        sb.DrawString(*S().font, s.size() > 130 ? s.substr(0, 127) + "..." : s, Vector2(x, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
    }

#ifdef MUSEUM_INSPECTOR
    void Note(const std::string& what, bool ok, const std::string& detail, double ms)
    {
        char buffer[48];
        std::snprintf(buffer, sizeof buffer, " (%.1f ms)", ms);
        last_ = what + (ok ? ": ok" : ": refused");
        log_.push_front(what + (ok ? ": ok" : ": failed") + (detail.empty() ? "" : ", " + detail) + buffer);
        while (log_.size() > 6) log_.pop_back();
        Museum::Log("inspector", "%s", log_.front().c_str());
    }

    template <typename F>
    double Timed(F&& f)
    {
        const auto start = std::chrono::steady_clock::now();
        f();
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }

    void StartAgent()
    {
        if (agent_ && agent_->IsRunning()) { Note("Agent::Start", false, "already running", 0.0); return; }
        Insp::AgentConfiguration config;
        config.applicationName = "CNA 3D Museum";
        config.metadata = {{"Room", "pl-inspector"}, {"Started by", "the visitor's button press"}};
        try {
            // The one resource this agent can preview: a small texture, registered so the snapshot lists it with an id.
            if (previewTexture_ == nullptr) {
                previewTexture_ = std::make_unique<Texture2D>(*S().device, kPreviewSize, kPreviewSize);
                std::vector<Color> pixels(static_cast<std::size_t>(kPreviewSize * kPreviewSize));
                for (int y = 0; y < kPreviewSize; ++y)
                    for (int x = 0; x < kPreviewSize; ++x) pixels[static_cast<std::size_t>(y * kPreviewSize + x)] = ((x / 8 + y / 8) % 2) ? Color(255, 200, 40, 255) : Color(30, 60, 140, 255);
                previewTexture_->SetData(pixels.data(), static_cast<int>(pixels.size()));
                CNA::Diagnostics::ResourceDescriptor descriptor;
                descriptor.kind = CNA::Diagnostics::ResourceKind::Texture2D;
                descriptor.label = "pl-inspector preview texture";
                descriptor.format = "Color";
                descriptor.width = descriptor.height = kPreviewSize;
                descriptor.mipCount = 1;
                descriptor.estimatedBytes = static_cast<std::uint64_t>(kPreviewSize) * kPreviewSize * 4;
                descriptor.byteAccuracy = CNA::Diagnostics::Accuracy::Exact;
                previewHandle_ = CNA::Diagnostics::ResourceHandle(descriptor);
            }
            provider_ = std::make_shared<RoomPreviewProvider>(previewHandle_.GetId());
            config.previewProvider = provider_;
        } catch (const std::exception& e) {
            ReportError(std::string("preparing the preview texture: ") + e.what());
        }
        std::string error;
        const double ms = Timed([&] { agent_ = Insp::Agent::Start(config, error); });
        Note("Agent::Start", agent_ != nullptr, agent_ ? "port " + std::to_string(agent_->GetPort()) : error, ms);
    }

    void StopAgent()
    {
        client_.Disconnect();
        if (agent_) agent_->Stop();
        agent_.reset();
        haveSnapshot_ = haveEvents_ = false;
        Note("Agent::Stop", true, "client disconnected too", 0.0);
    }

    void ConnectClient(bool correctToken)
    {
        if (!agent_ || !agent_->IsRunning()) { Note("Client::Connect", false, "no agent is running", 0.0); return; }
        client_.Disconnect();
        Insp::ClientConfiguration config;
        config.port = agent_->GetPort();
        config.clientName = "museum client";
        config.authenticationToken = correctToken ? agent_->GetAuthenticationToken() : "not-the-token";
        std::string error;
        bool ok = false;
        const double ms = Timed([&] { ok = client_.Connect(config, error); });
        Note(correctToken ? "Client::Connect" : "Client::Connect (wrong token)", ok, ok ? "negotiated" : error, ms);
    }

    bool NeedClient(const char* what)
    {
        if (client_.IsConnected()) return true;
        Note(what, false, "not connected", 0.0);
        return false;
    }

    void Ping()
    {
        if (!NeedClient("Client::Ping")) return;
        std::string error;
        bool ok = false;
        const double ms = Timed([&] { ok = client_.Ping(error); });
        Note("Client::Ping", ok, ok ? "pong" : error, ms);
    }

    void Snapshot()
    {
        if (!NeedClient("Client::CaptureSnapshot")) return;
        Insp::SnapshotRequest request;
        request.parts = static_cast<std::uint32_t>(Insp::SnapshotPart::Metrics) | static_cast<std::uint32_t>(Insp::SnapshotPart::Frames) | static_cast<std::uint32_t>(Insp::SnapshotPart::Resources);
        std::string error;
        bool ok = false;
        const double ms = Timed([&] { ok = client_.CaptureSnapshot(request, snapshot_, error); });
        haveSnapshot_ = ok;
        Note("Client::CaptureSnapshot", ok, ok ? std::to_string(snapshot_.metricCount) + " metrics" : error, ms);
    }

    void Events()
    {
        if (!NeedClient("Client::ReadEvents")) return;
        Insp::EventsRequest request;
        request.maximumEvents = 64;
        Insp::EventsResponse response;
        std::string error;
        bool ok = false;
        const double ms = Timed([&] { ok = client_.ReadEvents(request, response, error); });
        haveEvents_ = true;
        if (!ok) {
            events_ = "Events: refused (" + error.substr(0, 60) + "); the timeline needs the runtime level Full when the client connects: switch it on, then connect again";
        } else {
            events_ = "Events: " + std::to_string(response.events.size()) + " returned (sequences " + std::to_string(response.oldestAvailableSequence) + ".." + std::to_string(response.newestAvailableSequence) + ", " +
                      std::to_string(response.producerEventsDropped) + " dropped by producers)";
        }
        Note("Client::ReadEvents", ok, ok ? std::to_string(response.events.size()) + " events" : error, ms);
    }

    void Preview()
    {
        if (!NeedClient("Client::RequestPreview")) return;
        Insp::PreviewRequest request;
        request.resourceId = haveSnapshot_ && !snapshot_.snapshot.resources.empty() ? snapshot_.snapshot.resources.front().id : 1;
        Insp::PreviewResponse response;
        std::string error;
        bool ok = false;
        const double ms = Timed([&] { ok = client_.RequestPreview(request, response, error); });
        Note("Client::RequestPreview", ok, ok ? std::string(PreviewName(response.status)) + (response.message.empty() ? "" : ": " + response.message) : error, ms);
    }

    void PreviewOwn()
    {
        if (!NeedClient("Client::RequestPreview (own texture)")) return;
        Insp::PreviewRequest request;
        request.resourceId = previewHandle_.GetId();
        Insp::PreviewResponse response;
        std::string error;
        bool ok = false;
        const double ms = Timed([&] { ok = client_.RequestPreview(request, response, error); });
        previewTicket_ = ok ? response.ticket : 0;
        Note("Client::RequestPreview (own texture)", ok, ok ? std::string(PreviewName(response.status)) + ", ticket " + std::to_string(response.ticket) : error, ms);
    }

    void PollOwn()
    {
        if (!NeedClient("Client::PollPreview")) return;
        Insp::PreviewPollRequest request;
        request.ticket = previewTicket_;
        Insp::PreviewResponse response;
        std::string error;
        bool ok = false;
        const double ms = Timed([&] { ok = client_.PollPreview(request, response, error); });
        const auto& png = response.encodedImage;
        const bool signature = png.size() > 8 && png[0] == 0x89 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G';
        Note("Client::PollPreview", ok,
             ok ? std::string(PreviewName(response.status)) + (response.status == Insp::PreviewStatus::Ready ? " " + std::to_string(response.width) + "x" + std::to_string(response.height) + " " + response.mimeType + ", " +
                                                                                   std::to_string(png.size()) + " bytes, PNG signature " + (signature ? "ok" : "missing")
                                                                             : (response.message.empty() ? "" : ": " + response.message))
                : error,
             ms);
    }

    static constexpr int kPreviewSize = 64;
    std::unique_ptr<Texture2D> previewTexture_;
    CNA::Diagnostics::ResourceHandle previewHandle_;
    std::shared_ptr<RoomPreviewProvider> provider_;
    std::uint64_t previewTicket_ = 0;
    std::unique_ptr<Insp::Agent> agent_;
    Insp::Client client_;
    Insp::SnapshotResponse snapshot_;
    bool haveSnapshot_ = false, haveEvents_ = false, fullLevel_ = false;
    std::string events_, last_ = "-";
    std::deque<std::string> log_;
#endif
};

}  // namespace

std::unique_ptr<Exhibit> MakeInspectorLab(const RoomDef& room, Services& services)
{
    return std::make_unique<InspectorLab>(room, services);
}
