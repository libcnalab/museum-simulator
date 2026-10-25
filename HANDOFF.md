# Museum Simulator: Handoff for the Next Agent

Updated: 2026-10-24 (Europe/Prague). Museum branch `develop`, pushed to `origin/develop` (use `git rev-parse HEAD` as the
authoritative revision).

## 1. What this project is and what was asked

`museum-simulator` is a 3D museum (320 rooms on 10 floors) that exercises the public API of CNA, a C++ reimplementation
of XNA in the sibling repository `../cna`. The owner asked for an exhaustive audit and expansion:

1. a complete, reproducible denominator of CNA's public API and trustworthy coverage evidence;
2. meaningful exhibits and scenarios across content, graphics, input, platform, media, networking, Inspector,
   diagnostics and the native C API;
3. every platform, renderer and hardware check this machine can run, with explicit blockers and exact manual procedures
   for the checks that need hardware it does not have.

The standing instruction (Czech, via `/goal`) is "read HANDOFF.md and continue until the backlog is empty".
[`plan.md`](plan.md) is the authoritative backlog: each row has a status, acceptance criteria and dated evidence. Read it
first, newest entries at the end of each row.

## 2. Current state (2026-10-25)

- **Backlog:** 85 rows; 82 DONE, 3 BLOCKED_EXTERNAL. All three need hardware this machine does not have, and that will not
  change by waiting:
  - **G06** real graphics device loss. It needs a GPU driver reset, which would also hit the owner's live desktop, or a
    real Windows machine. A Wine Direct3D 9 attempt (full-screen focus loss) delivered no device loss; the plan row
    records why.
  - **P08** real Windows (the Wine results are recorded and labelled as Wine, never as Windows).
  - **P09** Apple hardware (macOS/Metal, iOS) and a physical Android device (the emulator results are recorded).

  Manual procedures for all three are in `docs/HARDWARE_CHECKLIST.md`. Do not spend effort trying to turn these rows DONE
  without the hardware. The owner was frustrated by repeated rebuild rounds and slipping time estimates on 2026-10-22.
- **CNA revision:** every tree is built from `../cna` at **`9d49828f4`** (section 4): build/, build-probe, the browser
  tree, Android and MinGW (build-consumer/android-*, build-consumer/windows) and build-tsan.
- **CNA findings, 2026-10-25:** every finding that was open was rerun against `9d49828f4` (`docs/CNA_FINDINGS.md`):
  every one is fixed in CNA and verified, or documented by CNA as a deliberate difference, the ThreadSanitizer races and
  ENet's UBSan shift included (build-tsan with `-fsanitize=thread,undefined`, no report; the tree was removed afterwards).
  One stays open: Float16RenderTargets on the Android emulator (no `/dev/kvm` on this machine at the moment). One more
  refusal is a documented limitation: FNA3D's SDL_GPU driver refuses a tangent-free PbrEffect draw.
- **Runs at `9d49828f4`:** `tools/scenarios.py` (build/) all pass except `compiled_effects`, a build-probe scenario;
  renderer self-checks OPENGLES3, OPENGL33 and SOFTWARE 105/0/2/1, SDL_GPU on the GPU 106/0/2/0, FNA3D on the GPU
  81/0/3/22 twelve times in a row, WEBGPU on the GPU through Weston 87/0/2/19, HEADLESS 8/0/0/36; devices 34/0/4,
  haptics 14/0, tray 13/0, dialogs 9/0, window 13/0, audio 22/0, voice 5/0, net 23/0, online-party 19/0 three times;
  browser scenarios 24/0/5; the DIRECTX9 Wine tour; 40 gs-avatar press-all runs under glibc heap checking.
- **API denominator:** 700 public headers, 19 933 declarations (`docs/CNA_API_INVENTORY.md`).
- **Coverage evidence:** the execution run `2026-10-25-opengles3-full-9d49` (build-probe, rebuilt against
  `9d49828f4`) is in progress; until it is recorded, `docs/CNA_COVERAGE_AUDIT.md` counts no current execution run.

