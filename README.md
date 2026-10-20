# CNA 3D Museum

A first-person 3D museum that teaches the [CNA](../cna) framework (a C++ port of the XNA 4.0 API with extras) by
letting you walk through it. Ten floors of exhibit rooms; every room demonstrates one thing, labels the API it is
using, says whether that API is **XNA 4.0** or a **CNA extension**, and has something to click, drag or toggle.

The museum is an ordinary CNA application: it uses CNA's public API directly and adds no engine of its own. The only
code above CNA is application structure (rooms, portals, a floor map, an exhibit framework).

| | | |
|---|---|---|
| ![lobby](docs/images/lobby.jpg) | ![vectors](docs/images/vectors.jpg) | ![instancing](docs/images/instancing.jpg) |
| The entrance hall | Floor 1: the cross product | Floor 3: instanced drawing |
| ![lights](docs/images/lights.jpg) | ![shader](docs/images/shader.jpg) | ![mirror](docs/images/mirror.jpg) |
| Floor 4: light types | Floor 4: a CRT shader | Floor 5: a planar mirror from a render target |
| ![pbr](docs/images/pbr.jpg) | ![fire](docs/images/fire.jpg) | ![debugdraw](docs/images/debugdraw.jpg) |
| Floor 6: a glTF model with PbrEffect | Floor 6: a particle fire | Floor 8: CNA's DebugDraw |
| ![formats](docs/images/formats.jpg) | ![validation](docs/images/validation.jpg) | ![deck](docs/images/deck.jpg) |
| Floor 8: image formats decoded by ContentManager | Floor 9: renderer self-checks | Floor 9: the observation deck |
| ![sampler](docs/images/sampler.jpg) | ![colours](docs/images/colours.jpg) | ![fontbuild](docs/images/fontbuild.jpg) |
| Floor 6: glTF sampler states (repeat, clamp, mirror) | Floor 1: the 141 named colours | Floor 8: a font built at run time |
| ![shadererrors](docs/images/shadererrors.jpg) | ![meshbuilder](docs/images/meshbuilder.jpg) | ![modelimport](docs/images/modelimport.jpg) |
| Floor 4: what a shader compile error looks like to CNA | Floor 8: a model built with MeshBuilder and ModelProcessor | Floor 8: a .x or .fbx file imported and processed |
| ![compiler](docs/images/compiler.jpg) | ![volshader](docs/images/volshader.jpg) | ![diagnostics](docs/images/diagnostics.jpg) |
| Floor 8: an .xnb written by ContentCompiler and loaded back | Floor 3: a Texture3D read by a sampler3D (AddressW) | Floor 8: CNA::Diagnostics zones and a recorded trace |

All images are real frames from the running museum (`tools/readme_shots.sh`), not mock-ups.

## What is in it

<!-- BEGIN COUNTS -->
```
floors:          10
wings:           38
exhibit rooms:   320  (7 outdoors)
walkable cells:  397  (rooms, galleries, concourses, halls, elevators, stairs)
exhibit kinds:   78
origin:          243 XNA 4.0, 75 CNA extension, 2 application
API names:       2903
  floor 0  12 rooms  Lobby and Introductory Tour
  floor 1  55 rooms  Fundamentals
  floor 2  23 rooms  Two-Dimensional Rendering
  floor 3  22 rooms  Three-Dimensional Fundamentals
  floor 4  30 rooms  Effects and Lighting
  floor 5  24 rooms  Render State and Render Targets
  floor 6  39 rooms  Models, Animation and Particles
  floor 7  28 rooms  Audio and Input
  floor 8  62 rooms  Content, Platform and CNA
  floor 9  25 rooms  Performance and Validation
```
<!-- END COUNTS -->

The numbers above are generated from the museum's own catalogue (`tools/gen_docs.sh`). The floors:

| Floor | Subject |
|---|---|
| 0 | Entrance lobby and a first tour: models, materials, lighting, sky, render state, targets, particles, animation, audio |
| 1 | Fundamentals: application model, vectors, matrices, rotations, curves, collision geometry, cameras |
| 2 | Two-dimensional rendering: SpriteBatch in depth, SpriteFont and text |
| 3 | Three-dimensional fundamentals: buffers, primitives, dynamic geometry, instancing, textures |
| 4 | Effects and lighting: BasicEffect super-wing, a lighting lab, CNA ShaderEffect shaders |
| 5 | Render state gallery and render-target complex (mirrors, MRT, float targets) |
| 6 | Models (glTF, PBR, SkinnedEffect), animation, particles, simulations (flocking, collisions, integrators, inverse kinematics) |
| 7 | Audio (3D audio, synthesis, music with a visualizer, video, microphone) and input (keyboard, mouse, game pad, text, joystick) |
| 8 | Content pipeline, CNA extensions (DebugDraw, CRT, depth and ASCII effects, shader packages, shadow mapping, statistics, devices), platform queries, players and networking (GamerServices, avatars, leaderboards, the Guide, achievements, packets, sessions you can find and join from a second museum, and with a local account service: accounts, friends, messages, parties, online sessions and invitations), a history of XNA and of this build of CNA |
| 9 | Performance lab, renderer validation, reference scenes, the observation deck |

