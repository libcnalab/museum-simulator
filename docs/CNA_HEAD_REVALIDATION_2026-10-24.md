# CNA HEAD revalidation — 2026-10-24

This note records the continuation of the museum handoff after CNA published the repair
campaign checkpoint. The museum's historical findings remain in `CNA_FINDINGS.md`; this
document separates source-level CNA HEAD dispositions from museum runs made with the older
`a6f9e9b2e` consumer build.

## Museum evidence available here

Using the existing museum build and `MUSEUM_GPU=1` (private off-screen GPU), the targeted
scenarios produced:

| Result | Scenarios |
|---|---|
| PASS | `capi_native`, all eight `workshop_*` scenarios, `vertextype`, `volume_shader`, `storage_contracts` |
| Pending newer CNA rebuild | `notifications` |

The targeted run was against the museum's then-existing CNA `a6f9e9b2e` binary. It is evidence
for the listed museum paths only; it is not a claim about the later CNA HEAD. The native and
web museum builds have since been rebuilt against CNA `ba707082e`; the targeted scenarios have
not yet been repeated against that newer binary.

## CNA HEAD disposition

Reading CNA `ba707082e` and `CNA_MUSEUM_REPAIR_HANDOFF.md` shows that the published fixes cover
the previously open museum reports for input-state text, ENet fragment masks, vertex-type
diagnostics, the Pipeline Workshop divergences, render-target readback capability reporting,
XNA presentation-parameter defaults, TextureProcessor's XNA-typed result, and the XML serializer
Color/Rectangle/Point members. CNA also records fixes for notification close/error handling,
storage validation, EnvironmentMapEffect parameters, the C API state split, achievement lookup,
FNA3D sampler lifetime, and the signed-out profile race.

The remaining items are not all ordinary defects that can be closed by waiting:

- D3D9 failed-reset behavior still needs a real Windows validation; the available Wine run is
  retained as Wine evidence only.
- Android half-float render-target behavior needs an Android-device/emulator rerun.
- The FNA3D GPU crash and the one-time `gs-avatar` heap corruption need repeated native/GPU or
  sanitizer evidence; one clean rerun is insufficient.
- UBSan/TSan findings need fresh sanitizer-tree reruns after the published fixes.
- The three CNA campaign rows 003, 101 and 152 still require native Windows/XNA validation.

Therefore the honest status is: all published CNA source fixes have been identified, the
consumer has revalidated the targeted workshop/storage/C API paths, and no realizable museum
backlog row remains. The remaining open or pending rows are explicitly external, intermittent,
or awaiting a rebuild/sanitizer/platform run.
