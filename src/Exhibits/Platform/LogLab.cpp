// Log laboratory: CNA::Logger, the engine's own log. Every CNA subsystem writes through it (renderers under RENDER, the audio
// engine under AUDIO, ...), a program writes its own lines under APPLICATION, and SetSink hands every line to the program. This room
// installs a sink that keeps the last lines and still prints each one to the console in CNA's own format, lets the visitor
// raise or lower the minimum level, send a line of any level and category (conditional forms included), and make CNA write one
// itself by compiling a shader that does not compile.
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "CNA/LogCategory.hpp"
#include "CNA/LogLevel.hpp"
#include "CNA/Logger.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const char* const kLevelNames[] = {"FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE", "EXPERIMENT"};
constexpr CNA::LogLevel kLevels[] = {CNA::LogLevel::FATAL, CNA::LogLevel::ERROR, CNA::LogLevel::WARN, CNA::LogLevel::INFO,
                                     CNA::LogLevel::DEBUG, CNA::LogLevel::TRACE, CNA::LogLevel::EXPERIMENT};
const char* const kCategoryNames[] = {"APPLICATION", "ERROR", "SYSTEM", "AUDIO", "VIDEO", "RENDER", "INPUT", "TEST", "GPU"};
constexpr CNA::LogCategory kCategories[] = {CNA::LogCategory::APPLICATION, CNA::LogCategory::ERROR, CNA::LogCategory::SYSTEM, CNA::LogCategory::AUDIO,
                                            CNA::LogCategory::VIDEO, CNA::LogCategory::RENDER, CNA::LogCategory::INPUT, CNA::LogCategory::TEST, CNA::LogCategory::GPU};

int LevelIndex(CNA::LogLevel l)
{
    for (int i = 0; i < 7; ++i)
        if (kLevels[i] == l) return i;
    return 3;
}
int CategoryIndex(CNA::LogCategory c)
{
    for (int i = 0; i < 9; ++i)
        if (kCategories[i] == c) return i;
    return 0;
}

Color LevelColour(int level)
{
    switch (level) {
        case 0: return Kit::Rgba(255, 110, 110);
        case 1: return Kit::Rgba(255, 150, 120);
        case 2: return Kit::Rgba(255, 215, 120);
        case 3: return Kit::Rgba(200, 230, 255);
        case 4: return Kit::Rgba(170, 200, 170);
        case 5: return Kit::Rgba(150, 160, 175);
        default: return Kit::Rgba(210, 170, 255);
    }
}

struct Line {
    int level;
    int category;
    std::string text;
};

class LogLab final : public ScreenExhibit {
public:
    LogLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    ~LogLab() override
    {
        if (installed_) {
            CNA::Logger::ResetSink();
            CNA::Logger::SetMinimumLevel(startLevel_);
        }
    }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("CNA::Logger", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddChoice("Minimum level (SetMinimumLevel)", {kLevelNames[0], kLevelNames[1], kLevelNames[2], kLevelNames[3], kLevelNames[4], kLevelNames[5]}, 3, [this](int v) { CNA::Logger::SetMinimumLevel(kLevels[v]); });
        p.AddChoice("Level of my line", {kLevelNames[0], kLevelNames[1], kLevelNames[2], kLevelNames[3], kLevelNames[4], kLevelNames[5], kLevelNames[6]}, 3, [this](int v) { level_ = v; });
        p.AddChoice("Category of my line", {kCategoryNames[0], kCategoryNames[1], kCategoryNames[2], kCategoryNames[3], kCategoryNames[4], kCategoryNames[5], kCategoryNames[6], kCategoryNames[7], kCategoryNames[8]}, 0, [this](int v) { category_ = v; });
        p.AddButton("Logger.Log(level, message, category)", [this] { Send(); });
        p.AddToggle("Condition for the *If forms", false, [this](bool v) { condition_ = v; });
        p.AddButton("Logger.InfoIf / WarnIf(message, condition)", [this] { SendConditional(); });
        p.AddButton("Make CNA write a line (a ShaderEffect that will not compile)", [this] { ProvokeEngine(); });
        p.AddButton("Clear the list", [this] { Clear(); });
        p.AddButton("Contracts: level filter, *If forms, sink replacement and reset", [this] { Contracts(); });
        p.AddReadout("GetMinimumLevel()", [] { return std::string(kLevelNames[LevelIndex(CNA::Logger::GetMinimumLevel())]); });
        p.AddReadout("Lines received by the sink", [this] { return std::to_string(received_.load()); });
        p.AddReadout("Last action", [this] { return note_; });
        AddInfoSign(b, "The engine's log",
                    {"CNA::Logger is how every CNA subsystem reports: seven levels (FATAL to TRACE, plus EXPERIMENT) and nine categories (RENDER, AUDIO, GPU ...). Lines below the minimum level are dropped before anything sees them.",
                     "Logger::SetSink gives the program every line that gets through: this room keeps the last fourteen and still prints each to the console; the text arrives already formatted as [LEVEL][CATEGORY] message."});
    }