Maps and indexes: [docs/MUSEUM_MAP.md](docs/MUSEUM_MAP.md) (every room), [docs/CNA_COVERAGE.md](docs/CNA_COVERAGE.md)
(what CNA covers and what does not), [docs/REFERENCE_VIEWS.md](docs/REFERENCE_VIEWS.md). How to verify it:
[docs/VERIFICATION.md](docs/VERIFICATION.md) (automatic profiles) and [docs/HARDWARE_CHECKLIST.md](docs/HARDWARE_CHECKLIST.md)
(manual, hardware); CNA defects the museum found: [docs/CNA_FINDINGS.md](docs/CNA_FINDINGS.md).

## Build

Requirements: a C++23 compiler, CMake 3.23+, Ninja (optional), CNA and its sibling checkouts next to this one
(`../cna`, `../sharp-runtime`, `../meta-gl`, ...), SDL3 development files as CNA requires. ccache is used when it is
installed and is optional. On Linux the museum has been built and run with GCC 14.

```bash
tools/build.sh                         # configure on first use, then build; prints only errors and the museum's own warnings
build/bin/cna-3d-museum                # run it
```

`tools/build.sh [TARGET...]` always builds in `build/` (set `BUILD_DIR` to use another tree). Useful variants:

```bash
# A build that can run on several renderers; the first is the default, CNA_GRAPHICS_RENDERER picks one at run time.
MUSEUM_RENDERERS="OPENGLES3;OPENGL33;SOFTWARE" tools/build.sh
CNA_GRAPHICS_RENDERER=SOFTWARE build/bin/cna-3d-museum
# With CNA_GRAPHICS_RENDERER unset the museum lets CNA fall back across the compiled-in renderers when the default cannot start
# (for example no X server: OPENGLES3 and OPENGL33 are passed over, SOFTWARE runs); each fallback is logged and shown in the
# "This Build of CNA" room. A renderer you name is never replaced: if it fails, the museum fails.

RECONFIGURE=1 tools/build.sh           # force a CMake configure
VERBOSE=1 tools/build.sh               # stream the whole build log
MUSEUM_CMAKE_ARGS="-DCNA_CNAEXT=OFF" RECONFIGURE=1 tools/build.sh    # build without CNA's extension module
```

`MUSEUM_CMAKE_ARGS` accepts shell quoting, so a value containing spaces stays one CMake argument (for example
`MUSEUM_CMAKE_ARGS='-DCMAKE_CXX_FLAGS="-O1 -g1"'`). The build script uses at most two compile jobs by default;
`JOBS` may lower that limit but must not raise it for this project.

The museum switches on three CNA options in its `CMakeLists.txt`: `CNA_CNAEXT` (DebugDraw and the post-process
effects), `CNA_DEVICES` (device queries) and `CNA_DIAGNOSTICS=FULL` (engine counters and the CPU-zone timeline). Each exhibit that needs one
checks for it and says so in the room when it is missing (a build with all three off was tried: it compiles, every room runs, and those rooms show a notice).

The build also compiles `Content/Pipeline/` with CNA's own content compiler (`cna-content`) into `.cnb` and `.xnb`
files next to the executable; that is what the *Content Compiler* room loads.

### WebGL 2 browser build

With an Emscripten SDK installed (the script uses `$EMSDK_ROOT`, `$EMSDK`, or `/home/robertvokac/emsdk`), build and
serve the browser edition like this:

```bash
tools/build_web.sh
python3 -m http.server 8000 --directory build-consumer/bin
# open http://127.0.0.1:8000/cna-3d-museum.html
```

Serve it over HTTP; browsers do not allow the `.wasm` and preloaded data bundle to work correctly from a `file://`
URL. The deployable files are `cna-3d-museum.html`, `.js`, `.wasm` and `.data`. `tools/web_smoke.sh` serves those
files temporarily and verifies rendering plus deferred IndexedDB autosave in headless Chrome. The web profile keeps CNA's in-game content
pipeline, but leaves out networking/GamerServices, the Inspector and native FFmpeg video; their rooms report that
those optional services are unavailable.

## Controls

