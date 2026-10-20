# Verifying the museum

Two profiles, kept apart on purpose:

- **Automatic**: scripts that run the museum on a private display (or none) and judge its own log lines. They never use
  the live desktop, the visitor's own folders, accounts, microphone, camera or loudspeakers. Each one prints PASS/FAIL
  (and SKIP or UNAVAILABLE where the environment cannot deliver something; neither is ever counted as a pass) and records
  its result for `tools/profile_summary.py`.
- **Manual**: what needs a person or hardware this machine does not have: [`HARDWARE_CHECKLIST.md`](HARDWARE_CHECKLIST.md).
  A manual step that was not done stays open; "no device here" is never a pass.

`tools/profile_summary.py` prints one table of every automatic profile with its passed / failed / skipped / unavailable /
not-run counts and whether the result belongs to the binary that exists now. Results of an older build are "stale", a
profile that never ran is "not run", and both count as not verified: the summary says "complete" only when every profile
is current and nothing failed.

## Prerequisites

| What | Needed by | Notes |
|---|---|---|
| CMake ≥ 3.23, Ninja, a C++23 compiler, ccache, SDL3 development files | every build | builds use at most two jobs (`JOBS=2 tools/build.sh`) and the shared ccache |
| `../cna` checkout | every build | the museum builds CNA from source; never edited from here |
| `~/deps/FNA3D`, `~/deps/wgpu-native-v29.0.1.1` | FNA3D, WEBGPU (`build-probe`) | shared checkouts, nothing is downloaded |
| Xvfb, `xvfb-run` | everything that renders | private virtual displays only |
| `xdotool` | devices, online | answers the museum's own message boxes and sign-in page on the private display |
| python3 with Pillow | reference views, room diffs | |
| python3 with numpy | audio, voice | FFT of the PCM written by SDL's `disk` audio driver |
| `sqlite3` | online | reads the private service's database |
| ffmpeg | `tools/gen_video.sh` (fixtures), video rooms (`MUSEUM_VIDEO=AUTO`) | |
| Google Chrome | web smoke | headless, SwiftShader WebGL 2 |
| Mesa radeonsi/RADV, `~/deps/weston` | `MUSEUM_GPU=1` / `MUSEUM_GPU=weston` | physical-GPU runs, see below |

## Automatic profiles

| Profile | Command | Expected result | Evidence |
|---|---|---|---|
| Native battery | `tools/check_all.sh` | every step `ok`: build, `museum_tests` (0 failed), `--validate`, generated docs, API inventory and coverage evidence current, press-all tour with 0 exhibit and 0 script errors, elevator/stairs to every floor, renderer self-checks, reference views within 1 %, memory over three tours within 25 % | `build/check_*.log` |
| Slow battery | `SLOW=1 tools/check_all.sh` | the above plus OPENGL33 and SOFTWARE tours, self-checks and memory | same |
| Scenarios | `tools/scenarios.py` | every scenario PASS (exact expected log lines; SKIP only when its build switch is off) | `build/scenarios/*.log` |
| Renderer self-checks | `tools/renderer_check.sh OPENGLES3 OPENGL33 SOFTWARE` | 0 FAIL in every rv-* room; SKIP only for what the renderer declares unsupported | `build/renderer_check/<R>.checks` |
| Other renderers | `BUILD_DIR=build-probe [VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json] tools/renderer_check.sh FNA3D WEBGPU SDL_GPU` | as above | `build-probe/renderer_check/` |
| Physical GPU, no display | `MUSEUM_GPU=1 tools/renderer_check.sh OPENGLES3 OPENGL33`; `MUSEUM_GPU=1 BUILD_DIR=build-probe VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/radeon_icd.json tools/renderer_check.sh SDL_GPU FNA3D` | as above; `<R>-gpu.device` names the driver Mesa's EGL / the Vulkan loader chose | `<R>-gpu.checks`, `<R>-gpu.device` |
| Physical GPU, private compositor | `MUSEUM_GPU=weston BUILD_DIR=build-probe VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/radeon_icd.json tools/renderer_check.sh WEBGPU` | as above (WEBGPU needs a window surface) | same |
| Devices wing | `tools/devices_check.sh` | 0 FAIL; UNAVAILABLE for what the machine lacks (no tray host, haptic device, camera opened only with `MUSEUM_ALLOW_CAMERA=1`) | `build/devices-check/` |
| Audio output | `tools/audio_check.sh` | the bell is 880 Hz, the synthesizer 440 Hz, MasterVolume 0 is silent | `build/audio-check/` |
| Network voice | `tools/voice_check.sh` | the talker's 300 Hz tone comes out of the listener | `build/voice-check/` |
| Host migration | `tools/net_check.sh` | round 1 (one local gamer on the host) PASS; round 2 FAILs while CNA's migration defect stands (`CNA_FINDINGS.md`) | `build/net-check/` |
| Online service | `tools/online_check.sh` | signed in through the Guide, rating and achievement stored by a private service | `build/online-check/` |
| Invitations, party, private slots, sign-out | `tools/online_party_check.sh` | three museums signed in as Visitor, Curator and Archivist: an invited gamer joins into the private slot, a party of two, SignedOut from the Guide | `build/online-party/` |
| WebGL 2 in a browser | `tools/build_web.sh` then `tools/web_smoke.sh` | `WEBGL2 BROWSER SMOKE: PASS` | Chrome console lines |
| The whole museum in a browser | `tools/web_tour.sh` (WEBGL2; `MUSEUM_WEB_RENDERER=WEBGPU` for browser WebGPU) | `WEBGL2 BROWSER TOUR: PASS`, every room visited | `build-consumer/web-tour/<renderer>/` |
| Scenarios in a browser | `tools/web_scenarios.py` | the same expectations as `tools/scenarios.py`; SKIP names its reason | `build-consumer/web-scenarios/<renderer>/` |
| Android (emulator) | `tools/build_android.sh` then `tools/android_run.sh` | `ANDROID RUN: PASS`, every room visited in batches | `build-consumer/android-run/` |
| Windows build under Wine | MinGW build (docs/RENDERER_TESTING.md) then `tools/wine_tour.sh -p` (`CNA_GRAPHICS_RENDERER=DIRECTX9` for Direct3D 9) | `WINE TOUR: PASS`, a Wine result, not Windows | `build-consumer/windows/` |
| API audit | `python3 tools/generate_api_inventory.py --check`, `python3 tools/audit_api_coverage.py --check` | inventory and evidence current; every unclaimed public header classified | `docs/CNA_API_INVENTORY.md`, `docs/CNA_COVERAGE_AUDIT.md` |
| Execution evidence | `tools/execution_coverage.py run --id <id> --build-dir build-probe --renderer OPENGLES3 --scenarios` (in a `-DMUSEUM_COVERAGE=ON` tree whose binary matches the sources) | one process per room, 0 errors; the audit then marks the implementation records whose every step ran | `audit/runs/<id>.*`, `audit/execution_evidence.json` |

Every automatic run keeps CNA's gamer profiles and saved data (`XDG_DATA_HOME`) in the build tree, never in the
user's own folders.

## Manual profile

[`HARDWARE_CHECKLIST.md`](HARDWARE_CHECKLIST.md): per device the room, the steps, what must be observed and what to keep
as evidence, plus the blockers of the machine this museum is developed on. Results go into `plan.md` (P12) with the
date, machine, revisions and evidence.