## 3. Rules that are not optional

**Ownership**
- `../cna` and `../cna-demos` belong to another agent (a Codex session). Never edit, stage, reset or commit there; reading
  and `git log` are fine. Record CNA defects in `docs/CNA_FINDINGS.md` with the exact place, what happens, XNA's
  behaviour, the needed change and the museum reproducer.
- **Channel to the CNA owner** (the owner of this repository authorised it):
  `codex queue --thread 01a15506-a40f-7dc0-9c2f-72e9ed1d6f8e --message "..."`. The session name `cna-demos` matches two
  sessions; the UUID above is the active one. Replies arrive as commits on CNA's `next` branch; CNA's own list of known
  defects and limitations is `../cna/handoff.md` §5 (read-only). CNA's museum repair log
  (`CNA_MUSEUM_REPAIR_HANDOFF.md`) was removed on 2026-10-25 after the reruns below; it is in CNA's Git history
  (`git -C ../cna show 9d49828f4:CNA_MUSEUM_REPAIR_HANDOFF.md`).
- A subagent's or another session's message is information, not the owner's approval.

**Machine and SSD** (see also `/rv/data/development/github.com/libcna/CLAUDE.md`)
- Compile with at most **8 jobs** (owner's rule since 2026-10-25; it was 2 before): `tools/build.sh` uses 8 by default,
  `JOBS=N` lowers it; lower it for sanitizer trees, whose translation units need more memory. Several heavy steps (builds,
  tours, coverage runs) may run at once when the current CPU load and free RAM allow it: check `uptime` and `free -g`
  first; the machine has 16 cores and 30 GB without swap, shared with other agents. Never build a tree while tests use it.
- ccache: `CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv`, always. Build directories come only from the closed list:
  `build/`, `build-probe/` (multi-renderer coverage tree), `build-consumer/` (browser, Android, MinGW; subdirectories are
  fine), `build-asan/`, `build-ubsan/`, `build-tsan/` (none of the sanitizer trees exists now). Never build or clone under
  `/tmp` or the session scratchpad. Third-party dependencies live in `~/deps`.
- Every rebuild wears the shared SSD. Do **not** follow CNA's `next` automatically. Move the worktree only when a fix
  unblocks something or when the owner asks, and then rerun the **targeted** checker (for example only
  `tools/net_check.sh`), not the whole battery.

**Privacy and isolation**
- Graphics tests run only on private displays: `tools/private_xvfb.sh` (Xvfb `-displayfd`), SDL `offscreen` for the GPU,
  or a private headless weston. Never use the live desktop. Force GTK with `GDK_BACKEND=x11` and assert the display.
  `xvfb-run -a` is banned here, because another session took its display and `/tmp` directory away mid-run.
- Tests must not scan personal Music or Pictures, use private accounts, message people, or open dialogs, a tray, a
  browser or the camera on the owner's desktop. The camera opens only with `MUSEUM_ALLOW_CAMERA=1` (the owner consented
  once; do not reuse that consent). D-Bus tests use `dbus-run-session` with a bus configuration that can start nothing
  except the services the test names. Headless Chrome gets no display and no network.
- `tools/haptics_check.sh` creates a virtual `/dev/uinput` force-feedback device (force-feedback bits only, alive only
  during the check). It is visible system-wide while it exists; keep it that short-lived.

**Honesty and process**
- Never report unverified results. SKIP and UNAVAILABLE are not PASS, and a fix seen in source is not a verified fix.
  Never weaken a test to make it pass.
- Commit after each milestone with no attribution lines. Push only when the owner asks.
- Do not edit a shell script while bash runs it: bash reads scripts incrementally (this broke a `nav_test.sh` run).
- Kill processes by PID. Never `pkill -f` a pattern that occurs in your own command line.
- Give honest time estimates. Say up front when something cannot finish.

