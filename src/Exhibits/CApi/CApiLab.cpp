// The Native C API room: CNA's exported C ABI used from a real C17 translation unit (src/CApi/NativeCProbe.c), not from
// C++. The room runs that client, shows each of its checks and logs them as
//     [museum][check] <room> <name>: PASS|FAIL (expected ..., got ...)
// The client covers what needs no second game: ABI version, error diagnostics, value types and a handle's lifetime.
// A game written in C creates its own runtime through cna_game_create; that is the companion's business, not this room's.
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#ifdef MUSEUM_C_API
#include "CApi/NativeCProbe.h"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

struct CCheck {
    std::string name, expected, got;
    bool pass = false;
};

class CApiLab final : public ScreenExhibit {
public:
    CApiLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("CNA's C ABI", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddButton("Run the C client again", [this] { Run(); });
        p.AddReadout("Passed / failed", [this] { return std::to_string(passed_) + " / " + std::to_string(failed_); });
        AddInfoSign(b, "A C17 program using CNA",
                    {"CNA exports a C ABI (CNA/C/*.h): plain structs for values, 64-bit handles for objects, a result code from every call and a",
                     "thread-local error message. The checks on the screen come from a .c file compiled as C17 and linked against cna_c_api."});
    }

    void OnScreenLoaded() override { Run(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.6f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        text("CNA's native C ABI, from C17", 24, 12, Kit::kGold, 1.0f);
#ifdef MUSEUM_C_API
        float y = 64.0f;
        for (const CCheck& c : checks_) {
            text(std::string(c.pass ? "PASS " : "FAIL ") + c.name, 24, y, c.pass ? Kit::Rgba(120, 230, 140) : Kit::Rgba(255, 120, 110), 0.55f);
            text(c.got.substr(0, 64), 560, y, Kit::kText, 0.5f);
            y += 44.0f;
        }
#else
        text("This build of the museum was compiled without its C client (MUSEUM_C_API, which needs CNA_BUILD_C_API).", 24, 100, Kit::Rgba(255, 190, 150), 0.75f);
#endif
        sb.End();
    }

private:
    void Run()
    {
        checks_.clear();
        passed_ = failed_ = 0;
#ifdef MUSEUM_C_API
        museum_c_api_run(
            [](void* user, const char* name, const char* expected, const char* got, int pass) {
                auto* self = static_cast<CApiLab*>(user);
                self->checks_.push_back({name, expected, got, pass != 0});
                (pass != 0 ? self->passed_ : self->failed_)++;
                Museum::Log("check", "%s %s: %s (expected %s, got %s)", self->R().key.c_str(), name, pass != 0 ? "PASS" : "FAIL", expected, got);
            },
            this);
#endif
    }

    std::vector<CCheck> checks_;
    int passed_ = 0, failed_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeCApiLab(const RoomDef& room, Services& services)
{
    return std::make_unique<CApiLab>(room, services);
}
