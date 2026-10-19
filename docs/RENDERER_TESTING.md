# Renderer testing

The museum contains no renderer-specific code. It asks CNA what the active renderer supports
(`GraphicsDevice.SupportsCapability`, `GetRendererCapabilityProfileEXT`) and says so in the room when a feature is
missing. A multi-renderer build lets the same binary run on several renderers:

```bash
MUSEUM_RENDERERS="OPENGLES3;OPENGL33;SOFTWARE" tools/build.sh     # first entry is the default
CNA_GRAPHICS_RENDERER=OPENGL33 build/bin/cna-3d-museum
```

Seven visual renderers have run the museum here: OPENGLES3, OPENGL33, SOFTWARE, FNA3D, WEBGPU, SDL_GPU and WEBGL2
(WEBGPU and SDL_GPU on Mesa's llvmpipe software Vulkan driver, `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`).
WEBGL2 runs as an Emscripten browser build (below). Direct3D 9 runs only through Wine; Metal remains untested. Two non-visual renderers were tried for robustness: **STUB**
(no 3D pipeline: the museum says so and exits 0 before executing a tour; `tools/tour.sh` therefore returns 4 for the missing final stats) and **HEADLESS** (all 293 rooms run cleanly; plain cube maps retain caller-authored CPU data while rendered pixels remain unavailable).
VULKAN was retired from CNA on 2026-10-13 and has been removed from the museum's documentation and build lines.

**Physical GPU (2026-10-20).** `MUSEUM_GPU=1` in front of any tool runs the museum with SDL's `offscreen` video driver:
no display server, no window anywhere, EGL on the GPU's render node and Vulkan through `VK_EXT_headless_surface`. On the
AMD Phoenix GPU here (PCI 1002:15bf, Mesa 25.0.7 radeonsi, as Mesa's EGL log in `build/renderer_check/<R>-gpu.device`
shows): `tools/renderer_check.sh OPENGLES3 OPENGL33` 97 PASS, 0 FAIL, 2 INFO, 1 SKIP on each (the llvmpipe set),
`tools/tour.sh -f 3 -p` on OPENGLES3 316 rooms, 0 exhibit errors, 0 script errors in 36 s (181 s on llvmpipe), and
`tools/reference.sh compare` within 1 % of the llvmpipe references for every view (worst `formats` 0.59 %).
From `build-probe` with `VK_ICD_FILENAMES=.../radeon_icd.json`: SDL_GPU on RADV 97/0/2/1, FNA3D on radeonsi 75 PASS,
0 FAIL, 2 INFO, 23 SKIP (formats FNA3D cannot create). WEBGPU does not start offscreen (its surface needs a native
window; `docs/CNA_FINDINGS.md`), so WEBGPU on the GPU needs a real display or a GPU-backed compositor.

The second group needs CNA's optional dependencies from the shared checkouts (nothing is downloaded):

```bash
BUILD_DIR=build-probe MUSEUM_RENDERERS="OPENGLES3;FNA3D;WEBGPU;SDL_GPU" \
  MUSEUM_CMAKE_ARGS="-DFETCHCONTENT_SOURCE_DIR_FNA3D=$HOME/deps/FNA3D -DFETCHCONTENT_UPDATES_DISCONNECTED=ON \
                     -DCNA_WEBGPU_ROOT=$HOME/deps/wgpu-native-v29.0.1.1 -DCNA_WEBGPU_AUTO_DOWNLOAD=OFF" tools/build.sh
CNA_GRAPHICS_RENDERER=WEBGPU VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json BUILD_DIR=build-probe tools/tour.sh -f 3 -p
```

The browser build is a separate Emscripten cross-compilation tree and emits an HTML loader, JavaScript, WebAssembly
and a preloaded data file:

```bash
tools/build_web.sh
python3 -m http.server 8000 --directory build-consumer/bin
# open http://127.0.0.1:8000/cna-3d-museum.html
tools/web_smoke.sh
```

The 2026-10-16 Chrome/SwiftShader smoke created an OpenGL ES 3.0 / WebGL 2.0 context, rendered 122 frames of the
lobby at 50 FPS, reported 28,526 triangles, 66 textures and 0 exhibit errors, then exited cleanly. This is a startup
and rendering check, not yet a complete all-room browser tour or the floor 9 validation matrix. The smoke also walks
continuously for six simulated seconds and requires progress persistence to occur only after a subsequent five-second
idle period; the verified autosave occurred at frame 112 after 5.1 idle seconds.

Browser-generated signs stay in their mipmapped render targets instead of synchronously crossing from WebGL to
Wasm for CPU mip generation and then being uploaded again. On the same 20-second Chrome/SwiftShader walking trace,
this reduced the largest lazy-room-build frame from 3,949.9 ms to 333.3 ms, only modestly above that software
renderer run's ordinary 100-300 ms frames. Individual room builds fell from 600-1,200 ms to 134-238 ms.

## The tools

| Tool | What it does |
|---|---|
| `tools/renderer_check.sh [R...]` | runs the floor 9 *Renderer Validation* rooms on each renderer; prints PASS/FAIL counts and leaves every check in `build/renderer_check/<R>.checks` |
| `tools/tour.sh` | walks into every room and runs a few frames: catches exceptions and unsupported-call failures |
| `tools/room_diff.sh RENDERER [-b BASE]` | the arrival view of every room on two renderers, ranked by differing pixels: the quickest way to find a renderer divergence among all 257 rooms (below) |
| `tools/reference.sh capture\|compare` | stored images for 30 named views (OPENGLES3 only) (`tests/reference/<R>`) |
| `tools/roomshots.sh`, `tools/shot.sh` | contact sheets and single screenshots for comparing renderers by eye |
| `tools/web_smoke.sh` | serves and runs the Emscripten bundle in Chrome, requiring a WebGL 2 context, idle-deferred autosave, final statistics and zero exhibit errors |

## The Renderer Validation rooms (floor 9)

Each room draws deterministic images into a 64 x 64 render target through stock CNA/XNA calls, reads the pixels back
with `GetData` and compares them with values worked out by hand: no tolerance beyond one or two levels of 255 except
where filtering makes an exact value meaningless. Rooms: geometry and coverage, SpriteBatch pixels, blending, depth,
interpolation/lighting/fog, sampling, render targets, camera matrices, stock effects. The *Depth* room also reports (as
information, not pass/fail) the smallest depth gap the renderer resolves at 100 m.

Result refreshed on 2026-10-16 after CNA `83c7b324c`
(CNA at the commit shown by `git -C ../cna log -1`; the SOFTWARE renderer is flagged Experimental by CNA):

| Renderer | Checks | Pass | Fail | Info | Skip |
|---|---|---|---|---|---|
| OPENGLES3 | 87 | 85 | 0 | 1 | 1 (compiled XNA effects unavailable) |
| OPENGL33 | 87 | 85 | 0 | 1 | 1 (compiled XNA effects unavailable) |
| SOFTWARE | 87 | 85 | 0 | 1 | 1 (compiled XNA effects unavailable) |
| FNA3D | 87 | 63 | 0 | 1 | 23 (unsupported target/surface-format cases) |
| WEBGPU | 87 | 64 | 0 | 1 | 22 (surface-format cases plus compiled XNA effects) |
| SDL_GPU | 87 | 85 | 0 | 1 | 1 (compiled XNA effects unavailable) |
| HEADLESS | 28 | 1 | 0 | 0 | 27 (rendered-pixel checks are unavailable without a rasterizer) |

The "Surface Formats" room (`rv-formats`) writes 16 values through every uncompressed `SurfaceFormat`, reads them back bit for bit,
and draws ten of the unsigned formats through SpriteBatch to check what the GPU samples. All of it passes on the three OpenGL-family
and SDL_GPU renderers; FNA3D can create only `Color` (plus DXT), WEBGPU only `Color`, `HalfVector4` and the 32-bit float formats.
One expectation worth knowing: for the channels a format does not store, Direct3D 9, XNA and every CNA renderer sample **1**
(`R16F` gives (r, 1, 1), `G16R16F` gives (r, g, 1)), while `PackedVector`'s own `ToVector4()` says 0.

The separate `tex-volume` room now round-trips Color, Single and Vector4 on all six visual Linux renderers: OPENGLES3, OPENGL33, SOFTWARE, FNA3D, WEBGPU and SDL_GPU. CNA `b23c9bd19` connected FNA3D's typed `Texture3D` route to its existing format-native volume storage and added the volume classifier. CNA `fd2b299c0` gave WebGPU native R32Float/RGBA32Float volume storage and format-sized transfers instead of hard-coded RGBA8. CNA `d410638c6` did the equivalent for SDL_GPU with a live native-format probe; its focused executable passed 36/36, including exact Single/Vector4 round-trips, under Xvfb/llvmpipe.

Run `tools/renderer_check.sh` for the current figures; they are not copied anywhere else.

## Whole-museum results

| Renderer | Tour of every room | Screenshot vs OPENGLES3 (lobby) |
|---|---|---|
| OPENGLES3 | clean, 0 exhibit errors (166 s after the compiled-instancing follow-up) | - |
| OPENGL33 | clean | 0.00 % of pixels differ by more than 15/255 |
| SOFTWARE | clean (about 25 minutes for the whole museum under Xvfb) | 4.6 % |
| FNA3D | clean, 0 exhibit errors (134 s after the compiled-instancing follow-up) | Fresh 295-view result: 26 differ from OPENGLES3 by more than 2 %; the added `pf-dashboard` difference is live timing data, and no model/PBR/IBL room remains |
| WEBGPU | clean, 0 exhibit errors (264 s) | 30 reference views: 26 within 1 %; `debugdraw` 4.7 %, `matrices` 1.6 %, `deck` 1.1 %, `billboards` 1.0 % |
| SDL_GPU | clean, 0 exhibit errors (451 s) | 30 reference views: all within 1 % |
| HEADLESS | clean, 0 exhibit errors (45 s) | no rasterized pixels by design |

Windows build under Wine (MinGW cross build, 2026-10-15, 286 rooms): SOFTWARE `pressall` tour exits 0 with 0 exhibit errors in 1730 s (networking, avatars and video are not built into that tree, so 915 panel controls instead of 998).

Latest (2026-10-16, 295 rooms): after CNA `c8145f60c`, the FNA3D press-all tour and a focused press-all pass over the rigid/skinned PBR, material, variant, IBL and shadow rooms exit 0 with 0 exhibit errors. Follow-up CNA `bafde1fff` closes the three older contracts that the full lane exposed (GraphicsAdapter/MSAA reporting, default depth and the CNA-extension SkinnedEffect vertex-colour route). CNA `ff6f18c08` then adds split-sum IBL to the rigid/skinned compiled PBR effects; its 8/8 pixel oracle, 34/34 compiled-effect tests and source audits pass. CNA `4d3cc6892` additionally flips the complete tangent basis for double-sided back faces, including mirrored world transforms; its shared FNA3D/OpenGL and OPENGLES3 oracle passes 10/10 and two targeted mutations fail the expected ordinary or mirrored cases. CNA `e44369bbe` adds independent UV0/UV1 selection for every rigid/skinned PBR material map; the shared material-map oracle passes 21/21 on FNA3D/OpenGL and OPENGLES3, while its mutation fails exactly the two UV1 discriminators. CNA `83c7b324c` completes single/cascaded directional, point-cube and spot shadow reception across Basic, Skinned, PBR and SkinnedPBR; the shared OpenGL pixel oracle passes 9/9, the Basic/Skinned punctual suites remain 5/5 each, and SOFTWARE's shared shadow oracle still passes. DirectX9's 14 checked shader blobs remain byte-for-byte reproducible with the FNA-only path disabled. The complete 50-test FNA3D label has 48 passes, two documented custom-source-effect skips and 0 failures; all 34 compiled-effect tests pass. The rebuilt museum passes 8,482/8,482 structural checks, and `cx-shadow` press-all reaches final stats with 0 exhibit errors; an inspected capture shows its live depth map and cast shadows. The complete 295-room FNA3D press-all tour reaches final stats in 101 s with 0 exhibit errors and 0 draw errors. The fixed `cx-shadow` arrival view still differs from OPENGLES3 by 4.01%, so it remains in the visual-difference list. The earlier targeted museum diff moves `model-ibl` from 18.46% to 0.000434% and `model-variants` from 13.54% to 0%, both below the 2% list threshold. The validation matrix remains 62 pass, 0 fail, 1 info and 23 skip. After CNA `d73f1aeab`, the HEADLESS tour reaches final stats in 23 s with the same clean result. The preceding multi-renderer run exited 0 with 0 exhibit errors on WEBGPU (125 s) and SDL_GPU (307 s). STUB has no 3D pipeline and is reported as an incomplete tour (exit 4) rather than a pass. The OPENGLES3 nine-step `tools/check_all.sh` battery is green. ASan+UBSan on OPENGLES3 over all 293 rooms exits 0 with no report. The leak harness now accepts multiple renderer names and keeps distinct logs; one versus three complete tours retained 74 textures on all three default-tree renderers, with peak-RSS growth OPENGLES3 691 to 721 MB (4%), OPENGL33 696 to 722 MB (3%), and SOFTWARE 1,204 to 1,252 MB (4%).

Final release sweep (later on 2026-10-16): all 8,483 structural checks pass. The OPENGLES3 nine-step battery covered every room and 1,086 panel controls in 116 s, every elevator/stair route, all 36 references and the leak check (74 textures retained; 691 to 718 MB, 3%). The leak harness now scales its default timeout with the requested tour count so a busy three-tour sample cannot inherit the one-tour five-minute ceiling; it still rejects incomplete Xvfb exits. Fresh full press-all tours were clean on FNA3D (158 s), WEBGPU (264 s), SDL_GPU (451 s) and HEADLESS (45 s). The full validation matrix above has no failure on any runnable Linux backend. HEADLESS now marks the punctual-light pixel probe unavailable instead of falsely failing when rendered-target readback throws `NotSupportedException`. ASan+UBSan passed 8,482 checks and a complete 295-room OPENGLES3 press-all tour in 557 s with no report; the sanitizer build directory was then removed. The subsequent stair usability and leak-harness changes are covered structurally and by their focused runs rather than another sanitizer build.

Compiled-instancing follow-up: the complete FNA3D press-all tour is clean in 134 s, and the final OPENGLES3 nine-step battery is green over the same sources (all rooms/controls in 166 s; every elevator and hall-to-hall stair route; 85 pass, 0 fail, 1 info, 1 compiled-effect skip; all 36 references; 74 textures retained and 694 to 720 MB peak RSS, 3%).

Earlier (2026-10-14, 280 rooms, after the Online Service wing, Exceptions, Vertex Types, Compiled Effects and the other new rooms): `tools/tour.sh -f 3 -p` exits 0 with 0 exhibit errors on FNA3D (76 s), WEBGPU (133 s) and SDL_GPU (274 s) in the multi-renderer tree (renderer chosen with `CNA_GRAPHICS_RENDERER`); the OPENGLES3 battery (`tools/check_all.sh`) is green.

Re-checked on 2026-10-14 with 272 rooms (after the Avatars, Leaderboards, Pipeline, Processor, Converters, Video and network-search rooms and the dressed screen rooms): `tools/tour.sh -f 3 -p` exits 0 with 0 exhibit errors on OPENGLES3, OPENGL33 and SOFTWARE (`SLOW=1 tools/check_all.sh`, all steps ok) and, in the multi-renderer tree, on FNA3D (75 s), WEBGPU (94 s) and SDL_GPU (175 s). HEADLESS then exited 3 with cube-data exceptions, subsequently fixed by CNA `d73f1aeab`. The Avatars, Video and Processing rooms were also looked at on those three: same picture; FNA3D has no DXT surface formats, and the Processing room says so on the level it cannot upload.

Re-checked on 2026-10-14 with 250 rooms after the room-frame change and the new rooms: `tools/tour.sh -f 3 -p` exits 0 on all three
renderers (OPENGLES3 65 s, OPENGL33 62 s, SOFTWARE 1,501 s); the stencil, cube-map, SkinnedEffect, terrain and off-centre rooms
look the same on all three.

## Windows: the MinGW build under Wine (DIRECTX9 and SOFTWARE)

The museum cross-compiles with CNA's MinGW toolchain and runs under Wine on the same Xvfb. This found a real portability bug
(`localtime_r` in the clock room does not exist on Windows) and is the only way to run CNA's DIRECTX9 renderer here:

```bash
BUILD_DIR=build-consumer MUSEUM_RENDERERS="DIRECTX9;SOFTWARE" \
  MUSEUM_CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$PWD/../cna/cmake/toolchains/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release -DMUSEUM_NETWORKING=OFF" tools/build.sh
cp -r build/bin/Content/Compiled build-consumer/bin/Content/        # the content tool is a foreign executable: reuse the native output
cp /usr/lib/gcc/x86_64-w64-mingw32/*-win32/{libgcc_s_seh-1,libstdc++-6}.dll /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll build-consumer/bin/
WINEPREFIX=$HOME/.cache/museum-wine SDL_VIDEODRIVER=windows CNA_GRAPHICS_RENDERER=DIRECTX9 \
  xvfb-run -a wine build-consumer/bin/cna-3d-museum.exe --script "goto:info;wait:5;quit" --no-audio
```

The Windows tree is configured with `-DMUSEUM_NETWORKING=OFF`: CNA's GamerServices needs libcurl, which the MinGW sysroot does not have, so the Players and Networking rooms there say they are not built in (the Avatars, Leaderboards and Net rooms are Linux-only for now). The content pipeline room (`MUSEUM_PIPELINE`) does build and run under Wine.

Under Wine the SOFTWARE renderer draws the lobby identically to Linux and tours all rooms with every control operated (272 rooms on 2026-10-14: exit 0, 0 exhibit errors, 1,313 s; the networking, avatar and video rooms say they are not built in on this tree; 257 rooms earlier took 1,413 s). **DIRECTX9** (Wine's Direct3D on llvmpipe OpenGL): the first tour (2026-10-14) ran without a crash but 11 rooms or stages were switched
off by the museum's guard, all with one CNA message, `DirectX9Renderer::DrawPrimitivesEx (BasicEffect): stride N with lighting=...
vertexColor=... texture=... has no matching CNA vertex layout`: CNA's DirectX 9 renderer accepted exactly one BasicEffect flag
combination per vertex stride. **Fixed in CNA on 2026-10-15** (`d3f1a9e51`): BasicEffect and AlphaTestEffect now accept any layout that
supplies the elements the chosen shader reads (all 32 BasicEffect shader buckets are reachable; the register tables were always
there), and `SetStringMarkerEXT` calls `D3DPERF_SetMarker` instead of throwing. The same tour now has **0 exhibit errors** over 286 rooms
(72 s) and the `rv-effects` BasicEffect check passes. Two 2026-10-16 follow-ups gave Texture2D, cube and volume resources real native classic-format storage plus live per-resource D3D9 classifiers: `tex-formats` rendered Color, Bgr565, Bgra4444, Alpha8 and HalfVector4; `tex-volume` round-tripped Color, Single and Vector4 with exact boxed reads. The expanded graphics-profile test passed 27/27 and bit-checked every advertised cube and volume format, including compressed cube blocks (`d1a78ca32`, `6da0e4bce`). CNA `829861cd8` then removed the remaining exact-stride gates from DualTextureEffect, EnvironmentMapEffect and SkinnedEffect: explicit compatible declarations at strides 44, 48 and 68 render exactly, the focused Wine test passes 22/22, and the full museum press-all tour has 0 exhibit/draw errors. CNA `0de32e6ba` added unshadowed point/spot `PunctualLightEXT` to the PBR and skinned-PBR shaders; the focused WineD3D pixel test passes 12/12 and was mutation-verified. CNA `86d85198c` then added a CNA-owned SM3 BasicEffect variant selected only for an active point/spot light: the shared test passes 5/5 and its mutation check failed the two illuminated cases, the museum probe reads 255,255,255, and all `rv-effects` checks now pass. CNA `79c513885` completed the matching SkinnedEffect path; its public identity-bone test passes 5/5 and its mutation fails exactly the two illuminated cases. CNA `d3539f3fb` adds directional/cascaded, point-cube and spot shadow reception to all rigid/skinned Basic/PBR paths; the WineD3D pixel oracle passes 29/29 and forcing directional shadows off makes six checks fail. The rebuilt museum reaches `cx-shadow` with 0 exhibit errors. Its caster is now renderer-independent: the CPU projects mesh vertices into orthographic light space and a vertex-coloured `BasicEffect` writes interpolated depth. A Wine/DIRECTX9 capture shows the live map and cast shadows with exit 0 and 0 exhibit errors.

## Every room against OPENGLES3 (`tools/room_diff.sh`)

Rooms whose arrival view differs from OPENGLES3's by more than 2 % of pixels (640 x 360, fixed clock and step):

| Renderer | Rooms over 2 % | What they are |
|---|---|---|
| OPENGL33 | 1 of 293 | Fresh 2026-10-16 result: `rs-fill` at 5.98 % (it can draw wireframe, OPENGLES3 cannot) |
| SOFTWARE | 16 of 293 | Historical result before the portable shadow caster: nine unsupported source-shader views (`fx-kernel`, `fx-plasma`, `fx-edge`, `cx-crt`, `fx-swirl`, `fx-halftone`, `tex-volshader`, `fx-vertexwave`, `fx-crt`), plus `rs-fill`, `rs-depthbias`, `materials` (animation phase), `au-video` (video frame), `cx-shadow`, `pf-dashboard` (live numbers) and `cx-package`. No model/PBR/IBL room exceeded 2 %. |
| SDL_GPU | 7 | `rs-depthbias` (z-fighting at zero bias resolves differently), `fx-plasma` and `fx-swirl` (floating-point trig in the shader), `rs-fill`, `cx-shadow`, `cx-package`, `pf-dashboard` (live numbers) |
| WEBGPU | 28 | those, plus `tex-formats` (Bgr565, Bgra4444 and Alpha8 are not offered), `cx-debugdraw`, and about twenty `vec-*`, `mat-*`, `quat-*` rooms whose 1-pixel grid lines sit one pixel off (a rasterisation difference, not a bug) |
| FNA3D | 26 of 295 | Fresh full 2026-10-16 result after CNA `83c7b324c`: `tex-formats`; `rs-fill`, `rs-depthbias`; twelve source/custom-shader views (`fx-kernel`, `fx-plasma`, `fx-edge`, `cx-crt`, `fx-halftone`, `cx-debugdraw`, `fx-swirl`, `tex-volshader`, `fx-vertexwave`, `fx-crt`, `fx-colormatrix`, `cx-package`); and `cx-shadow`, `cx-points`, `geo-vertextypes`, `rt-formats`, `an-walker`, `pf-dashboard` (live frame-time/resource measurements), `fx-grade`, `fx-compiled`, `gs-avatar`, `rt-mrt`, `vec-normalize`. The paired dashboard images have an identical room shell and different live bars. No model/PBR/IBL room remains in the list. |

After CNA `60019d629`, a focused SOFTWARE pressed tour of `model-pbr`, `model-skinned`,
`model-instances`, `model-material`, `model-ibl`, `model-morph` and `model-variants` exited 0 with
no exhibit errors. The fresh all-room measurement above subsequently confirmed the visual closure.

CNA `375c8927e` and `f74e0a8f7` subsequently implemented SOFTWARE shadow reception and split-sum
IBL. Their deterministic CPU pixel oracles each pass 8/8, and the full SOFTWARE label passes
194/194 (two expected shader-source skips). After rebuilding this two-renderer MinGW consumer, a
SOFTWARE/Wine tour through `model-ibl`, `model-pbr` and `model-skinned` exercised every panel and
finished with 0 exhibit errors. The fresh visual measurement above contains no model/PBR/IBL room.

Repeated on 2026-10-14 evening over 284 rooms: OPENGL33 vs OPENGLES3 differed by more than 1 % only in `rs-fill` (wireframe); SOFTWARE vs OPENGLES3 differed by more than 8 % only in `model-instances` (24 %, caused by the old flat PBR fallback), `fx-kernel` and `fx-plasma`. The PBR difference is historical after CNA `SOFTWARE-347`; the shader-dialect boundary remains.

This pass found two real defects that the 30 stored views and the pixel checks had missed, one in the museum and one in CNA. The museum's: `geo-manybuffers` drew black cones on OPENGLES3 and every GPU renderer (only SOFTWARE lit them): all its vertices have texture coordinate (0, 0), so the screen-space derivatives are zero and anisotropic filtering of a zero footprint is undefined (black on Mesa's software GL and Vulkan). It now samples with a linear sampler. CNA's: `rs-blendfactor` was opaque on WEBGPU
(fixed in CNA, see finding 12 and the new "Blend.BlendFactor on a 3D draw after an ordinary one" check).

## Findings

1. **SOFTWARE appeared to render the museum from the wrong camera angle.** It was not a renderer problem. SOFTWARE has
   no native window, so relative mouse mode and pointer warping do not work and `Mouse.GetState()` kept returning
   (0, 0); the player controller turned that into a huge mouse movement every frame. Fixed in the museum (the controller
   verifies pointer re-centring first and falls back to the keyboard). Found by the Renderer Validation rooms, which proved
   the rendering itself correct.
2. **CNA: loose `.tif`/`.tiff` were advertised as `Texture2D` formats but cannot be decoded** (the decoder is
   dependency-free and has no TIFF). Fixed in CNA: the extensions are no longer offered.
3. **CNA: the OPENGLES3/OPENGL33 capability profile reported 0 for colour attachments, sampled textures per stage and
   vertex inputs** (the interface's "unclassified" default) next to a supported `MultipleRenderTargets`. Fixed in
   CNA: EasyGL now reports its context's values.
4. OPENGLES3 and OPENGL33 do not support `FillMode.WireFrame` (CNA throws); the museum asks `SupportsCapability(WireFrame)`
   and shows solid geometry with a note.
5. `GetBackBufferData` and render-target readback need `GraphicsProfile.HiDef`.
6. Depth precision (information): all six renderers resolved a 5 mm gap at 100 m with near 0.1 / far 1000.
7. **CNA: EasyGL (OPENGLES3, OPENGL33) sampled a rendered `RenderTargetCube` upside down** (the live cube-map room showed
   the floor in both hemispheres of the mirror ball; FNA3D, WEBGPU, SDL_GPU and SOFTWARE agreed with each other, and the six
   face images read back with `GetData` were identical on all of them). A known open ticket on CNA's side (REMED-GFX-137);
   the museum's new check "Cube target sampled as an environment map keeps up as up" failed on exactly the two EasyGL
   renderers. Fixed in CNA (`a6e8104d2`: the stock env-map program mirrors the lookup for a rendered cube; shared example
   test added). CNA `c8722e283` completes the sampling half for the two independent PBR IBL cubes and opt-in source
   `ShaderEffect`; its eight-check witness passes on both GL profiles and is mutation-verified. The historical W1 upload
   asymmetry was already closed by CNA `23c5557f7`; the current readback contract confirms same-order 64/64 on both GL
   profiles. CNA `313cd8ae6` closes the later GLES-only face-edge seam: the face-local row flip now keeps its bilinear
   footprint inside the selected face, and the exact-edge oracle is mutation-verified on both GL profiles.
8. **FNA3D** reports `Instancing`, but its stock effects correctly cannot consume application-defined per-instance semantics.
   The two instancing rooms now use a museum-owned compiled XNA effect whose four `BLENDWEIGHT` rows form the instance matrix,
   so FNA3D executes one real instanced draw instead of the former stock-effect refusal/fallback. The new validation pixel
   contract places two orange quads at x=16/48 and passes exactly (230,76,38 at both probes). CNA `c8145f60c` closes the former glTF
   fallback: CNA-owned rigid/skinned Effect Framework binaries now render direct-light metallic-roughness PBR, and CNA
   `ff6f18c08` adds the shared split-sum IBL path, `4d3cc6892` adds double-sided tangent-basis flipping,
   `e44369bbe` adds independent UV0/UV1 selection, and `83c7b324c` adds the shared directional/cascaded, point-cube and spot shadow receivers across Basic, Skinned and PBR paths. The shader rooms already
   used `CustomEffects`, which FNA3D cannot compile from source. (CNA documents these boundaries in
   `docs/fna3d-renderer.md`.)
9. **Custom shaders need source in the renderer's own dialect** (`GetShaderDialectEXT`). The shader rooms carry three: GLSL ES 3.00
   (OPENGLES3, OPENGL33 and SOFTWARE-adapted), WGSL (WEBGPU, `Exhibits/Shaders/ShaderSources.cpp`: the same twelve algorithms
   line for line) and Vulkan GLSL 450 (SDL_GPU: the GLSL ES bodies behind another header; the renderer compiles it to SPIR-V
   with shaderc). FNA3D takes none (`CustomEffects` false), so its shader rooms show the scene unprocessed and say so. The
   shader package room (`cx-package`) now holds a WGSL variant too, and "selects WGSL" on WEBGPU. Two behaviours of CNA to know
   when writing these: SDL_GPU's `ShaderEffect` has **one fixed 128-byte uniform block whatever the uniform names say** (a
   viewport size, a matrix, one vec4 "color", one float "slot0"; every `SetUniformVec2/3/4` writes the same vec4), so the lab
   packs its four uniforms into those two slots; and a packaged `ShaderEffect` must name its entry point `main` while the WGSL
   inside it is still looked up as `vs_main`/`fs_main`. WEBGPU's SpriteBatch route writes **colour attachment 0 only**, so the
   multiple-render-target room's edge map stays empty there (the room measures it and says so). `cx-shadow` deliberately
   avoids source shaders: CPU-projected depth is interpolated by a vertex-coloured stock `BasicEffect`, so one caster path
   works on OPENGLES3, SOFTWARE and DIRECTX9.
10. **CNA: the SOFTWARE renderer ignored `BasicEffect.PunctualLightEXT`** (point and spot lights): a lit sphere stayed black, and
    there is no capability flag to ask first, so the museum added `Rendering/Probes.cpp`, a one-off render test whose answer
    the light rooms print on their panels and the validation matrix counts. Fixed in CNA (`53aba6ef1`: EasyGL's falloff and
    cone, no shadow map; shared example test). CNA `cb52126f4` closes FNA3D with a CNA-owned,
    preshader-free compiled Effect selected only while the extension is active. BasicEffect and
    identity-bone SkinnedEffect pixel witnesses each pass 5/5; ordinary draws retain Microsoft's
    untouched stock bytecode.
11. **CNA: WEBGPU applied the blend factor current at the flush to every queued 3D draw**, so a draw made with `Blend.BlendFactor` and
    followed by `BlendState.Opaque` (which resets the factor to white) came out opaque. Found by `room_diff` (the render-state room
    looked right on every other renderer), reproduced by a new validation check, fixed in CNA (`09c3108e5`: each queued draw carries
    its factor; shared example test, fails without the change).
12. **An exception inside one room no longer takes the museum down** (a museum fix found by FNA3D's instancing exception, which
    escaped `Game::Run`): the world catches exceptions from each room's hooks, logs `[museum][exhibit-error]`, toasts the
    message and switches the room's hooks off after three. `tools/tour.sh` reports them and exits 3.

## Adding a check

Open `src/Exhibits/Validation/ValidationLab.cpp`, add a block to the mode that fits (`Add(name, expected, got, pass, image)`
logs one `[museum][validate]` line per check and shows a thumbnail of the picture). If it fails on one renderer and not
another, that is a finding about the renderer: reproduce it in a minimal check, record it here, and fix it in CNA if it is
clearly CNA's. Do not add a workaround to the museum.
