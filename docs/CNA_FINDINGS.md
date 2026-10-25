# CNA defects and divergences found by the museum

The museum does not change CNA: another maintainer owns `../cna`. Each entry here was reproduced from the museum
against the CNA revision named in `docs/CNA_API_INVENTORY.md`, with the exact change it needs. Workarounds in the museum
are named in the code where they are used and listed here; nothing is hidden behind one.

On 2026-10-25 every finding that was open was rerun against CNA `9d49828f4` (`build/`, `build-probe/` and the trees named
below, all built from `../cna`). The ones that held moved to "Fixed in CNA"; the ones CNA keeps on purpose and documents
moved to "Documented differences".

## Open

### Float16RenderTargets is reported on the Android emulator, whose half-float target reads back as NaN
- **Where:** EasyGL's capability profile (OPENGLES3) on the Android emulator's guest GLES (`-gpu guest`, Android 15 image).
- **What happens:** `GraphicsDevice` reports `GraphicsCapability.Float16RenderTargets` as supported, but in rv-capabilities
  two additive white quads drawn into a `HalfVector4` render target read back as `(0.000, NaN, 0.000, NaN)` instead of 2.0
  per channel (`tools/android_run.sh`, 2026-10-20). On the desktop OPENGLES3 renderer the same check passes.
- **Needed change:** claim the capability only when the context can render to and read back RGBA16F, or convert on readback.
- **2026-10-25:** CNA `53292f147` converts floating-point target readback on GLES. The Android tree is rebuilt against
  `9d49828f4` (`tools/build_android.sh`), but the emulator could not run: `/dev/kvm` does not exist (the `kvm_amd` module
  is not loaded on this machine since 2026-10-23). Rerun `tools/android_run.sh rv-capabilities` once it is.

## Documented differences (CNA's decision, not defects)

The museum checks each one against CNA's own documentation, so a change is still noticed.

- **XNB container version 4 loads** (wk-typewriter "XNB version byte 4"). XNA 4.0 refuses it (`BadXnbVersion`); CNA reads
  XNA 3.x's container on purpose, with the legacy `SurfaceFormat` numbering (`docs/xnb-interoperability.md`, "Version").
- **A clean leaves `.cna-content.lock`** (wk-contentproject "CleanContent.Execute: what it leaves"). XNA's CleanContent
  leaves no pipeline file; CNA keeps the lock that serializes later builds and cleans of the output root
  (`docs/content-pipeline.md`).
