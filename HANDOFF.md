# Museum Simulator CNA Coverage Handoff

Updated: 2026-10-20 (Europe/Prague)

## Purpose and authority

The user requested an exhaustive audit and expansion of `museum-simulator` against the
actual public API of the sibling CNA checkout. The work includes:

1. a complete, reproducible public API denominator and trustworthy coverage evidence;
2. meaningful missing exhibits and scenarios across content, graphics, input, platform,
   media, networking, Inspector, diagnostics, and the native C API;
3. every platform, renderer, and hardware check available in this environment, with
   explicit blockers and exact procedures for checks that require unavailable hardware.

Read [`plan.md`](plan.md) before making changes. It is the authoritative backlog and
records task state, dependencies, acceptance criteria, discoveries, and evidence. Do not
declare completion until every applicable row is complete or has a documented external
blocker and all final gates pass.

The user resumed the goal on 2026-10-19 ("read the handoff and continue until the backlog is empty").

## Repository ownership and concurrency

- Repository: `museum-simulator`, branch `develop`.
- The audit started from museum commit `d34c4a9ec50b176f754b26bc71b60a4091694dc1`.
  This handoff and the audit implementation are in the commit immediately after that
  baseline; use `git rev-parse HEAD` as the authoritative museum revision.
- `../cna` is a separate repository owned by another active agent. Do not alter, stage,
  reset, or commit its files. The other agent is also responsible for `cna-demos`.
- `cna-demos` is only a comparison source for task A10. Its 719 demo IDs and 155 issue
  records are not work to duplicate in this repository.
- Builds must use at most two compile jobs (`--parallel 2`, `ninja -j2`, or `JOBS=2`).
- Never treat an old binary, a catalogue name, a support query, or a skipped test as proof
  that an API was implemented, executed, or correct.

## Current state (2026-10-20, third session)

`plan.md` is the authoritative backlog with per-item status and evidence; read it first. In short:

- **Denominator** (`tools/generate_api_inventory.py`): 699 public headers, 19 851 declarations; the 70 declarations
  whose signatures name internal types are classified in `audit/internal_type_classification.json` (A11).
- **Catalogue and implementation evidence:** about 2 900 claims; every claim-room pair has a reviewed record in
  `audit/implementation_evidence.json`. Every public header is either claimed or classified in
  `audit/area_classification.json` (the coverage check fails on a stale or missing rule). The whole C ABI (59 headers)
  is claimed: C17 code in the museum plus a separate C17 game probe (`cna-c-probe`) run from the cx-capi room.
- **Execution evidence:** `tools/execution_coverage.py` runs one museum process per room in a tree configured with
  `-DMUSEUM_COVERAGE=ON` and records the executed lines per room. It refuses a binary older than the sources, takes
  provenance before the run and records nothing if a source changes during it. A current run needs build-probe back
  in its coverage configuration (A08).