| Key | Action |
|---|---|
| `W A S D` / `Up Down` | move (Shift toggles walk/run) |
| mouse, or `Left Right` and `Page Up/Down` | look around |
| `E` or left click | interact with what the crosshair is on (buttons, sliders, doors) |
| `Q` or right click | previous choice on a panel |
| `M` / `Tab` | the directory and map: floor tabs, search, quick travel |
| `N` | minimap on/off |
| `T`, `Shift+T`, `Y` | museum clock: hour later/earlier, let the day run |
| `F1` / `H` | controls overlay |
| `F3` | statistics overlay |
| `F2` | mute audio |
| `[` `]` | mouse sensitivity |
| `R` | back to the entrance |
| `Esc` | pause menu (the mouse is released) |
| `F11` | full screen |

Game pad (optional): left stick walk, right stick look, A interact, Start pause. No exhibit requires a pad, a
joystick, a touch screen or a microphone. In *The Game Pad* room CNA's keyboard-emulated pad lets you try one without
hardware.

### Finding your way

- Every floor has a hall, a concourse and wings. Room numbers (`4.2.07`) are on each room's door plaque.
- The **elevator** is east of every hall: ten buttons, one per floor.
- The **stairs** are west of the halls: each signed switchback is a walkable route to the next floor, with a second
  shaft carrying the following flight.
- The **directory** (`M`) lists every room by floor and wing with a map, supports search, and quick-travels
  (Enter, or click twice) to rooms on floors you have visited.
- The **minimap** (top right) follows you; the floor numeral on the hall rug and the floor accent colour tell you where you are.
- Room banners and the door-side plaque name the API, say what the controls do and what to expect.

## Command line

`build/bin/cna-3d-museum --help` lists all options. The ones used by the tools:

| Option | Purpose |
|---|---|
| `--room KEY`, `--pos X,Y,Z`, `--yaw`, `--pitch` | start somewhere specific (`--list-rooms` prints the keys) |
| `--view NAME`, `--no-hud`, `--time HOURS` | a named reference view, without overlay, at a fixed museum hour |
| `--screenshot FILE [--screenshot-frame N]` | save one frame and exit |
| `--script "a;b;c"` | scripted actions: `goto:`, `at:`, `walk:`, `press:LABEL=V`, `use:`, `time:`, `shot:`, `quit` ... |
| `--fixed-step S`, `--frames N` | deterministic, bounded runs |
| `--validate`, `--inventory`, `--list-rooms`, `--list-turned-rooms`, `--list-views`, `--map-markdown`, `--coverage-markdown` | data-only commands, no window |
| `--no-audio`, `--volume`, `--width`, `--height`, `--fullscreen` | usual switches |
| `--no-save`, `--fresh`, `--save` | the visited rooms are kept between runs in CNA's per-user storage (`StorageDevice`); scripted and screenshot runs never touch it unless `--save` |

## Testing

| Tool | What it checks |
|---|---|
| `build/bin/museum_tests` (also `ctest --test-dir build`; build it with `tools/build.sh cna-3d-museum museum_tests`) | catalogue validity, unique keys and numbers, door symmetry, whole-museum walking connectivity, exact arrival poses, spawn/cell lookup, exhibit registry, missing assets |
| `cna-3d-museum --validate` | the same structure checks without a window; the museum also runs them at every start-up and logs any `INVALID:` problem |
| `tools/online_stack.sh start\|stop\|env` | starts a private CNA gamer-services server (sibling repo, built once) with demo accounts for the *Online Service* rooms; `eval "$(tools/online_stack.sh env)"` points the museum at it |
| `tools/online_check.sh` | end-to-end check of those rooms: signs in through the Guide with xdotool, plays a scored round and reads the service's database |
| `tools/tour.sh [-f FRAMES] [-p] [-z N] [ROOM...]` | visits **every** room (or those named), prints engine statistics, exit status 0 only if nothing threw; `-p` also operates every control of every room through its whole range, `-z N` then makes N random control actions per room (a repeatable fuzz) |
| `tools/leak_check.sh [RENDERER...]` | tours every room once and three times on the selected renderer(s): peak memory and registered textures must not grow |
| `tools/nav_test.sh` | rides the elevator to every floor, walks all nine stair flights up and one down, and checks every arrival floor |
| `tools/renderer_check.sh [RENDERER...]` | runs the floor 9 *Renderer Validation* self-checks on each renderer and prints a PASS/FAIL matrix |
| `tools/reference.sh capture\|compare` | visual regression: renders the named reference views and compares them with stored images |
| `tools/room_diff.sh RENDERER` | every room's arrival view on RENDERER against OPENGLES3, ranked by differing pixels (needs both in one build) |
| `tools/check_all.sh` | the whole battery on the default renderer (build, tests, validation, documentation freshness, tour with every control, navigation, renderer checks, visual regression, memory); `SLOW=1` adds tours, self-checks and memory checks on OPENGL33 and SOFTWARE |
| `tools/web_smoke.sh` | serves the Emscripten bundle locally and checks a real Chrome WebGL 2 start, idle-deferred autosave, statistics, zero exhibit errors and clean exit |
| `tools/run.sh "script"`, `tools/shot.sh OUT.png ...`, `tools/roomshots.sh OUT.png KEY...` | run a script, take a screenshot, or a contact sheet of rooms, on a private virtual display (Xvfb) |

