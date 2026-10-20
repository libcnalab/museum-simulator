// The Native C API room: CNA's exported C ABI used from C17, not from C++. Two clients:
//   * the in-process one (src/CApi/NativeCProbe.c and CProbe*.c): every family that needs no game of its own, run
//     when the screen loads and by its button;
//   * the C game probe (src/CApi/GameProbe/, the separate program bin/cna-c-probe): the families that need a game
//     created from C through cna_game_create. A process hosts one game and the museum already is one, so the button
//     starts it as a child process on the museum's own display and environment, waits for it (with a timeout) and
//     logs its lines. Its HOME is an isolated copy of the media-library fixture, never the visitor's own folders.
// Every check is shown and logged as
//     [museum][check] <room> <name>: PASS|FAIL (expected ..., got ...)
// and a check the game probe could not run here as SKIP, never as PASS.
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#ifdef MUSEUM_C_API
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Rendering/AssetPaths.hpp"
#ifdef MUSEUM_C_API
#include "CApi/NativeCProbe.h"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

namespace fs = std::filesystem;

struct CCheck {
    std::string name, expected, got;
    char status = 'F';  // 'P' pass, 'F' fail, 'S' skip
};

class CApiLab final : public ScreenExhibit {
public:
    CApiLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("CNA's C ABI", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddButton("Run the C client again", [this] { Run(); });
        p.AddButton("Run the C game probe (separate process)", [this] { RunGameProbe(); });
        p.AddReadout("In the museum: passed / failed", [this] { return std::to_string(passed_) + " / " + std::to_string(failed_); });
        p.AddReadout("Game probe: passed / failed / skipped", [this] { return probeState_; });
        AddInfoSign(b, "A C17 program using CNA",
                    {"CNA exports a C ABI (CNA/C/*.h): plain structs for values, 64-bit handles for objects, a result code from every call and a",
                     "thread-local error message. The checks come from .c files compiled as C17 and linked against cna_c_api. A game written in C",
                     "(cna_game_create) needs a process of its own: the second button starts bin/cna-c-probe and waits for its report."});
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
        text("In the museum: " + std::to_string(passed_) + " passed, " + std::to_string(failed_) + " failed.   Game probe: " + probeState_, 24, 56, Kit::kText, 0.55f);
        // Failures first (they are what a visitor needs to see), then skips, then passes; as many as fit.
        std::vector<const CCheck*> shown;
        for (const char status : {'F', 'S', 'P'})
            for (const CCheck& c : checks_)
                if (c.status == status) shown.push_back(&c);
        float y = 92.0f;
        for (const CCheck* c : shown) {
            if (y > 510.0f) break;
            const Color colour = c->status == 'P' ? Kit::Rgba(120, 230, 140) : c->status == 'S' ? Kit::Rgba(230, 210, 120) : Kit::Rgba(255, 120, 110);
            text(std::string(c->status == 'P' ? "PASS " : c->status == 'S' ? "SKIP " : "FAIL ") + c->name.substr(0, 70), 24, y, colour, 0.45f);
            y += 26.0f;
        }
#else
        text("This build of the museum was compiled without its C client (MUSEUM_C_API, which needs CNA_BUILD_C_API).", 24, 100, Kit::Rgba(255, 190, 150), 0.75f);
#endif
        sb.End();
    }

private:
    void Record(const std::string& name, const std::string& expected, const std::string& got, char status)
    {
        checks_.push_back({name, expected, got, status});
    }

