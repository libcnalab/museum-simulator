// Diagnostics laboratory: instrumenting the program's own code with CNA::Diagnostics, the layer behind the engine's statistics counters
// and the Inspector. The room registers its own counter, gauge, frame counter, zones, a marker, a custom resource and a source that
// is polled every frame; runs a small workload inside nested zones; and shows the numbers through the same provider the Inspector
// reads. With the runtime level raised to FULL it also records a trace (StartRecording / StopRecording), writes it as Chrome Trace JSON and as
// CNA's binary format, and reads the binary form back. Needs a CNA built with CNA_DIAGNOSTICS=FULL for zones, markers and traces;
// at STATS only the counters and resources exist and the room says so.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <sstream>
#include <optional>
#include <string>
#include <vector>

#include "CNA/Diagnostics/Instrumentation.hpp"
#include "CNA/Diagnostics/Diagnostics.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
namespace Diag = CNA::Diagnostics;

namespace {

const char* KindName(Diag::EventKind k)
{
    switch (k) {
        case Diag::EventKind::Zone: return "zone";
        case Diag::EventKind::Marker: return "marker";
        case Diag::EventKind::Frame: return "frame";
        case Diag::EventKind::ResourceCreated: return "resource+";
        case Diag::EventKind::ResourceDestroyed: return "resource-";
        default: return "malformed";
    }
}

const char* ModeName(Diag::Mode m) { return m == Diag::Mode::Full ? "Full" : m == Diag::Mode::Stats ? "Stats" : "Off"; }

// Polled by the engine once per frame: it sets one gauge and bumps one frame counter, through the sink it is handed.
class RoomSource final : public Diag::IDiagnosticsSource {
public:
    RoomSource() : gauge_("Museum/SourceGauge"), frameCounter_("Museum/SourceFrames") {}
    void Collect(Diag::FrameStatisticsSink& sink) override
    {
        sink.Set(gauge_, value_.load());
        sink.Add(frameCounter_, 1);
        ++collected_;
    }
    std::atomic<std::int64_t> value_{0};
    std::atomic<int> collected_{0};

private:
    Diag::GaugeHandle gauge_;
    Diag::FrameCounterHandle frameCounter_;
};

class DiagnosticsLab final : public ScreenExhibit {
public:
    DiagnosticsLab(const RoomDef& room, Services& services)
        : ScreenExhibit(room, services, 960, 540, 7.6f),
          clicks_("Museum/Clicks"),
          level_("Museum/Level"),
          sparks_("Museum/Sparks"),
          outer_("Museum/Workload"),
          inner_("Museum/Inner"),
          marker_("Museum/Marker"),
          source_(std::make_shared<RoomSource>())
    {
    }

    ~DiagnosticsLab() override { Release(); }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("CNA::Diagnostics", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddSlider("Workload (ms per frame, in two zones)", 0.0f, 8.0f, workMs_, [this](float v) { workMs_ = v; }, "%.1f", 0.5f);
        p.AddSlider("Sparks per frame (FrameCounterHandle)", 0.0f, 50.0f, sparks_n_, [this](float v) { sparksN_ = static_cast<int>(v); }, "%.0f", 1.0f);
        p.AddSlider("Custom resource size (KB)", 1.0f, 4096.0f, resourceKb_, [this](float v) { resourceKb_ = v; UpdateResource(); }, "%.0f", 64.0f);
        p.AddButton("CounterHandle.Add(1)", [this] { clicks_.Add(1); });
        p.AddToggle("SetRuntimeMode(Full): zones, markers, timeline", false, [this](bool v) { SetFull(v); });
        p.AddButton("MarkEvent(a marker)", [this] { Diag::MarkEvent(marker_, Diag::Category::Application, ++markerValue_); });
        p.AddButton("StartRecording(4096 events)", [this] { Start(); });
        p.AddButton("StopRecording: read the Trace", [this] { Stop(); });
        p.AddButton("Trace.WriteBinary then ReadBinary", [this] { RoundTrip(); });
        p.AddButton("Overflow: record 16 events while 200 markers fire", [this] { Overflow(); });
        p.AddButton("Unregister the source and the resource, then register again", [this] { Reregister(); });
        p.AddButton("Instrumentation macros (COUNTER_ADD, GAUGE_SET/ADD, PROFILE_SCOPE, EVENT)", [this] { Macros(); });
        p.AddReadout("Build / runtime level", [] { return std::string(ModeName(Diag::GetBuildMode())) + " / " + ModeName(Diag::GetRuntimeMode()); });
        p.AddReadout("Recording", [this] { return session_.IsActive() ? std::string("active") : std::string("no"); });
        p.AddReadout("Source collected", [this] { return std::to_string(source_->collected_.load()) + " times"; });
        AddInfoSign(b, "Measuring your own code",
                    {"CNA::Diagnostics is the layer under the engine's statistics and the Inspector, and a program can add to it: counters and gauges by name, zones that time a scope, markers, resources with a size, and sources the engine polls once per frame.",
                     "At the counters level only counters and resources are kept; at FULL the zones and markers become a timeline that StartRecording can capture and write as Chrome Trace JSON for chrome://tracing or Perfetto."});
    }