- **`AddDependency` keeps dependencies inside the source roots** (wk-processor): a path outside them is refused with
  `PipelineException` (XNA's type, CNA `889a31be4`), where XNA records an outside absolute path as it is
  (`ContentProcessorContext.hpp`).
- **`cna_cnb_copy_decompressed` with `CNA_CNB_COMPRESSION_NONE` ignores both declared sizes** (cx-capi): the C route copies
  the stored bytes and enforces only the destination capacity, unlike the strict C++ `DecompressCnbChunk` (`cnb.h`,
  `docs/c-api/CNB.md`, CNA `c57ecab8c`). The check pins exactly that, including the refused 5-byte destination with nothing
  written.
- **The WAV source importer keeps 8-bit PCM as `Pcm8` and narrows 24-bit, 32-bit and float to `Pcm16`** with
  round-to-nearest (wk-incremental): the header promised the opposite until CNA `acc2ee55b` corrected it to the behaviour.
- **`SystemTray.IsSupported` is the platform's tray capability, not a probe for a running StatusNotifier host**: the
  constructor reports a missing host with `PlatformException` (`SystemTray.hpp`, `docs/devices-ext.md`, CNA `d7ed7592d`).
  `tools/devices_check.sh` reports it UNAVAILABLE without a host; `tools/tray_check.sh` runs it against a private one.
- **WEBGPU needs a native window surface.** Under SDL's `offscreen` driver a named WEBGPU fails with
  `WebGPURenderer::Surface failed: missing native window`; an application that opts into fallback gets the failure in
  `GraphicsRendererSelection::GetFallbackHistory()` as `InitializationFailed` and the next renderer
  (`docs/webgpu-renderer.md`, CNA `376eb834b`). `MUSEUM_RENDERER_FALLBACK=1` (`src/Main.cpp`) is that opt-in: on
  2026-10-25 the museum logged "renderer WEBGPU was passed over (InitializationFailed) ... using OPENGLES3 instead".

## Behaviour worth knowing (not defects)
- `HttpNotificationChannel`: notifications that arrived while the channel was open but were not dispatched yet are still
  raised by `DispatchPendingNotificationsEXT` after `Close` (2 in the museum's same-frame round).
- `getCurrentGraphicsRendererCategory()` is `constexpr` and describes the build's default renderer, not the running
  device; the running one is `getGraphicsRendererCategory(device.GetGraphicsRendererType())`.
- `GraphicsCapability::CompiledEffects`: only FNA3D in the default configuration; the other renderers need their
  `CNA_<RENDERER>_COMPILED_EFFECTS` option.
- `GraphicsAdapter`: like FNA, `Description` is the display's name ("1", "screen") and `VendorId`/`DeviceId` the
  machine's PCI ids, read from sysfs, whichever driver draws: under `LIBGL_ALWAYS_SOFTWARE` (llvmpipe) the adapter still
  reports AMD 1002:15bf. XNA's `Description` named the adapter (the GPU). No public member names the driver that renders.
- `VideoPlayer` sound follows `SoundEffect.MasterVolume`: with MasterVolume 0 a playing video is silent although its own
  `Volume` is 1 (measured through SDL's disk audio driver). XNA's managed VideoPlayer hands `Volume` straight to its native
  decoder and never reads MasterVolume; whether XNA's native mixer applied it cannot be read from the managed code, so this
  is recorded as a difference to confirm, not as a defect.
- `MeshHelper.CalculateNormals` sums the normalised normal of every triangle at a position (a quad split into two
  triangles counts twice at the corners its diagonal touches), exactly like XNA.
- FNA3D refuses a `PbrEffect` draw whose vertices carry no tangent (`VertexPositionNormalTexture`) with
  `NotSupportedException` (`FNA3D-MISSING-STOCK-INPUT`) wherever FNA3D picks its SDL_GPU driver (the GPU here): a missing
  input cannot be built into that driver's pipeline. Its OpenGL driver (llvmpipe) and every other renderer supply a neutral
  tangent. rv-shading reports it as INFO; CNA lists it in its `handoff.md` §5.2.
- `MessageBox.Show` marks the first button the Return default and the last the Escape default. SDL shows the box through
  Zenity when Zenity is installed, and Zenity ignores both hints (`docs/devices-ext.md`); `tools/dialogs_check.sh` therefore
  clicks the Zenity buttons and checks the keys in a second museum without Zenity on its PATH, which gets SDL's own X11 box.
- `StorageDevice.EndOpenContainer`, as XNA's, marks the result ended before it compares the device: a result refused for
  the wrong device is spent, and a second End on it is `InvalidOperationException` (XNA IL: `endHasBeenCalled = true`
  precedes the device check). ct-storage gives that check its own result.
- Where no per-user data folder can be created (XDG_DATA_HOME below a regular file), `IsConnected` is false and
  `EndOpenContainer` throws `IOException` from creating the container's folder chain, as XNA's `DirectoryInfo.Create` does
  (CNA `c30925496`, measured against genuine XNA under Wine); `SetAppNameEXT` itself resolves nothing.

## Fixed in CNA after the museum reported them

Verified on 2026-10-25 against CNA `9d49828f4` (the scenarios named are in `tests/scenarios/`; `tools/scenarios.py` 28 passed,
0 failed, 1 skipped in `build/`, the skip being `compiled_effects`, which runs in `build-probe`):

- **BoundingFrustum.Intersects(Ray) clipped no ray from outside the frustum** (CNA `9a5e5d896`): math-checks "Ray from
  outside" gets XNA's 4.5, all 169 math checks pass (scenario `math_checks`).
- **SDL_GPU sampled no volume through ShaderEffect, and one layout crashed** (CNA `830e9a934`, documented in
  `docs/sdlgpu-shader-effect-layout.md`): tex-volshader now runs a GLSL 450 source in that native layout on SDL_GPU (RADV)
  and samples slice 20 with 0 of 1024 pixels off (scenario `volume_shader` with `CNA_GRAPHICS_RENDERER=SDL_GPU`); the room no
  longer leaves SDL_GPU out. WGSL on WEBGPU and GLSL ES on OPENGLES3 pass as before.
- **Vertex texture fetch (HiDef) was missing on WEBGPU and SDL_GPU** (CNA `079dfc55e`, `e8bfdf2c5`, `c105303dc`):
  fx-compiled's vs_3_0 `vertextexture` draws its graph and PointClamp/PointWrap change its shape on SDL_GPU (RADV) and on
  WEBGPU (GPU through Weston), as on OPENGLES3 and FNA3D (scenario `compiled_effects` per renderer).
- **HapticDevice's inline default constructor did not compile outside CNA** (CNA `974c1ac08`, CNA-DEMOS-ISSUE-009): the
  devices room now holds a default-constructed `HapticDevice` member (the `std::optional` workaround is gone);
  `tools/haptics_check.sh` 14 of 14.
- **VertexTypeRegistryEXT named a refused type in its mangled form** (CNA `a80534927`): rv-vertextype "the refusal names
  the type as the source spells it" finds `'(anonymous namespace)::UnregisteredVertex'` (scenario `vertextype`).
- **MessageBox.Show marked no default button** (CNA `aff773b3b`): in SDL's X11 message box Return chooses the first button and
  Escape the last (`tools/dialogs_check.sh` 9 of 9; Zenity ignores both hints, see above).
- **EnvironmentMapEffect reflected no texture parameter** (CNA `28145a201`): be-model's new check finds
  `Parameters["Texture"]` (Texture2D) and `Parameters["EnvironmentMap"]` (TextureCube), and the properties and the parameters
  read what the other side set (scenario `effect_parameters`).
- **No capability query for reading a render target back** (CNA `e357bc4c4`, `RendererFeature::RenderTargetReadback`):
  rv-capabilities holds the answer to a real readback (Supported and read back on OPENGLES3, SDL_GPU, WEBGPU and FNA3D;
  Unsupported and refused on HEADLESS), and the sign factory asks the feature instead of catching the refusal.
- **StorageDevice and StorageContainer skipped XNA's argument, state and disposal checks** (CNA `3798787bb`, `17c66a9d3`,
  `ac6be8f03`): every ct-storage contract check passes (scenario `storage_contracts`).
- **StorageDevice.SetAppNameEXT threw when no storage root could be created** (CNA `fffa552ad`): it resolves nothing now;
  `main` no longer guards it (scenario `storage_not_connected`).
- **Content pipeline divergences found by the Pipeline Workshop**, each a FAIL check before:
  1. nested build names collided (CNA `98aa655f2`, `419723275`): `Convert`'s material and `BuildAsset` give `plaque_0` and
     `plaque_1` (scenario `workshop_scanner`, wk-processor);
  2. the "TextureProcessor" alias produced a CNA runtime type (CNA `671d8478e`): `BuildAndLoadAsset<TextureContent,
     TextureContent>` gives a Texture2DContent;
  3. `Convert`'s errors (CNA `889a31be4`): ArgumentNullException for an empty name, InvalidOperationException for an unknown
     processor;
  4. `AddDependency` threw `std::invalid_argument` (CNA `889a31be4`): now `PipelineException`; the source-root rule itself is a
     documented difference (above);
  5. MaterialProcessor with an EffectMaterialContent without an effect (CNA `889a31be4`): InvalidContentException in both
     cases (scenario `workshop_material`);
  6. `ReadRawObject<Int32>` (CNA `252e9bca1`) and 7. a truncated XNB body (CNA `583af2a5e`): wk-typewriter reads -42,
     123456789 and wraps the cut body in ContentLoadException (scenario `workshop_reflective`);
  9. XmlSerializer had no member list for Color and Rectangle (CNA `bbed8c61e`, Sharp Runtime `f990d074`): both serialize in
     XNA's own text (`<R>10</R><G>20</G><B>30</B><A>40</A><PackedValue>673059850</PackedValue>`, and Rectangle's fields then
     `<Location>`) and read back (scenario `workshop_xml`);
  10. `NormalizeWeights` kept a zero weight (CNA `889a31be4`): B .625, A .25, D .125 (scenario `workshop_animation`);
  11. `PipelineComponentScanner.Update` answered false for a changed list (CNA `889a31be4`) (scenario `workshop_scanner`);
  12. two `ContentTypeSerializerAttribute` instances were not equal (CNA `889a31be4`) (scenario
      `workshop_serializer_attribute`);
  13. a glTF with embedded images could not be loaded after the default build (CNA `cd1f5ef0a`, CNA-DEMOS-ISSUE-022: the
      extracted textures are always child outputs) (scenario `workshop_routes`);
  15. `TaskItem::operator==` was implicitly deleted (CNA `48a5c75eb`): wk-tasks now calls it (scenario `workshop_tasks`);
  16. the content tasks need `cna_content_compiler` (CNA `626de40e6` documents it; the museum links it);
  14 and 8, 17: see "Documented differences".
- **Native C API** (cx-capi, scenario `capi_native`: the in-museum client 65 passed, 0 failed; the game probe 63 passed,
  0 failed):
  - one process carried two CNA states (CNA `89fb1493f`): `cna_assembly_copy_title_ext` reads "CNA 3D Museum";
  - `MouseState.ToString` and `GamePadState.ToString` (CNA `0ef875619`): `{X:10 Y:20 Buttons:Left Wheel:120}` and
    `{IsConnected:True}`;
  - `cna_achievement_collection_get_by_key` with an unknown key (CNA `4a756413f`): `CNA_RESULT_INVALID_ARGUMENT`. The same
    commit made the collection read-only, as XNA's is; the probe now builds it with both achievements and checks that
    `cna_achievement_collection_add` answers `CNA_RESULT_NOT_SUPPORTED`;
  - `cna_presentation_parameters_init` (CNA `e460e0ed6`): XNA's 0x0 full-screen defaults;
  - `devices.h` now says desktops and browsers report `EMULATOR` (CNA `acc2ee55b`); the NONE codec is a documented
    difference (above).
- **HttpNotificationChannel failed silently where no socket can listen** (CNA `3c61c30ef`, `3078bb41e`): in the browser
  build `Open` publishes no URI and the next dispatch raises `ErrorOccurred` with `ChannelOpenFailed` ("listener setup
  failed: Not supported"), and a send to an address nobody listens on is refused; natively the channel opens, delivers and
  refuses as before (scenario `notifications`, native and `tools/web_scenarios.py`, whose run is 24 passed, 0 failed,
  5 skipped for build switches the browser build lacks).
- **A profile read just after a Guide sign-out reached the service without a token (seen once)** (CNA `3cd6028b7`,
  `ca8633622`): `tools/online_party_check.sh` 19 of 19 three times, none of the three museums logging the old message.
- **FNA3D's SDL_GPU (Vulkan) backend crashed once binding fragment samplers** (about one run in four before CNA
  `c2664bad6`): `MUSEUM_GPU=1 BUILD_DIR=build-probe tools/renderer_check.sh FNA3D` on RADV passed 12 times in a row
  (81 PASS, 0 FAIL each); at one crash in four, twelve clean runs had a 3 % chance.
- **Vendored ENet shifted a signed 1 by 31 places (UBSan)** (CNA `5b6aab981`, MUSEUM-NET-003, and `03b92269c` for the
  fragment bitmaps) and **three data races ThreadSanitizer found on 2026-10-20** (CNA `20fe6efd2`: `HttpNotificationChannel::Close`
  against its listener thread; `844e3ce41`: `SetMixerTrackMixCallback` against the mixer thread; `594d7d328`: the exit-time
  drain of pending pan states while the audio thread still mixed): rerun in build-tsan, one tree built with
  `-fsanitize=thread,undefined` (ENet and the museum included), with the 2026-10-20 steps: the `notifications` and
  `inspector` scenarios, a 30-frame press-all tour of the au-*, dev-notify, pl-inspector, net-packets and net-session rooms,
  `tools/net_check.sh` (23 of 23) and `tools/audio_check.sh`. No ThreadSanitizer report from CNA or the museum and no UBSan
  report at all. The only TSan reports, 19 in the first audio_check run, were UBSan's own runtime probing memory with
  `pipe()` (`__sanitizer::IsAccessibleMemoryRange`), the two runtimes' interplay; `tools/tsan.supp` now suppresses calls
  from libubsan, and the rerun has no report. Its one oracle failure (panright's channel ratio 6 for 10) is timing: the
  chirp is entirely in the right channel, but under TSan's slowdown a centered room sound that natively follows 2.6 s later
  overlaps its tail. The sanitizer tree was removed afterwards.
- **D3D9: a failed full-screen Reset was followed by a fatal GetRenderTarget error** (seen under Wine; CNA `d0cea7036`): the
  MinGW build under Wine 10 with `--fullscreen` on a 1280x720 Xvfb, where the 800x480 mode is unavailable, now logs
  "D3D9 Reset (resize to 800x480) failed, hr=0x8876086C -- device lost, awaiting reset recovery" and runs on to its final
  stats with exit 0. The sign factory now leaves signs plain while the device is lost (XNA refuses every draw then with
  DeviceLostException, and so does CNA); before that change the run logged 53 sign errors. `tools/wine_tour.sh -p` on
  DIRECTX9 tours all 320 rooms. Wine results, not Windows.

Earlier:

- **PbrEffect differed between renderers** (WEBGPU unlit, SDL_GPU refused tangent-free vertices; CNA `91612a0a3`,
  `bb6a2f44e`): at `a6f9e9b2e` rv-shading passes on every renderer - WEBGPU 83/0 natively and on the GPU, SDL_GPU 104/0,
  the browser WebGPU tour 320/320 with 82/0 self-checks (2026-10-22).
- **Haptics.OpenEXT could not open an id that GetHapticsEXT listed** (the haptic subsystem was quit between calls and
  SDL renumbered the devices; CNA `272696e8b` + `a6f9e9b2e`, which also fixed `272696e8b`'s failing SDL budget audit):
  `tools/haptics_check.sh` against a virtual /dev/uinput force-feedback device passes 14 of 14 at `a6f9e9b2e` - listed and
  opened, rumble as a sine periodic effect, constant effect uploaded at level 12000, re-uploaded at 16000, played, stopped and
  erased, gain 32767 for SetGainEXT(50), autocenter 0, clean exit (2026-10-22).
- **`DEF_PROP` in ContentManager's private section made its loading helpers public** (CNA `b0b0207e6`): at `a6f9e9b2e` the
  inventory no longer lists `ReadXnbHeader` and the other helpers (19 849 declarations, 9 fewer), and its internal-type
  classification is removed (2026-10-22).
- **Browser WebGPU ended the program on `rgba16unorm`** (no `texture-formats-tier1`; CNA `b6fc8e441`, MUSEUM-WEBGPU-001) and
  **reported "Destroyed texture ... used in a submit"** (canvas backbuffer across browser waits; CNA `31a3d54b8`,
  MUSEUM-WEBGPU-002): at CNA `5b6aab981` the browser WebGPU tour visits all 320 rooms (no rooms left out) with 0
  destroyed-texture and 0 uncaptured errors, exit 0; `app-device` queries the capability lists in the browser again (the
  museum's workaround is removed). The tour's one FAIL is the WEBGPU PbrEffect finding, as natively (2026-10-22).
- **Host migration picked a gamer of the machine that just left** (`AttemptHostMigration`, CNA `cd9868526`,
  MUSEUM-NET-001): with two local gamers on the departing host, `tools/net_check.sh` now sees `HostChanged: Guest ->
  Visitor` and B keeps its session (2026-10-22).
- **A gamer joining a game in progress stayed in the lobby** (`ServerWelcomeMessage` without the session state, CNA
  `04fb4978a`, MUSEUM-NET-002): `tools/net_check.sh` round lifecycle passes, 23 passed, 0 failed (2026-10-22, at `93baa9363`).
- **A tray host reading the menu made the game abort** (dangling entry labels in `Sdl3Tray`, CNA `93baa9363`,
  MUSEUM-TRAY-001): `tools/tray_check.sh` reads the menu, clicks "Say hello", toggles and removes the icon and reads the
  tooltip; the museum exits 0 (13 passed, 0 failed, twice, 2026-10-22).
- **A missing content asset left no canonical exception type (C API).** After `cna_content_manager_load_texture2d` of a missing asset, `cna_error_copy_last_exception_type_ext` now reports `Microsoft.Xna.Framework.Content.ContentLoadException`: the cx-capi game probe's check passes and the probe reports 63 checks, 1 failed (was 2), in the final battery on 2026-10-20.
- StorageContainer members after `Dispose` now throw `ObjectDisposedException` as in XNA (CNA `5865ae938`); verified by
  `tests/scenarios/storage_contracts.json` (FileExists, CreateFile and GetFileNames after Dispose: PASS).
See the "CNA findings fixed in CNA" list in the project memory and `plan.md`; entries move here when their fix is
verified by a museum run.

### Seen once, not reproduced (closed)

- **A heap corruption crash in gs-avatar under OPENGLES3** (once, at CNA `a6f9e9b2e`, in the execution coverage run
  `2026-10-22-opengles3-full-a6f9`: SIGSEGV inside `malloc` in Mesa's libgallium, called from
  `EasyGLRenderer::DrawIndexedPrimitivesEx` under the museum's `StaticMesh::Draw`). Not reproduced in 13 runs then (8 with
  `glibc.malloc.check=3`) nor in 40 press-all runs of the room with `glibc.malloc.check=3` at `9d49828f4` (build-probe,
  OPENGLES3 on llvmpipe, 2026-10-25). While looking for it CNA fixed an avatar-load lifetime defect (a mutex freed before
  its lock was released, CNA `2b39eb934`); whether that was the cause cannot be shown. If it returns, run the room in
  build-asan.
