// A short history of XNA and CNA. The XNA timeline is history (it will not change); everything about CNA is read
// from the running build: its git revision at configure time, the options it was compiled with, the renderers it
// contains and their maturity, and which of the museum's rooms demonstrate extensions. Nothing about CNA is typed in.
#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "CNA/GraphicsRendererMaturity.hpp"
#include "CNA/GraphicsRendererSelection.hpp"
#include "CNA/GraphicsRendererType.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "World/Catalogue.hpp"

#ifndef MUSEUM_CNA_REVISION
#define MUSEUM_CNA_REVISION "unknown"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Xna, Cna, Compare };

class HistoryLab final : public ScreenExhibit {
public:
    HistoryLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f)
    {
        const std::string m = room.params.Get("mode");
        mode_ = m == "cna" ? Mode::Cna : (m == "compare" ? Mode::Compare : Mode::Xna);
    }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        if (mode_ == Mode::Xna) p.AddSlider("Scroll the timeline", 0.0f, 1.0f, scroll_, [this](float v) { scroll_ = v; });
        if (mode_ == Mode::Cna) {
            p.AddReadout("CNA revision (at configure)", [] { return std::string(MUSEUM_CNA_REVISION); });
            p.AddReadout("Renderers in this build", [this] { return std::to_string(CNA::GraphicsRendererSelection::GetAvailable().size()); });
        }
        if (mode_ == Mode::Compare) p.AddToggle("Show the CNA extension names", true, [this](bool v) { names_ = v; });
        AddInfoSign(b, Title(), Explanation());
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        Texture2D& white = *S().whiteTexture;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(Title(), 24, 10, Kit::kGold, 1.0f);
        if (mode_ == Mode::Xna) DrawXna(rect, text);
        else if (mode_ == Mode::Cna) DrawCna(rect, text);
        else DrawCompare(rect, text);
        sb.End();
    }