    void Run()
    {
        checks_.erase(std::remove_if(checks_.begin(), checks_.end(), [](const CCheck& c) { return c.name.rfind("game: ", 0) != 0; }), checks_.end());
        passed_ = failed_ = 0;
#ifdef MUSEUM_C_API
        museum_c_api_run(
            [](void* user, const char* name, const char* expected, const char* got, int pass) {
                auto* self = static_cast<CApiLab*>(user);
                self->Record(name, expected, got, pass != 0 ? 'P' : 'F');
                (pass != 0 ? self->passed_ : self->failed_)++;
                Museum::Log("check", "%s %s: %s (expected %s, got %s)", self->R().key.c_str(), name, pass != 0 ? "PASS" : "FAIL", expected, got);
            },
            this);
        Museum::Log("capi", "C client in the museum: %d passed, %d failed", passed_, failed_);
#endif
    }

#ifdef MUSEUM_C_API
    /** A check line the game probe printed, "[c-probe] game: <name>: PASS|FAIL|SKIP (...)", recorded and logged. */
    void TakeProbeLine(const std::string& line, int& passed, int& failed, int& skipped)
    {
        static const std::string prefix = "[c-probe] ";
        if (line.rfind(prefix + "game: ", 0) != 0) {
            Museum::Log("capi", "%s", line.c_str());
            return;
        }
        const std::string body = line.substr(prefix.size());
        Museum::Log("check", "%s %s", R().key.c_str(), body.c_str());
        char status = 'F';
        std::size_t at = std::string::npos;
        for (const char* word : {": PASS (", ": FAIL (", ": SKIP ("}) {
            const std::size_t found = body.find(word);
            if (found != std::string::npos && found < at) {
                at = found;
                status = word[2];
            }
        }
        (status == 'P' ? passed : status == 'S' ? skipped : failed)++;
        Record(at == std::string::npos ? body : body.substr(0, at), "", body, status);
    }