    void OnScreenLoaded() override
    {
        startLevel_ = CNA::Logger::GetMinimumLevel();
        InstallSink();
        CNA::Logger::Info("the museum's log room installed a sink", CNA::LogCategory::APPLICATION);
    }

    void InstallSink()
    {
        CNA::Logger::SetSink([this](CNA::LogLevel level, CNA::LogCategory category, std::string_view text) {
            // The sink replaces the default one, so the console line CNA would have written is written here. The text arrives already
            // formatted: "[LEVEL][CATEGORY] message".
            std::fprintf(stderr, "%.*s\n", static_cast<int>(text.size()), text.data());
            std::lock_guard<std::mutex> lock(mutex_);
            lines_.push_back({LevelIndex(level), CategoryIndex(category), std::string(text)});
            while (lines_.size() > 14) lines_.pop_front();
            ++received_;
        });
        installed_ = true;
    }

    // What a program can rely on: a line below the minimum level reaches no sink, the *If forms log only
    // when their condition holds, a sink sees the line formatted as [LEVEL][CATEGORY] message, a replaced sink is never called
    // again, and after ResetSink no program sink is. The room's own sink and level are put back afterwards.
    void Contracts()
    {
        const auto check = [this](const std::string& name, const std::string& expected, const std::string& got) {
            Museum::Log("check", "%s %s: %s (expected %s, got %s)", R().key.c_str(), name.c_str(), expected == got ? "PASS" : "FAIL", expected.c_str(), got.c_str());
        };
        const CNA::LogLevel level = CNA::Logger::GetMinimumLevel();
        std::vector<std::string> first, second;
        CNA::Logger::SetSink([&first](CNA::LogLevel, CNA::LogCategory, std::string_view text) { first.emplace_back(text); });
        CNA::Logger::SetMinimumLevel(CNA::LogLevel::WARN);
        CNA::Logger::Info("contracts: below the level");
        CNA::Logger::Warn("contracts: at the level", CNA::LogCategory::AUDIO);
        check("SetMinimumLevel(WARN): Info dropped, Warn delivered", "1 line", std::to_string(first.size()) + (first.size() == 1 ? " line" : " lines"));
        check("the sink's text", "[WARN][AUDIO] contracts: at the level",
              first.empty() ? std::string("nothing") : first.front());
        first.clear();
        CNA::Logger::WarnIf("contracts: condition false", false);
        CNA::Logger::ErrorIf("contracts: condition true", true);
        CNA::Logger::InfoIf("contracts: below the level, condition true", true);
        check("WarnIf(false), ErrorIf(true), InfoIf(true) below the level", "only the ErrorIf line",
              first.size() == 1 && first.front().find("condition true") != std::string::npos ? std::string("only the ErrorIf line") : std::to_string(first.size()) + " line(s)");
        first.clear();
        CNA::Logger::SetSink([&second](CNA::LogLevel, CNA::LogCategory, std::string_view text) { second.emplace_back(text); });
        CNA::Logger::Warn("contracts: after SetSink again");
        check("SetSink replaces the sink: the old one is not called", "old 0, new 1", "old " + std::to_string(first.size()) + ", new " + std::to_string(second.size()));
        CNA::Logger::ResetSink();
        CNA::Logger::Warn("contracts: after ResetSink (the default console sink)");
        check("ResetSink: no program sink is called", "old 0, new 1", "old " + std::to_string(first.size()) + ", new " + std::to_string(second.size()));
        CNA::Logger::SetMinimumLevel(level);
        InstallSink();
        note_ = "contracts checked (see the log)";
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Text(sb, "What the sink received", 24, 8, 0.85f, Kit::kGold);
        Text(sb, std::string("minimum level ") + kLevelNames[LevelIndex(CNA::Logger::GetMinimumLevel())] + ": a line below it never reaches the list", 24, 50, 0.55f, Kit::kDim);
        std::vector<Line> copy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            copy.assign(lines_.begin(), lines_.end());
        }
        float y = 82.0f;
        if (copy.empty()) Text(sb, "(nothing yet: send a line with the panel)", 34, y, 0.6f, Kit::kDim);
        for (const Line& l : copy) {
            std::string text = l.text;
            if (text.size() > 96) text = text.substr(0, 93) + "...";
            Text(sb, text, 34, y, 0.56f, LevelColour(l.level));
            y += 30.0f;
        }
        sb.End();
    }

