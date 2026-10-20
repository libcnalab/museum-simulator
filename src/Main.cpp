#include "Catalogue/Inventory.hpp"
#include "Catalogue/ReferenceViews.hpp"
#include "CNA/GraphicsRendererSelection.hpp"
#include "Microsoft/Xna/Framework/Storage/StorageDevice.hpp"
#include "MuseumGame.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    try {
        const MuseumOptions options = MuseumOptions::Parse(argc, argv);
        if (options.showHelp) {
            std::fputs(MuseumOptions::Usage(), stdout);
            return 0;
        }
        // Data-only commands: they need no window, device or content, and their exit status is the result.
        if (options.listRooms) {
            for (const RoomDef& r : TheMuseum().rooms)
                if (r.kind == CellKind::Room) std::printf("%s\n", r.key.c_str());
            return 0;
        }
        if (options.listTurnedRooms) {
            for (const RoomDef& r : TheMuseum().rooms)
                if (r.kind == CellKind::Room && r.Turned()) std::printf("%s\n", r.key.c_str());
            return 0;
        }
        if (options.mapMarkdown || options.coverageMarkdown || options.coverageJson) {
            const MuseumPlan& museum = TheMuseum();
            const std::string output = options.mapMarkdown ? MapAsMarkdown(museum)
                : options.coverageMarkdown ? CoverageAsMarkdown(museum)
                : CoverageAsJson(museum);
            std::fputs(output.c_str(), stdout);
            return 0;
        }
        if (options.listViews) {
            for (const ReferenceView& v : ReferenceViews()) std::printf("%s\t%s\t%s\n", v.name, v.room, v.shows);
            return 0;
        }
        if (options.validate || options.inventory) {
            const MuseumPlan& museum = TheMuseum();
            const std::vector<std::string> errors = ValidateMuseum(museum);
            for (const std::string& e : errors) std::fprintf(stderr, "invalid: %s\n", e.c_str());
            if (options.inventory) std::fputs(InventoryAsJson(museum).c_str(), stdout);
            else std::fputs(InventoryAsText(museum).c_str(), stdout);
            std::fprintf(stderr, "%s\n", errors.empty() ? "museum structure: ok" : "museum structure: INVALID");
            return errors.empty() ? 0 : 1;
        }
        // StorageDevice also backs GamerServices' local achievements and leaderboards. Set the
        // title name before either subsystem can initialize so their root never depends on which
        // exhibit (or whether progress persistence) happened to open storage first.
        // CNA resolves (and creates) the storage root right here; without a writable per-user data folder that throws
        // StorageDeviceNotConnectedException after the name is set. The museum runs on without saved data then.
        try {
            Microsoft::Xna::Framework::Storage::StorageDevice::SetAppNameEXT("cna-3d-museum");
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[museum][storage] no storage root, saved data is off: %s\n", e.what());
        }
#ifdef MUSEUM_NETWORKING
        // Two local offline profiles are signed in at start-up (unless the caller chose others): the networking rooms need two local
        // gamers to talk to each other. They live in CNA's own profile store, never in the museum's files.
        // With a service configured (tools/online_stack.sh) the profiles are real accounts, signed in through the Guide.
        if (std::getenv("CNA_GAMER_SERVICES_AUTO_SIGN_IN") == nullptr && std::getenv("CNA_GAMER_SERVICES_ENDPOINT") == nullptr) {
#ifdef _WIN32
            _putenv_s("CNA_GAMER_SERVICES_AUTO_SIGN_IN", "Visitor,Guest");
#else
            setenv("CNA_GAMER_SERVICES_AUTO_SIGN_IN", "Visitor,Guest", 0);
#endif
        }
#endif
        // Nobody named a renderer: let CNA fall back across the ones compiled in (mature GPU renderers first, CPU renderers next) when the
        // default cannot start on this machine, so a visitor still gets a museum. A named renderer (CNA_GRAPHICS_RENDERER) stays an
        // error when it fails, and every fallback is logged.
        if (std::getenv("CNA_GRAPHICS_RENDERER") == nullptr) CNA::GraphicsRendererSelection::EnableAutomaticFallback(true);
        MuseumGame game(options);
        game.Run();
        return 0;
    } catch (const std::exception& failure) {
        std::fprintf(stderr, "cna-3d-museum: %s\n", failure.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "cna-3d-museum: unknown error\n");
        return 1;
    }
}