    /** Copies the media-library fixture (Content/MediaLibrary) into a fresh HOME for the child; never the visitor's own folders. */
    static bool PrepareFixtureHome(const fs::path& home, const fs::path& fixture, std::string& why)
    {
        std::error_code ec;
        fs::remove_all(home, ec);
        fs::create_directories(home / ".config", ec);
        fs::create_directories(home / ".local" / "share", ec);
        fs::create_directories(home / "gamer-profiles", ec);
        if (ec) {
            why = "cannot create " + home.string() + ": " + ec.message();
            return false;
        }
        if (fs::is_directory(fixture)) {
            fs::copy(fixture, home, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
            if (ec) {
                why = "cannot copy the media-library fixture: " + ec.message();
                return false;
            }
        }
        return true;
    }
#endif

    void RunGameProbe()
    {
        checks_.erase(std::remove_if(checks_.begin(), checks_.end(), [](const CCheck& c) { return c.name.rfind("game: ", 0) == 0 || c.name == "game probe"; }), checks_.end());
#ifdef MUSEUM_C_API
        std::error_code ec;
        const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
        const fs::path bin = exe.parent_path() / "cna-c-probe";
        if (ec || !fs::exists(bin)) {
            probeState_ = "not built";
            Record("game probe", "", "no " + bin.string(), 'S');
            Museum::Log("check", "%s game probe: SKIP (no cna-c-probe next to the museum binary: %s)", R().key.c_str(), bin.string().c_str());
            return;
        }
        const std::string content = AssetPaths::FindContentRoot();
        const fs::path home = exe.parent_path().parent_path() / "c-probe-home";
        std::string why;
        if (!PrepareFixtureHome(home, fs::path(content) / "MediaLibrary", why)) {
            probeState_ = "could not start";
            Record("game probe", "", why, 'F');
            Museum::Log("check", "%s game probe: FAIL (expected an isolated fixture home, got %s)", R().key.c_str(), why.c_str());
            return;
        }

        // The museum's own environment (display, renderer, audio driver, Mesa thread cap), with HOME and the data folders
        // moved into the fixture home. The X authority stays where the visitor's session keeps it.
        std::vector<std::string> env;
        const char* oldHome = std::getenv("HOME");
        bool hasXauthority = false;
        for (char** e = environ; *e != nullptr; ++e) {
            const std::string entry(*e);
            bool replaced = false;
            for (const char* name : {"HOME=", "XDG_CONFIG_HOME=", "XDG_DATA_HOME=", "CNA_GAMER_SERVICES_PROFILES_DIR="}) replaced = replaced || entry.rfind(name, 0) == 0;
            if (replaced) continue;
            if (entry.rfind("XAUTHORITY=", 0) == 0) hasXauthority = true;
            env.push_back(entry);
        }
        env.push_back("HOME=" + home.string());
        env.push_back("XDG_CONFIG_HOME=" + (home / ".config").string());
        env.push_back("XDG_DATA_HOME=" + (home / ".local" / "share").string());
        env.push_back("CNA_GAMER_SERVICES_PROFILES_DIR=" + (home / "gamer-profiles").string());
        if (!hasXauthority && oldHome != nullptr) env.push_back(std::string("XAUTHORITY=") + oldHome + "/.Xauthority");
        std::vector<char*> envp;
        for (std::string& e : env) envp.push_back(e.data());
        envp.push_back(nullptr);
        std::string binText = bin.string(), contentText = content, homeText = home.string();
        char* argv[] = {binText.data(), contentText.data(), homeText.data(), nullptr};

        int pipeFds[2];
        if (pipe(pipeFds) != 0) {
            probeState_ = "could not start";
            Museum::Log("check", "%s game probe: FAIL (expected a pipe, got errno %d)", R().key.c_str(), errno);
            return;
        }
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, pipeFds[0]);
        posix_spawn_file_actions_addclose(&actions, pipeFds[1]);
        pid_t pid = 0;
        const int spawned = posix_spawn(&pid, binText.c_str(), &actions, nullptr, argv, envp.data());
        posix_spawn_file_actions_destroy(&actions);
        close(pipeFds[1]);
        if (spawned != 0) {
            close(pipeFds[0]);
            probeState_ = "could not start";
            Museum::Log("check", "%s game probe: FAIL (expected the child to start, got posix_spawn %d)", R().key.c_str(), spawned);
            return;
        }
        Museum::Log("capi", "game probe started: pid %d, %s %s %s", static_cast<int>(pid), binText.c_str(), contentText.c_str(), homeText.c_str());

        // Waits (the museum pauses meanwhile) for the child's report, at most kTimeout; a child that overruns is killed by PID.
        constexpr auto kTimeout = std::chrono::seconds(120);
        const auto start = std::chrono::steady_clock::now();
        std::string pending;
        int passed = 0, failed = 0, skipped = 0;
        bool sawDone = false, timedOut = false;
        char buffer[4096];
        for (;;) {
            const auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > kTimeout) {
                timedOut = true;
                kill(pid, SIGKILL);
                break;
            }
            pollfd fd{pipeFds[0], POLLIN, 0};
            const int ready = poll(&fd, 1, 250);
            if (ready < 0 && errno == EINTR) continue;
            if (ready <= 0) continue;
            const ssize_t n = read(pipeFds[0], buffer, sizeof buffer);
            if (n <= 0) break;
            pending.append(buffer, static_cast<std::size_t>(n));
            for (std::size_t eol; (eol = pending.find('\n')) != std::string::npos;) {
                const std::string line = pending.substr(0, eol);
                pending.erase(0, eol + 1);
                if (line.rfind("[c-probe] done:", 0) == 0) sawDone = true;
                TakeProbeLine(line, passed, failed, skipped);
            }
        }
        close(pipeFds[0]);
        int status = 0;
        waitpid(pid, &status, 0);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        probeState_ = std::to_string(passed) + " / " + std::to_string(failed) + " / " + std::to_string(skipped);
        Museum::Log("capi", "game probe pid %d finished after %.1f s: %s %d, %d passed, %d failed, %d skipped", static_cast<int>(pid), seconds,
                    WIFEXITED(status) ? "exit status" : "signal", WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status), passed, failed, skipped);
        // A run that crashed, overran or never reported its summary is a failure of its own, whatever it printed before.
        if (timedOut || !sawDone || !WIFEXITED(status)) {
            const std::string got = timedOut ? "killed after 120 s" : !WIFEXITED(status) ? "terminated by signal " + std::to_string(WTERMSIG(status)) : "no summary line";
            Record("game probe", "", got, 'F');
            Museum::Log("check", "%s game probe: FAIL (expected a complete run ending in its summary line, got %s)", R().key.c_str(), got.c_str());
        }
#else
        Museum::Log("check", "%s game probe: SKIP (this museum was built without MUSEUM_C_API)", R().key.c_str());
#endif
    }

    std::vector<CCheck> checks_;
    int passed_ = 0, failed_ = 0;
    std::string probeState_ = "not run";
};

}  // namespace

std::unique_ptr<Exhibit> MakeCApiLab(const RoomDef& room, Services& services)
{
    return std::make_unique<CApiLab>(room, services);
}