private:
    struct Event {
        const char* when;
        const char* title;
        const char* detail;
    };

    [[nodiscard]] std::string Title() const
    {
        return mode_ == Mode::Xna ? "XNA, 2004 to today" : (mode_ == Mode::Cna ? "This build of CNA" : "XNA 4.0 and what CNA adds");
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        if (mode_ == Mode::Xna) return {"XNA Game Studio was Microsoft's managed game framework for Windows and Xbox 360. Its last release, 4.0, is the API CNA", "reproduces. The dates are history, not measurements."};
        if (mode_ == Mode::Cna) return {"Everything on this screen is read from the running build of CNA: nothing is typed in, so it cannot go stale.", "The CNA revision is recorded by CMake when the museum is configured."};
        return {"Rooms are labelled XNA 4.0 (the feature exists in XNA 4.0) or CNA extension (it does not). These numbers and names are", "generated from the museum's catalogue as you read them."};
    }

    template <typename Rect, typename Text>
    void DrawXna(Rect& rect, Text& text)
    {
        static const Event events[] = {
            {"2004", "XNA announced", "Microsoft shows a managed successor to the C++ DirectX SDK at the Game Developers Conference."},
            {"Dec 2006", "XNA Game Studio Express 1.0", "Windows only, Direct3D 9, C# Express; Game, GameTime, SpriteBatch and the content pipeline arrive together."},
            {"Dec 2007", "XNA Game Studio 2.0", "Works with every Visual Studio edition; networking (Xbox LIVE and System Link sessions) joins the framework."},
            {"Oct 2008", "XNA Game Studio 3.0", "Zune support and the Community Games programme; the Xbox 360 gets a path for independent games."},
            {"Jun 2009", "XNA Game Studio 3.1", "Audio and video additions and the last release before the large API clean-up."},
            {"Sep 2010", "XNA Game Studio 4.0", "Windows Phone 7; the Reach and HiDef profiles; stock effects replace fixed-function state; RenderTarget2D becomes a texture."},
            {"2013", "Development stops", "Microsoft stops developing XNA. MonoGame, FNA and others carry the API forward; CNA is one of them."},
        };
        const int count = static_cast<int>(sizeof(events) / sizeof(events[0]));
        const float offset = scroll_ * 90.0f;
        rect(150, 70, 4, 440, Kit::Rgba(110, 120, 150));
        for (int i = 0; i < count; ++i) {
            const float y = 80.0f + static_cast<float>(i) * 66.0f - offset;
            if (y < 54.0f || y > 480.0f) continue;
            rect(144, y + 8, 16, 16, Kit::Rgba(255, 210, 120));
            text(events[i].when, 24, y + 4, Kit::Rgba(255, 210, 120), 0.7f);
            text(events[i].title, 176, y - 2, Kit::kText, 0.75f);
            text(events[i].detail, 176, y + 26, Kit::kDim, 0.5f);
        }
    }

    template <typename Rect, typename Text>
    void DrawCna(Rect& rect, Text& text)
    {
        (void)rect;
        std::vector<std::string> lines;
        lines.push_back(std::string("CNA revision at configure time: ") + MUSEUM_CNA_REVISION);
        lines.push_back("Active renderer: " + std::string(S().device->GetGraphicsRendererName()));
        std::string compiled;
        for (const CNA::GraphicsRendererType t : CNA::GraphicsRendererSelection::GetAvailable())
            compiled += std::string(CNA::getGraphicsRendererName(t)) + " (" +
                        std::string(CNA::toStringView(CNA::getGraphicsRendererMaturity(t))) + ")   ";
        lines.push_back("Renderers compiled into this build: " + compiled);
        {
            using CNA::GraphicsRendererSelection;
            std::string sel = std::string("Requested ") + std::string(CNA::getGraphicsRendererName(GraphicsRendererSelection::GetSelected())) +
                              ", active " + std::string(CNA::getGraphicsRendererName(GraphicsRendererSelection::GetActive())) + ", fallback " +
                              (GraphicsRendererSelection::IsFallbackEnabled() ? "on" : "off (a renderer that fails is an error, never a silent substitute)");
            const auto history = GraphicsRendererSelection::GetFallbackHistory();
            if (!history.empty()) {
                sel += "; passed over:";
                for (const auto& r : history) sel += " " + std::string(CNA::getGraphicsRendererName(r.type)) + " (" + std::string(CNA::getGraphicsRendererFallbackReasonName(r.reason)) + ")";
            }
            lines.push_back(sel);
        }
        lines.push_back(std::string("Built with: CNA_CNAEXT ") +
#ifdef CNA_CNAEXT
                        "on"
#else
                        "off"
#endif
                        + ",  CNA_DEVICES " +
#ifdef CNA_DEVICES
                        "on"
#else
                        "off"
#endif
                        + ",  CNA_DIAGNOSTICS_LEVEL " + std::to_string(
#ifdef CNA_DIAGNOSTICS_LEVEL
                                                         CNA_DIAGNOSTICS_LEVEL
#else
                                                         0
#endif
                                                         ));
        lines.push_back(std::string("Museum compiled as C++ ") + std::to_string(__cplusplus / 100 % 100) + " with " + __VERSION__ +
#ifdef NDEBUG
                        ", optimised"
#else
                        ", debug"
#endif
        );
        float y = 70.0f;
        for (const std::string& l : lines) {
            text(l, 30, y, Kit::kText, 0.62f);
            y += 40.0f;
        }
        text("Renderers CNA has retired are not listed: they are gone from its public list.", 30, y + 10, Kit::kDim, 0.55f);
    }

    template <typename Rect, typename Text>
    void DrawCompare(Rect& rect, Text& text)
    {
        const MuseumPlan& m = TheMuseum();
        int xna = 0, ext = 0, app = 0;
        std::set<std::string> extNames;
        for (const RoomDef& r : m.rooms) {
            if (r.kind != CellKind::Room) continue;
            if (r.origin == ApiOrigin::Xna) ++xna;
            else if (r.origin == ApiOrigin::CnaExtension) {
                ++ext;
                for (const std::string& a : r.apis) extNames.insert(a);
            } else ++app;
        }
        const float total = static_cast<float>(std::max(1, xna + ext + app));
        rect(30, 70, 900.0f * static_cast<float>(xna) / total, 40, Kit::Rgba(110, 170, 255));
        const float extStart = 30 + 900.0f * static_cast<float>(xna) / total;
        const float appStart = extStart + 900.0f * static_cast<float>(ext) / total;
        rect(extStart, 70, 900.0f * static_cast<float>(ext) / total, 40, Kit::Rgba(255, 190, 90));
        rect(appStart, 70, 900.0f * static_cast<float>(app) / total, 40, Kit::Rgba(170, 220, 150));
        text(std::to_string(xna) + " XNA rooms    " + std::to_string(ext) + " extension rooms    " +
                 std::to_string(app) + " application rooms",
             34, 114, Kit::kText, 0.65f);
        if (!names_) return;
        std::string line;
        float y = 150.0f;
        for (const std::string& n : extNames) {
            if (line.size() + n.size() > 100) {
                text(line, 30, y, Kit::kDim, 0.5f);
                y += 22.0f;
                line.clear();
                if (y > 510.0f) break;
            }
            line += n + "   ";
        }
        if (!line.empty() && y <= 510.0f) text(line, 30, y, Kit::kDim, 0.5f);
    }

    Mode mode_ = Mode::Xna;
    float scroll_ = 0.0f;
    bool names_ = true;
};

}  // namespace

std::unique_ptr<Exhibit> MakeHistoryLab(const RoomDef& room, Services& services)
{
    return std::make_unique<HistoryLab>(room, services);
}