    void Load(InteractionSystem& interaction, MuseumWorld& world) override
    {
        ScreenExhibit::Load(interaction, world);
        registration_ = Diag::RegisterSource(source_);
        UpdateResource();
    }

    void Update(const UpdateContext& ctx) override
    {
        ScreenExhibit::Update(ctx);
        if (!ctx.nearby) return;
        {
            // The workload: two nested zones burn a little CPU so the timeline has something to show.
            Diag::ZoneScope outer(outer_);
            Burn(workMs_ * 0.5f);
            {
                Diag::ZoneScope inner(inner_);
                Burn(workMs_ * 0.5f);
            }
        }
        level_.Set(static_cast<std::int64_t>(workMs_ * 100.0f));
        sparks_.Add(sparksN_);
        source_->value_ = static_cast<std::int64_t>(workMs_ * 10.0f);
        if (++tick_ % 8 == 0) Refresh();
    }

    void OnLeave() override { Release(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Text(sb, "The program's own instruments", 24, 6, 0.8f, Kit::kGold);
        float y = 44.0f;
        const auto line = [&](const std::string& s, Color c = Kit::kText, float scale = 0.5f) { Text(sb, s, 34, y, scale, c); y += 19.0f; };
        line(std::string("build level ") + ModeName(Diag::GetBuildMode()) + ", runtime level " + ModeName(Diag::GetRuntimeMode()) + ", frame " + std::to_string(snapshot_.currentFrameNumber), Kit::kDim);
        line("Metrics registered by this room (the provider's snapshot)", Kit::kGold, 0.55f);
        int shown = 0;
        for (const Diag::MetricSample& m : snapshot_.metrics) {
            if (m.name.rfind("Museum/", 0) != 0) continue;
            const char* kind = m.kind == Diag::MetricKind::Counter ? "counter" : m.kind == Diag::MetricKind::Gauge ? "gauge" : "per frame";
            line("  " + m.name + " = " + std::to_string(m.value) + "  (" + kind + ")");
            ++shown;
        }
        if (shown == 0) line("  (none yet)", Kit::kDim);
        for (const Diag::ResourceRecord& r : snapshot_.resources) {
            if (r.label != "Museum custom buffer") continue;
            line("Resource " + r.label + ": " + std::to_string(r.estimatedBytes / 1024) + " KB (custom), among " + std::to_string(snapshot_.resources.size()) + " live resources");
        }
        if (!snapshot_.recentFrames.empty()) {
            const Diag::FrameSample& f = snapshot_.recentFrames.back();
            char buffer[96];
            std::snprintf(buffer, sizeof buffer, "Last frame #%llu: %.2f ms, %.0f fps", static_cast<unsigned long long>(f.frameNumber), static_cast<double>(f.durationNs) / 1.0e6, f.framesPerSecond);
            line(buffer);
        }
        y += 6.0f;
        line("Timeline (ReadEvents, newest last)", Kit::kGold, 0.55f);
        if (Diag::GetBuildMode() != Diag::Mode::Full) line("  this CNA was built at " + std::string(ModeName(Diag::GetBuildMode())) + ": zones and markers are compiled out", Kit::Rgba(255, 190, 150));
        else if (Diag::GetRuntimeMode() != Diag::Mode::Full) line("  runtime level is " + std::string(ModeName(Diag::GetRuntimeMode())) + ": switch on SetRuntimeMode(Full)", Kit::kDim);
        else if (events_.empty()) line("  (no events yet)", Kit::kDim);
        for (const std::string& e : events_) line("  " + e, Kit::Rgba(210, 190, 255), 0.46f);
        y += 6.0f;
        line("Recorded trace", Kit::kGold, 0.55f);
        for (const std::string& t : traceLines_) line("  " + t, Kit::Rgba(170, 220, 255), 0.48f);
        if (traceLines_.empty()) line("  (press StartRecording, wait, then StopRecording)", Kit::kDim);
        sb.End();
    }

private:
    void Text(SpriteBatch& sb, const std::string& s, float x, float y, float scale, Color c)
    {
        sb.DrawString(*S().font, s.size() > 120 ? s.substr(0, 117) + "..." : s, Vector2(x, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
    }

    static void Burn(float ms)
    {
        if (ms <= 0.0f) return;
        const auto end = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<float, std::milli>(ms));
        volatile double sink = 0.0;
        while (std::chrono::steady_clock::now() < end) sink = sink + 1.0;
    }

    void SetFull(bool on)
    {
        if (!Diag::SetRuntimeMode(on ? Diag::Mode::Full : Diag::Mode::Stats)) traceLines_ = {"SetRuntimeMode(Full) was refused: the build level is " + std::string(ModeName(Diag::GetBuildMode()))};
    }

    void UpdateResource()
    {
        Diag::ResourceDescriptor d;
        d.kind = Diag::ResourceKind::Custom;
        d.label = "Museum custom buffer";
        d.format = "bytes";
        d.estimatedBytes = static_cast<std::uint64_t>(resourceKb_) * 1024ULL;
        d.byteAccuracy = Diag::Accuracy::Estimated;
        if (resource_.GetId() == 0) resource_ = Diag::ResourceHandle(d);
        else resource_.Update(d);
    }

    void Refresh()
    {
        snapshot_ = Diag::GetProvider().CaptureSnapshot();
        events_.clear();
        if (Diag::GetRuntimeMode() != Diag::Mode::Full) return;
        const std::uint64_t after = lastSequence_ > 6 ? lastSequence_ - 6 : 0;
        const Diag::EventBatch batch = Diag::GetProvider().ReadEvents(after, 6);
        if (!batch.events.empty()) lastSequence_ = batch.newestAvailableSequence;
        for (const Diag::EventRecord& e : batch.events) {
            char buffer[128];
            std::snprintf(buffer, sizeof buffer, "#%llu %-9s %-28s %7.1f us", static_cast<unsigned long long>(e.sequence), KindName(e.kind), Diag::GetProvider().ResolveName(e.name).substr(0, 28).c_str(), static_cast<double>(e.durationNs) / 1000.0);
            events_.push_back(buffer);
        }
    }

    void Start()
    {
        if (session_.IsActive()) { traceLines_ = {"already recording"}; return; }
        if (Diag::GetRuntimeMode() != Diag::Mode::Full) { traceLines_ = {"recording needs runtime level Full: switch on SetRuntimeMode(Full) first"}; return; }
        session_ = Diag::StartRecording(4096);
        traceLines_ = {session_.IsActive() ? "recording up to 4096 events..." : "StartRecording refused"};
    }

    void Stop()
    {
        if (!session_.IsActive()) { traceLines_ = {"not recording"}; return; }
        trace_ = std::make_unique<Diag::Trace>(Diag::StopRecording(session_));
        Describe();
    }

    void Describe()
    {
        traceLines_.clear();
        if (!trace_) return;
        std::size_t zones = 0, markers = 0, frames = 0;
        double longest = 0.0;
        std::string longestName;
        for (const Diag::EventRecord& e : trace_->GetEvents()) {
            if (e.kind == Diag::EventKind::Zone) ++zones;
            else if (e.kind == Diag::EventKind::Marker) ++markers;
            else if (e.kind == Diag::EventKind::Frame) ++frames;
            if (e.kind == Diag::EventKind::Zone && static_cast<double>(e.durationNs) > longest) { longest = static_cast<double>(e.durationNs); longestName = trace_->ResolveName(e.name); }
        }
        char buffer[160];
        std::snprintf(buffer, sizeof buffer, "format v%u: %zu events (%zu zones, %zu markers, %zu frames), %llu dropped", static_cast<unsigned>(trace_->GetVersion()), trace_->GetEvents().size(), zones, markers, frames, static_cast<unsigned long long>(trace_->GetDroppedEventCount()));
        traceLines_.push_back(buffer);
        std::snprintf(buffer, sizeof buffer, "longest zone: %s, %.2f ms", longestName.c_str(), longest / 1.0e6);
        traceLines_.push_back(buffer);
        std::ostringstream chrome;
        const bool ok = trace_->WriteChromeTrace(chrome);
        traceLines_.push_back(std::string("Trace.WriteChromeTrace: ") + (ok ? std::to_string(chrome.str().size() / 1024) + " KB of JSON, starts " : "failed") + (ok ? chrome.str().substr(0, 40) : std::string()));
    }

    // A recording smaller than what happens: the trace must say how many events it could not keep.
    void Overflow()
    {
        if (session_.IsActive()) { traceLines_ = {"already recording"}; return; }
        if (Diag::GetRuntimeMode() != Diag::Mode::Full) {
            traceLines_ = {"recording needs runtime level Full: switch on SetRuntimeMode(Full) first"};
            Museum::Log("diagnostics", "overflow: not attempted, runtime level %s", ModeName(Diag::GetRuntimeMode()));
            return;
        }
        session_ = Diag::StartRecording(16);
        for (int i = 0; i < 200; ++i) Diag::MarkEvent(marker_, Diag::Category::Application, ++markerValue_);
        trace_ = std::make_unique<Diag::Trace>(Diag::StopRecording(session_));
        Describe();
        Museum::Log("diagnostics", "overflow: StartRecording(16) kept %zu events and reports %llu dropped", trace_->GetEvents().size(), static_cast<unsigned long long>(trace_->GetDroppedEventCount()));
    }

    // The one-line instrumentation macros of CNA/Diagnostics/Instrumentation.hpp. With CNA_DIAGNOSTICS_LEVEL >= 1 (STATS,
    // FULL) they register and update metrics; at OFF they compile to an unevaluated check and leave nothing behind. The
    // profile scope and event only reach a trace at runtime level Full, so they are recorded in a short session.
    void Macros()
    {
        const auto check = [this](const std::string& name, const std::string& expected, const std::string& got) {
            Museum::Log("check", "%s %s: %s (expected %s, got %s)", R().key.c_str(), name.c_str(), expected == got ? "PASS" : "FAIL", expected.c_str(), got.c_str());
        };
        const auto metric = [](const Diag::Snapshot& snap, const std::string& name) -> std::optional<std::int64_t> {
            for (const Diag::MetricSample& m : snap.metrics)
                if (m.name == name) return m.value;
            return std::nullopt;
        };
        const auto shown = [](const std::optional<std::int64_t>& v) { return v ? std::to_string(*v) : std::string("absent"); };
        const std::optional<std::int64_t> before = metric(Diag::GetProvider().CaptureSnapshot(), "Museum/MacroCounter");
        CNA_DIAGNOSTICS_COUNTER_ADD("Museum/MacroCounter", 3);
        CNA_DIAGNOSTICS_GAUGE_SET("Museum/MacroGauge", 40);
        CNA_DIAGNOSTICS_GAUGE_ADD("Museum/MacroGauge", 2);
        CNA_DIAGNOSTICS_FRAME_COUNTER_ADD("Museum/MacroFrames", 5);
        const Diag::Snapshot after = Diag::GetProvider().CaptureSnapshot();
#if CNA_DIAGNOSTICS_LEVEL >= 1
        const bool compiledIn = true;
#else
        const bool compiledIn = false;
#endif
        const std::string level = std::to_string(CNA_DIAGNOSTICS_LEVEL);
        check("CNA_DIAGNOSTICS_COUNTER_ADD(\"Museum/MacroCounter\", 3) at level " + level, compiledIn ? "the counter grew by 3" : "no counter (compiled out)",
              compiledIn ? (metric(after, "Museum/MacroCounter").value_or(-1) - before.value_or(0) == 3 ? std::string("the counter grew by 3") : "from " + shown(before) + " to " + shown(metric(after, "Museum/MacroCounter")))
                         : (metric(after, "Museum/MacroCounter") ? std::string("a counter ") + shown(metric(after, "Museum/MacroCounter")) : std::string("no counter (compiled out)")));
        check("CNA_DIAGNOSTICS_GAUGE_SET 40 then GAUGE_ADD 2 at level " + level, compiledIn ? "42" : "absent", shown(metric(after, "Museum/MacroGauge")));
        check("CNA_DIAGNOSTICS_FRAME_COUNTER_ADD registers a per-frame counter at level " + level, compiledIn ? "registered" : "absent",
              metric(after, "Museum/MacroFrames") ? std::string("registered") : std::string("absent"));
        if (Diag::GetRuntimeMode() != Diag::Mode::Full || session_.IsActive()) {
            Museum::Log("check", "%s CNA_PROFILE_SCOPE and CNA_DIAGNOSTICS_EVENT: SKIP (they reach a trace only at runtime level Full with no other recording)", R().key.c_str());
            return;
        }
        Diag::RecordingSession macroSession = Diag::StartRecording(256);
        {
            CNA_PROFILE_SCOPE("Museum/MacroScope");
            CNA_DIAGNOSTICS_EVENT("Museum/MacroEvent");
        }
        const Diag::Trace trace = Diag::StopRecording(macroSession);
        std::string zone = "absent", marker = "absent";
        for (const Diag::EventRecord& e : trace.GetEvents()) {
            const std::string name = trace.ResolveName(e.name);
            if (name == "Museum/MacroScope" && e.kind == Diag::EventKind::Zone) zone = "a zone";
            if (name == "Museum/MacroEvent" && e.kind == Diag::EventKind::Marker) marker = "a marker";
        }
        check("CNA_PROFILE_SCOPE(\"Museum/MacroScope\") in the trace at level " + level, compiledIn ? "a zone" : "absent", zone);
        check("CNA_DIAGNOSTICS_EVENT(\"Museum/MacroEvent\") in the trace at level " + level, compiledIn ? "a marker" : "absent", marker);
    }

    // A source and a resource leave the provider's view when their registrations end, and come back when renewed.
    void Reregister()
    {
        if (Diag::GetBuildMode() == Diag::Mode::Off) {
            // CNA_DIAGNOSTICS=OFF compiles the provider out: no source or resource is tracked, so there is nothing to follow.
            Museum::Log("diagnostics", "re-registration: diagnostics are compiled out (CNA_DIAGNOSTICS=OFF), nothing is tracked");
            return;
        }
        const auto listed = [this] {
            const Diag::Snapshot snap = Diag::GetProvider().CaptureSnapshot();
            bool resource = false;
            for (const Diag::ResourceRecord& r : snap.resources) resource = resource || r.label == "Museum custom buffer";
            return resource;
        };
        registration_.Reset();
        resource_.Reset();
        const bool resourceAfterReset = listed();
        registration_ = Diag::RegisterSource(source_);   // the engine polls the renewed source from the next frame on
        UpdateResource();
        const bool resourceBack = listed();
        Museum::Log("diagnostics", "re-registration: after Reset the resource is %s; registered again it is %s", resourceAfterReset ? "still listed" : "gone", resourceBack ? "listed" : "missing");
        if (resourceAfterReset || !resourceBack) ReportError("a diagnostics resource did not follow its ResourceHandle");
    }

    void RoundTrip()
    {
        if (!trace_) { traceLines_ = {"no trace yet: StartRecording, then StopRecording"}; return; }
        std::stringstream binary;
        const bool written = trace_->WriteBinary(binary);
        const std::size_t bytes = binary.str().size();
        try {
            const Diag::Trace again = Diag::Trace::ReadBinary(binary);
            Describe();
            traceLines_.push_back("WriteBinary " + std::string(written ? "ok" : "failed") + ", " + std::to_string(bytes / 1024) + " KB; ReadBinary gave " + std::to_string(again.GetEvents().size()) + " events, " + (again.GetEvents().size() == trace_->GetEvents().size() ? "the same count" : "a DIFFERENT count"));
        } catch (const std::exception& e) {
            traceLines_.push_back(std::string("ReadBinary threw: ") + e.what());
            ReportError(std::string("Trace ReadBinary of a trace just written: ") + e.what());   // its own output must read back
        }
    }

    void Release()
    {
        if (session_.IsActive()) { (void)Diag::StopRecording(session_); }
        if (Diag::GetRuntimeMode() == Diag::Mode::Full) (void)Diag::SetRuntimeMode(Diag::Mode::Stats);
        registration_.Reset();
        resource_.Reset();
    }

    Diag::CounterHandle clicks_;
    Diag::GaugeHandle level_;
    Diag::FrameCounterHandle sparks_;
    Diag::NameHandle outer_, inner_, marker_;
    Diag::ResourceHandle resource_;
    std::shared_ptr<RoomSource> source_;
    Diag::SourceRegistration registration_;
    Diag::RecordingSession session_;
    std::unique_ptr<Diag::Trace> trace_;
    Diag::Snapshot snapshot_;
    std::vector<std::string> events_, traceLines_;
    std::uint64_t lastSequence_ = 0;
    std::int64_t markerValue_ = 0;
    float workMs_ = 2.0f, resourceKb_ = 256.0f, sparks_n_ = 5.0f;
    int sparksN_ = 5, tick_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeDiagnosticsLab(const RoomDef& room, Services& services)
{
    return std::make_unique<DiagnosticsLab>(room, services);
}
