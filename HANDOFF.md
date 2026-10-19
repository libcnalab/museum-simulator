# Museum Simulator CNA Coverage Handoff

Updated: 2026-10-19 (Europe/Prague)

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

The persistent goal was deliberately paused after the handoff commit at the user's
request. A future agent should resume only when the user asks it to continue.

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

## Current authoritative audit state

The generated public API inventory currently observes CNA HEAD
`df92342022eb7d80e2a041a1dad8cde43dc5e560` described as
`v0.1.0-465-gdf9234202-dirty`.

At handoff time the sibling CNA tree has concurrent documentation changes in:

- `CNA_DEMOS_REPAIR_HANDOFF.md`
- `CNA_DEMOS_REPAIR_MATRIX.json`

The other agent committed the Quaternion implementation and tests as `df9234202`
(`Preserve finite range in Quaternion inverse and division`). Neither that commit nor the
remaining documentation changes alter a public header. The public-header SHA-256 remains
`16f94e940a7d898f302fbdd293f75b904cb4741549fce2f14c8f8ca99849ea69`, with a
declaration delta of +0/-0. Do not race or overwrite that work.

Inventory and catalogue counts:

- 699 public headers;
- 19,851 public declarations: 12,941 C++ and 6,910 C;
- 225 internal/detail headers excluded;
- 40 internal/detail namespace declarations excluded;
- 70 public declarations whose signatures expose an internal/detail type;
- 295 museum rooms and 1,284 normalized claims;
- 1,813 claim-room pairs;
- 1,921 declarations linked by at least one catalogue claim;
- zero ambiguous, unresolved, or non-declaration claims;
- seven `System.*` dependency claims kept outside the CNA denominator.

Evidence counts after completing `rt-formats`:

- 99 reviewed implementation records;
- 81 implementation-verified CNA declarations;
- 69 fully implementation-verified claims;
- 97 reviewed and 89 fully verified claim-room pairs;
- two current recorded OPENGLES3 runs;
- 81 executed CNA declarations, 69 fully executed claims, and 89 executed pairs;
- five exact correctness oracles;
- eight correctness-verified declarations, six fully verified claims, and seven pairs.

Rooms fully reviewed at implementation and execution level:

- `pl-diagnostics`: 26/26
- `ct-compiler`: 14/14
- `pl-exceptions`: 13 verified plus the exact parameterless `SpriteBatch.Begin`
  overload; the family remains partial because the room does not call seven other overloads
- `pf-dashboard`: 7/7
- `app-gametime`: 5/5
- `ct-resource`: 5/5
- `cx-stats`: 4/4
- `rt-formats`: 3/3

Other reviewed partial rooms are shown in `docs/CNA_COVERAGE_AUDIT.md`. The most useful
next contained review is `model-cameras`: its room is already present in the tracked
OPENGLES3 run, but only `ModelCameraEXT.HasInfiniteFarPlane` has a reviewed record. Review
the remaining seven claims against `src/Exhibits/Models/ModelCamerasLab.cpp`, add precise
records with stable symbol IDs, and link only paths actually executed by the run.

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

## Binary provenance caveat

The last fully built and runtime-tested binary baseline predates the current concurrent
Quaternion implementation change in CNA:

- built CNA revision: `2a245b91f2e6388863cc81f5c93880494dad664a`;
- museum binary SHA-256:
  `22fe215c0883526f409e02f8c535a650b3bd629779f08cc69a8e187aedc7bfd3`;
- `libcna.so` SHA-256:
  `c7c14ed576f439e609978aa8de3f79921b3594d6276250ad693e222b5bd5ce71`;
- `museum_tests`: 8,484 checks, zero failures;
- `--validate`: passed;
- CTest: 6/6 passed;
- renderer self-check: 85 PASS, 0 FAIL, 1 INFO, 1 SKIP.

The evidence tool still regards both recorded runs as current for those unchanged binaries,
but task A07 is correctly `IN_PROGRESS`: current CNA source has not been rebuilt after the
other agent's Quaternion changes. Once that agent has stabilized or committed the tree,
rebuild with at most two jobs, run tests and validation, record new binary hashes, and repeat
any execution runs whose CNA binary hash changes. Never edit new hashes into a manifest
without rerunning the corresponding script.

## Work completed in the final audit pass

- `ct-compiler`: all 14 claims now have reviewed direct/indirect paths and execution
  evidence. This covers compiler/options, platform/profile/compression fields,
  importer/processor/content types, type-writer lookup, and the protected
  `ReadAsset`/`OpenStream` route through `MemoryContentManager`.
- `app-gametime`: all five claims now have reviewed implementation and execution evidence;
  the existing exact log line is a correctness oracle for reset-after-stall.
- `rt-formats`: all three room claims now have implementation and execution evidence:
  `SurfaceFormat`, `SupportsSurfaceFormatAsRenderTargetEXT`, and `Texture.Format`.
  Results are only shown in the panel, so no correctness oracle was fabricated.

The current museum source also contains earlier audit fixes and tooling additions described
in `plan.md`, including removal of direct `CNA::Internal` names, correction of false
catalogue claims, truthful none/LZX compiler compression controls, deterministic inventory
drift gates, execution provenance validation, and generated documentation gates.

## First steps for the next agent

1. Confirm a clean museum checkout and read `plan.md` plus the two generated summaries.
2. Inspect `git -C ../cna status --short` and `git -C ../cna rev-parse HEAD`. Do not touch
   it while another agent owns dirty files.
3. Run the nonmutating checks below. If only the CNA revision/diff changed with no public
   declaration delta, regenerate inventory and coverage summaries, then record provenance.
4. Continue A03/A05 with the remaining seven `model-cameras` claims. Use the current public
   inventory to obtain stable IDs and verify unique anchors with the coverage generator.
5. Link records to an execution run only when the run actually enters the room and executes
   the required branch. Add correctness only for exact, logged outcomes.
6. After sibling CNA work stabilizes, complete A07 before claiming the runtime baseline is
   current.

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

GPU/private-display runs may require the same elevated execution context used by the prior
session. Keep renderer, command, build options, revisions, binary hashes, exact script/log
hashes, room order, interactions, final stats, and error counts in the execution manifest.

## Known immediate limitation

At handoff, unit tests, coverage audit, inventory, generated docs, CTest, museum structural
tests, `--validate`, and whitespace checks passed. The inventory initially reported stale
because CNA advanced concurrently; it was regenerated against `df9234202`, with +0/-0
declaration delta and the same public-header hash. A07 remains open until the new CNA
Quaternion implementation is built and runtime evidence is refreshed.