- **Correctness:** exact witnesses: `[museum][check]` lines (wk-*, math-checks, cx-capi, ct-storage, fx-compiled,
  tex-volshader, pl-log, pl-diagnostics, model-sampler, in-text, in-phone, app-device, ...), renderer self-checks
  (`[museum][validate]`), `tools/scenarios.py` (scenarios in `tests/scenarios/`), `tools/devices_check.sh`,
  `tools/audio_check.sh` (PCM through SDL's disk driver: tones, pan, XACT, MediaPlayer, microphone loopback),
  `tools/voice_check.sh`, `tools/net_check.sh` (migration, packets under simulated loss, session lifecycle),
  `tools/online_check.sh` (a private service, read back).
- **Profiles:** each runner records its result (`tools/record_profile.py`); `tools/profile_summary.py` shows every
  profile as current, stale or not run and calls the battery complete only when all are current and green.
- **Renderers and hardware:** `MUSEUM_GPU=1` (SDL offscreen) and `MUSEUM_GPU=weston` (private headless Weston) run on
  the machine's real GPU without the live desktop. build-probe carries every native renderer and the compiled-effects
  options. The browser edition (`tools/build_web.sh`, build-consumer) carries WEBGL2 and WEBGPU (`?renderer=`);
  `tools/web_tour.sh` tours it in headless Chrome. What needs a person or absent hardware is in
  `docs/HARDWARE_CHECKLIST.md`; how to run everything is in `docs/VERIFICATION.md`.
- **CNA defects** found by the museum are in `docs/CNA_FINDINGS.md` with the exact change each needs. CNA is owned
  by another agent: record, never edit. Entries move to "Fixed" when a museum run shows the fix.
- New wings (appended so no existing room moved): floor 7 *Devices and the System*, floor 8 *Pipeline Workshop*
  (wk-* rooms including the content build engine), rv-vertextype and rv-capabilities on floor 9.

## Meaning of the evidence levels

The audit deliberately separates four levels:

1. **Catalogue link**: a room names a claim that resolves to public declarations.
2. **Implementation**: a reviewed manifest record ties the claim and room to stable public
   declaration IDs and unique source anchors. Direct and indirect framework paths differ.
3. **Execution**: a current binary ran that reviewed path in a tracked, hashed script/log
   with the expected rooms and controls, final stats, and no exhibit/draw/control errors.
4. **Correctness**: an exact witness in a tracked run demonstrates the expected result.

Only add evidence that reaches the stated level. A panel readout that was never logged is
execution evidence at most; it is not a correctness oracle.

Key files:

- `audit/implementation_evidence.json`: reviewed source paths and stable declaration IDs.
- `audit/execution_evidence.json`: run provenance, record links, and correctness oracles.
- `audit/runs/`: immutable scripts and logs referenced by SHA-256.
- `tools/generate_api_inventory.py`: Doxygen-based complete public API inventory.
- `tools/audit_api_coverage.py`: claim resolution and evidence validator/generator.
- `build/api-audit/public-api.json`: generated full denominator, intentionally ignored.
- `build/api-audit/coverage-evidence.json`: generated complete evidence matrix, ignored.
- `docs/CNA_API_INVENTORY.md`: tracked inventory summary.
- `docs/CNA_COVERAGE_AUDIT.md`: tracked generated evidence summary.

The two tracked execution runs are:

- `2026-10-19-opengles3-reviewed`: 15 rooms, 20 frames per room, complete `pressall`,
  four settling frames, 69 controls, zero exhibit/draw/control errors.
- `2026-10-19-opengles3-tex-io-branches`: exact scripted PNG/JPEG and sized
  `Texture2D.FromStream` branches with waits and clean final stats.

Correctness currently proves:

- bit-exact PNG MemoryStream round trip;
- expected lossy JPEG MemoryStream round trip;
- exact 96x48 sized `Texture2D.FromStream` result;
- missing ResourceManager asset refusal for the requested name;
- `Game.ResetElapsedTime` produces `ElapsedGameTime 0.0 ms` after a deliberate 400 ms
  stall.

## First steps for the next agent

1. `git status`, `git log -5`, then read `plan.md` (status table and the newest entries at the end).
2. `git -C ../cna status --short; git -C ../cna log -3`: CNA moves all the time; rerun
   `python3 tools/generate_api_inventory.py --write` and `python3 tools/audit_api_coverage.py --write` when only the
   revision changed (declaration delta +0/-0).
3. Pick the first unblocked TODO/IN_PROGRESS row of `plan.md`; builds use at most two jobs and only the closed list
   of build directories (`build/`, `build-probe/` = multi-renderer + coverage tree, `build-consumer/`).
4. After new rooms: review their claims (records in the implementation manifest), add scenarios or checks with exact
   expectations, run the per-room coverage on build-probe, regenerate docs (`tools/gen_docs.sh`) and commit.

## Validation commands

Run from the repository root:

```bash
python3 -m unittest tests/test_audit_api_coverage.py
python3 tools/generate_api_inventory.py --check
python3 tools/audit_api_coverage.py --check
tools/gen_docs.sh --check
ctest --test-dir build --output-on-failure
git diff --check
```

For a source or CNA implementation change, also run with no more than two jobs:

```bash
cmake --build build --parallel 2
build/bin/museum_tests
build/bin/cna-3d-museum --validate
```

Graphics runs use a private Xvfb display with llvmpipe by default. `MUSEUM_GPU=1` in front of any tool runs
on the machine's real GPU with no display at all (SDL `offscreen`; nothing reaches the live desktop);
`MUSEUM_GPU=1 tools/renderer_check.sh OPENGLES3 OPENGL33` records the driver that drew in
`build/renderer_check/<R>-gpu.device` (CNA's adapter line cannot tell llvmpipe from the GPU). `MUSEUM_GPU=weston` instead
starts a private headless Weston (`~/deps/weston`) on the GPU for renderers that need a window surface (WEBGPU). Vulkan renderers
(WEBGPU, SDL_GPU) on the GPU: `BUILD_DIR=build-probe VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/radeon_icd.json`. Keep renderer, command, build options, revisions, binary hashes, exact script/log
hashes, room order, interactions, final stats, and error counts in the execution manifest.

## Functional checks (each prints PASS/FAIL/UNAVAILABLE lines and a summary)

```bash
tools/check_all.sh              # the battery; ONLINE=1 and SLOW=1 add profiles, skips are never ok
tools/scenarios.py              # tests/scenarios/*.json: scripted visits with exact expected log lines
tools/devices_check.sh          # manual-button rooms on a private display with fake zenity/xdg-open
tools/audio_check.sh            # audio on, SDL disk driver, FFT of the real mixed output
tools/net_check.sh              # three processes, host migration
tools/online_check.sh           # the account service (needs ../cna-gamer-services-server built)
tools/online_party_check.sh     # three accounts: invitation into a private slot, a party, sign-out
tools/build_android.sh && MUSEUM_ANDROID_SKIP=model-sampler tools/android_run.sh   # Android emulator tour (see RENDERER_TESTING)
tools/execution_coverage.py run --id <run> --build-dir build-probe --renderer OPENGLES3 --scenarios
```