private:
    void Text(SpriteBatch& sb, const std::string& s, float x, float y, float scale, Color c)
    {
        sb.DrawString(*S().font, s, Vector2(x, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
    }

    void Send()
    {
        ++sent_;
        const std::string message = "visitor line " + std::to_string(sent_);
        const int before = received_.load();
        CNA::Logger::Log(kLevels[level_], message, kCategories[category_]);
        note_ = std::string(kLevelNames[level_]) + (received_.load() > before ? " reached the sink" : " was dropped before the sink");
    }

    void SendConditional()
    {
        ++sent_;
        const int before = received_.load();
        CNA::Logger::InfoIf("InfoIf line " + std::to_string(sent_) + " (condition true)", condition_);
        const int info = received_.load() - before;
        CNA::Logger::WarnIf("WarnIf line " + std::to_string(sent_) + " (condition false)", !condition_);
        const int warn = received_.load() - before - info;
        note_ = std::string("InfoIf: ") + (info > 0 ? "logged" : "silent") + ", WarnIf: " + (warn > 0 ? "logged" : "silent");
    }

    void ProvokeEngine()
    {
        const int before = received_.load();
        try {
            // Not valid GLSL or WGSL on purpose; the renderer reports the compile error through the log.
            ShaderEffect broken(*S().device, "this is not a vertex shader", "this is not a fragment shader");
            note_ = broken.IsEffectValid() ? "this renderer accepts the source without compiling it" : "ShaderEffect.IsEffectValid() is false";
        } catch (const std::exception& e) {
            note_ = std::string("ShaderEffect threw: ") + std::string(e.what()).substr(0, 40);
        }
        if (received_.load() == before) note_ += "; the renderer wrote nothing";
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lines_.clear();
        note_ = "list cleared";
    }

    std::mutex mutex_;
    std::deque<Line> lines_;
    std::atomic<int> received_{0};
    int sent_ = 0, level_ = 3, category_ = 0;
    bool condition_ = false, installed_ = false;
    CNA::LogLevel startLevel_ = CNA::LogLevel::INFO;
    std::string note_ = "-";
};

}  // namespace

std::unique_ptr<Exhibit> MakeLogLab(const RoomDef& room, Services& services)
{
    return std::make_unique<LogLab>(room, services);
}