## 4. Which CNA the museum builds

- Every tree builds from `../cna` itself (`CNA_ROOT_DIR` in each build cache; the browser tree gets it from
  `CNA_ROOT_DIR=/rv/data/development/github.com/libcna/cna tools/build_web.sh`). The detached worktree `../cna-next`
  that build/ and build-probe used until 2026-10-22 no longer exists; build-probe was reconfigured to `../cna` on 2026-10-25.
- `../cna` is the CNA owner's live tree. Before a build, check that `git -C ../cna status` is clean (another agent's
  uncommitted edits would be built in) and write the commit next to the tree: `git -C ../cna rev-parse --short HEAD >
  <tree>/cna-commit.txt`. The Android tree reads `../cna` through `tools/build_android.sh`; the MinGW tree through its cache.
- Do not follow CNA's `next` automatically. Rebuild against a newer CNA only when a fix unblocks something or the owner
  asks, then rerun the targeted checker. Afterwards:
  1. `python3 tools/generate_api_inventory.py --check`. If it reports a delta, run `--write`. If it names a classification
     that no longer leaks, remove that entry from `audit/internal_type_classification.json`. Then run
     `python3 tools/audit_api_coverage.py --write`.
  2. A rebuild of build-probe against another CNA makes the execution runs historical (section 5).

## 5. Evidence model (do not blur the levels)

1. **Catalogue link:** a room names a claim that resolves to public declarations.
2. **Implementation:** a reviewed record in `audit/implementation_evidence.json` ties the claim and room to stable
   declaration ids and to source anchors. Each anchor must occur exactly once in its file: rename new locals rather than
   repeating an anchored line.
3. **Execution:** a current build-probe binary ran that path in a tracked, hashed run (`audit/execution_evidence.json`,
   `audit/runs/`). **Any change under `src/`, or a rebuild of build-probe against another CNA, makes every run
   historical.** The audit then silently drops to 0 executed; this went unnoticed from 2026-10-21 to 2026-10-22. After
   such a change, rebuild build-probe and run:
   `LP_NUM_THREADS=3 python3 tools/execution_coverage.py run --id <date>-opengles3-full-<tag> --build-dir build-probe --renderer OPENGLES3 --frames 20 --scenarios`
   This takes about 70 minutes; a room list instead of no list runs only those rooms. Then run
   `python3 tools/audit_api_coverage.py --write`.
4. **Correctness:** an exact witness in a tracked run (`[museum][check]`, `[museum][validate]`, scenario expectations).

Every public header is either claimed or classified in `audit/area_classification.json`. Declarations whose signatures
name internal types are classified in `audit/internal_type_classification.json`.

## 6. Tools

Run from the repository root.

