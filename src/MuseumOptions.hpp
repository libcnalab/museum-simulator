#pragma once

#include <optional>
#include <string>

#include "Microsoft/Xna/Framework/Vector3.hpp"

// Command-line switches. They exist so the museum can be driven and verified
// without a human (screenshots, fixed frame counts, teleporting to a room).
struct MuseumOptions {
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool vsync = true;
    float mouseSensitivity = 0.0025f;   // radians per pixel
    float volume = 1.0f;
    bool audio = true;
    int maxFrames = 0;                  // 0 = run until the user quits
    std::string screenshotPath;         // written after `screenshotFrame`, then exits
    int screenshotFrame = 8;
    std::optional<std::string> startRoom;  // room id, e.g. "lighting"
    std::optional<std::string> renderer;   // --renderer NAME: CNA_GRAPHICS_RENDERER for this run (the browser has no environment)
    std::optional<float> startYawDegrees;
    std::optional<float> startPitchDegrees;
    std::optional<Microsoft::Xna::Framework::Vector3> startPosition;
    std::optional<float> startHours;   // set the museum clock (0..24) and keep it still unless --time-run
    bool timeRuns = false;             // the museum day advances by itself (T / Y also control it in the game)
    float fixedTimeStep = 0.0f;         // >0: advance the simulation by this many seconds per frame
    std::string script;                 // semicolon-separated test actions, see MuseumGame::RunScript
    std::optional<std::string> view;   // start at a named reference view (see --list-views)
    bool save = true;                  // keep the visited rooms between runs (off for scripted and screenshot runs)
    bool forceSave = false;            // --save: keep progress even in a scripted run (used to test saving)
    bool fresh = false;                // ignore the saved progress at start-up
    bool hud = true;                   // false: draw only the 3D scene (screenshots for comparison)
    bool mapMarkdown = false;          // print the museum map as Markdown (no window) and exit
    bool coverageMarkdown = false;     // print the API coverage table as Markdown (no window) and exit
    bool coverageJson = false;         // print catalogue API claims and rooms as JSON (no window) and exit
    bool listViews = false;            // print the reference views, one per line (no window) and exit
    bool showHelp = false;
    bool validate = false;              // check the catalogue (no window) and exit with the result
    bool listTurnedRooms = false;       // print the key of every room whose local frame is turned (entered from the north) and exit
    bool listRooms = false;             // print every room key, one per line (no window) and exit
    bool inventory = false;             // print the museum's counts as JSON (no window) and exit

    static MuseumOptions Parse(int argc, char** argv);
    static const char* Usage();
};