None of the scripts touches your real display. Failures are never swallowed: a crash, a timeout or a missing
screenshot is a non-zero exit status.

## Renderers

The museum uses only portable CNA calls, so any 3D-capable renderer should work; it checks capabilities and says so
in the room when something is unavailable (for example wireframe fill on OPENGLES3). Tested here (the last three need CNA's optional dependencies; see [docs/RENDERER_TESTING.md](docs/RENDERER_TESTING.md)):

| Renderer | Whole museum tour | Self-checks (floor 9) | Notes |
|---|---|---|---|
| OPENGLES3 (default) | all rooms clean | all pass | the reference renderer; `tests/reference/OPENGLES3` |
| OPENGL33 | all rooms clean | all pass | screenshots identical to OPENGLES3 |
| SOFTWARE | all rooms clean (about 25 minutes under Xvfb) | all pass | CPU rasteriser; no window, so mouse look is off: use the keyboard |
| SOFTWARE, Windows build under Wine | all rooms clean (MinGW cross build) | - | `docs/RENDERER_TESTING.md` has the recipe; found a Windows-only bug (`localtime_r`) |
| FNA3D | all rooms clean | 63 pass, 0 fail, 1 info, 23 skipped | runs compiled XNA `.fxb` effects (the only one that does), including the museum's real matrix-stream instancing path and CNA's rigid/skinned PBR with IBL, double-sided materials, independent UV sets and shadow reception, plus Basic/Skinned punctual-light and shadow extensions; no custom source shaders |
| WEBGPU (llvmpipe) | all rooms clean | all pass | takes WGSL: the shader rooms run on WGSL sources the museum carries; the MRT edge map and the shadow-map caster do not |
| SDL_GPU (llvmpipe) | all rooms clean | all pass | takes SPIR-V: the shader rooms compile Vulkan GLSL at run time; the shadow-map caster does not |
| WEBGL2 (Emscripten) | browser smoke clean | not yet run | Chrome/SwiftShader rendered the lobby and exited with 0 exhibit errors; `tools/build_web.sh` |

Details, the per-renderer check matrix and what was found are in [docs/RENDERER_TESTING.md](docs/RENDERER_TESTING.md).

## Known limitations

See [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md). In short: the museum has walkable stairs but no moving escalators;
real audio output was verified to start and
stream on a PipeWire machine but audibility cannot be judged by a test; the Windows (MinGW) build leaves out
networking, GamerServices and avatars and the video player (no libcurl or FFmpeg there); friends, invitations, parties,
voice and online sessions need an account service and are not demonstrated (see
[docs/CNA_COVERAGE.md](docs/CNA_COVERAGE.md)).

## Repository layout

| Path | Contents |
|---|---|
| `src/Catalogue` | the museum as data: floors, wings, rooms, themes, reference views, inventory |
| `src/World` | layout, lazy building and activation of rooms, portals and visibility, collision |
| `src/Exhibits` | the exhibit framework and one directory per subject (`Math`, `Sprites`, `Geometry`, `Perf`, `Validation`, ...) |
| `src/Rendering`, `src/Interaction`, `src/UI`, `src/Audio`, `src/Camera`, `src/Diagnostics` | application helpers (gizmos, control panels, HUD, floor map, player controller) |
| `Content/` | procedural textures, glTF models, WAV sounds, an XACT project, fonts, content-pipeline samples (generated by `tools/gen_*.py`) |
| `web/` | Emscripten HTML shell for the WebGL 2 browser build |
| `tests/` | structure tests and the visual-regression reference images |
| `tools/` | build, run, test, documentation and asset-generation scripts |
| `docs/` | map, coverage, architecture, renderer testing, known issues, CNA API notes |
| `HANDOFF.md` | current state and next steps for whoever continues |

Architecture: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## License

MIT, see [LICENSE](LICENSE). The bundled DejaVu font keeps its own license (`Content/Fonts/LICENSE-DejaVu.txt`).