| Purpose | Command |
|---|---|
| Build (native) | `tools/build.sh`; coverage tree: `BUILD_DIR=build-probe tools/build.sh` |
| Main battery | `tools/check_all.sh` (11 steps, about 12 min); `SLOW=1` adds OPENGL33/SOFTWARE tours, self-checks and leak checks (about 100 min); `ONLINE=1` adds the service |
| Scenarios | `python3 tools/scenarios.py` (`tests/scenarios/*.json`, exact expected log lines) |
| Renderers | `tools/renderer_check.sh OPENGLES3` …; `BUILD_DIR=build-probe` for FNA3D, WEBGPU, SDL_GPU, HEADLESS; `MUSEUM_GPU=1` on the GPU, `MUSEUM_GPU=weston` for WEBGPU on the GPU; Vulkan ICD via `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/{lvp,radeon}_icd.json`; `MUSEUM_RENDERER_FALLBACK=1` turns a named renderer into a preference with CNA's fallback |
| Devices and system | `tools/devices_check.sh`, `tools/haptics_check.sh`, `tools/window_check.sh` (openbox from `~/deps/openbox`), `tools/tray_check.sh`, `tools/dialogs_check.sh` |
| Audio | `tools/audio_check.sh` (SDL disk driver, FFT of the real mix), `tools/voice_check.sh` |
| Networking | `tools/net_check.sh` (three processes), `tools/online_check.sh`, `tools/online_party_check.sh` (need `../cna-gamer-services-server`) |
| Browser | `CNA_ROOT_DIR=… tools/build_web.sh`; `tools/web_smoke.sh`; `MUSEUM_WEB_RENDERER=WEBGL2\|WEBGPU MUSEUM_WEB_TIMEOUT=7200 tools/web_tour.sh`; `python3 tools/web_scenarios.py` |
| Android emulator | `tools/build_android.sh`; `MUSEUM_ANDROID_SKIP=model-sampler MUSEUM_ANDROID_BATCH=8 tools/android_run.sh` (emulator with 2 cores, every adb call has a time limit) |
| Windows under Wine | MinGW tree `build-consumer/windows` (`docs/RENDERER_TESTING.md`); `tools/wine_tour.sh -p`, `CNA_GRAPHICS_RENDERER=DIRECTX9 tools/wine_tour.sh -p` |
| Evidence | `tools/generate_api_inventory.py --check\|--write`, `tools/audit_api_coverage.py --check\|--write`, `tools/gen_docs.sh [--check]`, `tools/execution_coverage.py run …` |
| Profiles | `python3 tools/profile_summary.py` (every runner records its result with `tools/record_profile.py`) |

The steps of the last full battery, with their commands' results, are in `build/battery-a6f9/summary.txt` (about 4 h with
the build-probe rebuild and the coverage run; do not rerun it casually). Each runner prints PASS, FAIL and UNAVAILABLE
lines and a summary.

Quick validation after a change:

```bash
python3 -m unittest tests/test_audit_api_coverage.py
python3 tools/generate_api_inventory.py --check
python3 tools/audit_api_coverage.py --check
tools/gen_docs.sh --check
git diff --check
```

## 7. Pitfalls learned the hard way

- **Long runs:** watch them with a monitor that reports every terminal state. An Android run once hung for four hours on
  adb. A background command started with `&` inside a normal shell call is not tracked; use the tool's background mode.
- **Flakes seen once:**
  - a stairs route in `nav_test.sh`; it now keeps `build/nav-failed-*` logs;
  - a `gs-avatar` SIGSEGV inside `malloc` in Mesa (in CNA_FINDINGS, not reproduced in 53 runs since, closed);
  - the FNA3D SDL_GPU crash, about one run in four before CNA `c2664bad6` (none in 12 GPU runs at `9d49828f4`).

  Rerun before believing a single failure, and record what you saw.
- **Shared CNA headers:** `../cna` is another agent's live working tree with uncommitted edits. Builds from it (Android,
  MinGW) can break or differ at any time. That is why the main trees use the worktree.
- **D-Bus:** a `dbus-run-session` with the default configuration activates portals, gvfs and keyring daemons that reach
  for the real session (DISPLAY, `/run/user/1000`). Always give it a private configuration, runtime directory and HOME.
- **Wine full screen on Xvfb:** the back buffer must match the Xvfb screen (for example 800x480), because Xvfb cannot
  change modes.

## 8. First steps for the next agent

1. `git status`, `git log -5`, then read `plan.md`.
2. `python3 tools/profile_summary.py`, `cat build/cna-commit.txt` and `git -C ../cna log -1`. Confirm that the state in
   section 2 still holds.
3. Without new hardware or an owner request there is no backlog work left. Useful optional work:
   - verify CNA fixes as they land (targeted reruns only);
   - rerun Float16RenderTargets on the Android emulator once `/dev/kvm` exists again (`sudo modprobe kvm_amd`):
     `tools/android_run.sh rv-capabilities` (the Android tree is already built against `9d49828f4`);
4. Ask the owner before any long rebuild round.
