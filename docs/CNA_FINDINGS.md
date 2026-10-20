# CNA defects and divergences found by the museum

The museum does not change CNA: another maintainer owns `../cna`. Each entry here was reproduced from the museum
against the CNA revision named in `docs/CNA_API_INVENTORY.md`, with the exact change it needs. Workarounds in the museum
are named in the code where they are used and listed here; nothing is hidden behind one.

## Open

### BoundingFrustum.Intersects(Ray) works only for a ray that starts inside the frustum
- **Where:** `modules/math/src/BoundingFrustum.cpp`, `BoundingFrustum::Intersects(const Ray&, std::optional<float>&)`.
- **What happens:** if the ray's origin is inside, the result is 0 (correct). If the origin is outside (containment
  `Disjoint`), CNA returns null even when the ray enters the frustum (an earlier revision threw
  `ArgumentOutOfRangeException`); if the origin lies on a plane (`Intersects`), it throws `NotImplementedException`.
  The math-checks room's ray from z = 10 towards a camera frustum at z = 6 gets null where XNA returns 4.5.
- **XNA 4.0:** never throws here. It clips the ray against the six planes (slab test): for each plane with
  `|dot(direction, normal)| >= 1e-5` it narrows `[tmin, tmax]` by `t = -(dot(position, normal) + D) / dot(direction, normal)`,
  returns null when the interval empties or a parallel ray lies outside a plane, and otherwise returns `tmin` when it is
  non-negative, else `tmax` when non-negative (`../xna4-decomp/.../Microsoft.Xna.Framework/BoundingFrustum.cs`,
  `Intersects(ref Ray, out float?)`).
- **Needed change:** replace the two throws with that clipping loop.
- **Seen in the museum:** `math-checks`, check "BoundingFrustum.Intersects(Ray from outside)" (FAIL); the by-reference
  overload agrees with the value one for a ray from inside (PASS).

### Host migration picks a gamer of the machine that just left
- **Where:** `modules/net/src/Internal/ENetBackend.cpp`, `AttemptHostMigration`: survivors are every wire id except the
  departing *host gamer*; the departing machine's other local gamers are still in `WireIdToGamer`, because
  `HandleDisconnect` asks for the migration before `RemovePeerGamers`.
- **What happens:** a host with two local gamers (ids 0 and 1) leaves; every client picks id 1, a gamer on the machine
  that is gone, cannot reach it and ends the session with `HostEndedSession` although every client sees
  `AllowHostMigration true` (`tools/net_check.sh`, round with two local gamers on the host).
- **Needed change:** exclude every gamer of the departing peer (its machine), not only the host gamer, before choosing
  the lowest remaining wire id.

### A gamer joining a game in progress stays in the lobby
- **Where:** `modules/net/include/CNA/Internal/Net/NetPacketCodec.hpp`, `ServerWelcomeMessage` (wire ids, roster and
  session properties, but no session state), and `ENetBackend`'s `HandleServerWelcome`. `StartGame` announces Playing with
  `BroadcastStateChange` only to the peers connected at that moment.
- **What happens:** with `AllowJoinInProgress` the host starts the game, a second museum joins: the joiner's
  `SessionState` is `Lobby` and it never raises `GameStarted`, while the host plays (`tools/net_check.sh`, round lifecycle:
  "Joined: ... AllowJoinInProgress true, state Lobby").
- **XNA 4.0:** a gamer that joins in progress joins a session whose `SessionState` is `Playing`.
- **Needed change:** carry the session state in `ServerWelcomeMessage` (and apply it in `HandleServerWelcome`, raising
  nothing for the joiner, as XNA's joiner does not get GameStarted replayed), or send a state change to the new peer.

### PbrEffect differs between renderers (rv-shading)
The same face-on quad (`VertexPositionNormalTexture`), one directional light along -Z, camera in front (`tools/renderer_check.sh`):
- **OPENGLES3, OPENGL33, SOFTWARE, FNA3D:** red dielectric 151,10,10; MetallicFactor 0 against 1 on a grey base 148 against
  255; emission (0,1,0) on black 10,255,10.
- **WEBGPU:** the base colour comes out unlit (red 255,0,0, grey 0.8 → 204,204,204 for both MetallicFactor values):
  lighting, metallic and roughness are not applied. **Needed change:** the WebGPU PbrEffect shader path must evaluate the
  lights as the GL one does.
- **SDL_GPU:** the draw is refused with `NotSupportedException: CNA SDL_GPU: the selected stock effect cannot be matched to
  this VertexDeclaration`: `SdlGpuRenderer.cpp` draws PbrEffect only for 48- or 60-byte vertices (with tangents). XNA's
  effects take any declaration that carries the semantics they read. **Needed change:** accept vertices without a tangent
  (no normal map) as the other renderers do.

### SDL_GPU: no ShaderEffect source layout samples a volume, and one crashes
`tex-volshader` draws slice 20 of a 32³ volume through `ShaderEffect.SetTexture(0, Texture3D)` and compares every pixel
(storage itself is fine everywhere: `Texture3D.GetData` returns all 32 768 texels). GLSL ES on OPENGLES3/OPENGL33 and WGSL
under the descriptor contract on WEBGPU (volume unit 0 at `@group(1) @binding(8)`, sampler at 40, scalars at
`@group(3) @binding(0)`) pass, 0 of 1024 pixels off. On SDL_GPU (lavapipe) every documented layout fails:
- SDL's native layout (vertex uniforms set 1, fragment sampler set 2 binding 0, fragment uniforms set 3): the sampler reads
  the sprite's own texture, the volume is never bound (all 1024 pixels white).
- the portable layout's volume binding (set 1 binding 8, as `shader_effect_per_unit_sampler_test.cpp` declares it) with
  the native vertex shader (uniforms set 1 binding 0): **the process dies with a segmentation fault**.
- the portable layout in both stages (scalar block set 3 binding 0): nothing is drawn (all pixels black) in a single-room
  run, and the press-all tour on SDL_GPU died with a segmentation fault entering the room. The museum therefore runs no
  volume shader on SDL_GPU and says so; the storage check still runs there.
- **Needed change:** document the GLSL 450 layout a ShaderEffect source must use for a volume on SDL_GPU, and refuse a
  layout CNA cannot map with a compile diagnostic instead of crashing (`SupportsTexture3DSamplingEXT` says true).

### Vertex texture fetch (XNA HiDef) is missing on WEBGPU and SDL_GPU
With the compiled-effects options on (`build-probe`), `fx-compiled` checks the fixtures exactly: `blur2tap` reflection and
its arithmetic (Weight 0.5 and 0.25 on a flat colour) pass on OPENGLES3, FNA3D, WEBGPU and SDL_GPU, and the vs_3_0
`vertextexture` effect draws a graph whose shape follows `VertexSamplerStates[2]` (PointClamp against PointWrap) on
OPENGLES3 and FNA3D. XNA's HiDef profile includes vertex texture fetch from Single/Vector4 textures, but:
- **WEBGPU:** creating the Vector4 vertex texture fails: `Texture: SurfaceFormat 15 is not implemented by the selected
  graphics renderer`.
- **SDL_GPU:** the draw is refused: `this compiled effect's vertex shader samples a texture; vertex-stage texture sampling
  is unsupported by this renderer`.
- **Needed change:** implement vertex-stage sampling (and the float vertex-texture formats) on both, or make HiDef device
  creation report the missing feature; the room logs both as SKIP with these messages.

### Browser WebGPU ends the program on a texture format the device was not given
- **What happens:** in the browser build (WEBGPU through emdawnwebgpu, Chrome 155 on the machine's GPU), entering
  `app-device` (which queries the device's capabilities and its renderer capability profile) ends the program with
  `exit 0`, without `Game.Exiting`; the console says `Uncaught (in promise) TypeError: Failed to execute 'createTexture' on
  'GPUDevice': Use of the 'rgba16unorm' texture format requires the 'texture-formats-tier1' feature to be enabled`
  (`MUSEUM_WEB_RENDERER=WEBGPU tools/web_tour.sh app-device`). The native WEBGPU renderer runs the same room cleanly.
- **Needed change:** request `texture-formats-tier1` when the adapter offers it, and otherwise refuse `Rgba64` (and the
  other tier-1 formats) with a C++ exception instead of creating the texture; a JavaScript exception must not end the
  program as a successful exit.
- **Also on browser WebGPU:** every run reports one validation error at start-up, `CNA WebGPU uncaptured error (2):
  Destroyed texture [Texture (unlabeled 1280x720 px, TextureFormat::BGRA8Unorm)] used in a submit` (the canvas texture of
  the first frame is used after the canvas was reconfigured); `tools/web_tour.sh` counts it as a failure.
- **Museum handling:** in a browser WebGPU build `app-device` does not query the capability lists and says why. The
  browser tour (`MUSEUM_WEB_RENDERER=WEBGPU tools/web_tour.sh`) then reached 162 of 320 rooms with 0 exhibit errors until
  the same exception ended the page near `rt-mrt` (a render target in Rgba64); the start-up validation error was reported
  18 times on the way. Any room that creates an Rgba64 texture ends the browser WebGPU page.
- **Re-run on 2026-10-20, after the WEBGL2 browser tour passed 320/320:** four rooms end the browser WebGPU page this way,
  found one at a time: `rt-mrt`, `pl-renderer`, `rv-formats`, `rv-capabilities`. With them left out
  (`MUSEUM_WEB_SKIP="rt-mrt pl-renderer rv-formats rv-capabilities"`) the other 316 rooms are all visited with 0 exhibit and
  0 script errors and 69 renderer self-checks pass; the one self-check FAIL is the WEBGPU PBR metallic finding above, as
  natively. The destroyed-canvas-texture validation error is not limited to start-up: it was reported 85 times over the
  tour, so the browser WebGPU tour fails until both are fixed in CNA.

### HapticDevice's inline default constructor cannot be compiled outside CNA (CNA-DEMOS-ISSUE-009)
- **Where:** `modules/input/include/CNA/Input/HapticDevice.hpp`: `CNAEXT HapticDevice() = default;` with a
  `std::unique_ptr<CNA::Platform::IPlatformHapticDevice>` member whose type no public header completes.
- **What happens:** default-constructing a `HapticDevice` (for example as a class member) fails with
  "invalid application of 'sizeof' to incomplete type 'CNA::Platform::IPlatformHapticDevice'".
- **Needed change:** declare the default constructor in the header and define it (`= default`) in the .cpp, as the
  move operations and destructor already are.
- **Museum workaround:** `DeviceLab` holds `std::optional<HapticDevice>` filled from `Haptics::OpenEXT`.

### VertexTypeRegistryEXT::Resolve names the refused type in its mangled form
- **What happens:** `Resolve(System::Type::From<T>())` for an unregistered `T` throws
  `ArgumentException: The vertex type 'N12_GLOBAL__N_118UnregisteredVertexE' is not a registered ...`.
- **Needed change:** demangle (`abi::__cxa_demangle` on GCC/Clang), as sharp-runtime's TypeConverter refusals already do.
- **Seen in:** `rv-vertextype` (the check passes: the refusal itself is correct).

### SystemTray.IsSupported is true where no icon can be created
- **What happens:** on Linux without a StatusNotifier host (no session bus), `SystemTray::getIsSupportedProperty()`
  returns true, and `SystemTray("...")` then throws `CNA::Platform: CreateTray failed: Unable to create tray: no SNI support!`.
- **Needed change:** either probe for a StatusNotifier watcher in `IsSupported`, or document that it reports the build's
  capability only and creation can still be refused.
- **Seen in:** `tools/devices_check.sh` (reported as UNAVAILABLE).

### MessageBox.Show marks no default button
- **What happens:** CNA passes every `SDL_MessageBoxButtonData` with `flags = 0`, so Return or Escape can only dismiss
  the box (`-1`), never choose a button.
- **Needed change (suggestion):** mark the first button `SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT` and the last
  `..._ESCAPEKEY_DEFAULT`, or document that only a pointer click chooses.

### EnvironmentMapEffect reflects no environment-map parameter
- **What happens:** `EnvironmentMapEffect::CacheEffectParameters` (`modules/graphics/src/Xna/EnvironmentMapEffect.cpp`)
  registers `EnvironmentMapAmount`, `EnvironmentMapSpecular`, `FresnelFactor`, colours, fog and matrices, but neither
  `Texture` nor `EnvironmentMap`; the textures live only in C++ members. XNA's effect reads both from its parameter
  collection (`Parameters["Texture"]`, `Parameters["EnvironmentMap"]`), so code that reflects or sets them by name finds
  nothing in CNA.
- **Needed change:** add the `Texture` (Texture2D) and `EnvironmentMap` (TextureCube) parameters and route the
  properties through them, as the other stock effects do for their textures.

### No capability query for reading a render target back
- **What happens:** `Texture2D::GetData` on a render target raises `System::NotSupportedException` on HEADLESS (which
  rasterises nothing, by design). Neither `GraphicsCapability` nor `RendererCapabilityProfile`'s `RendererFeature`
  list has an entry for render-target readback, so a program can only find out by trying.
- **Needed change (suggestion):** a `RendererFeature::RenderTargetReadback` (and `GraphicsCapability` twin).
- **Museum handling:** `SignFactory` treats that refusal as the renderer's declared limitation (signs keep their
  background colour) and any other exception as an error.

### WebGPU renderer cannot render without a window surface
- **What happens:** with SDL's `offscreen` video driver (no display server; EGL and Vulkan work there, and OPENGLES3,
  OPENGL33, FNA3D and SDL_GPU render on the GPU), creating the WEBGPU device fails with
  `WebGPURenderer::Surface failed: missing native window (Headless)`.
- **Needed change (suggestion):** when the window has no native handle, render into an offscreen texture (wgpu needs no
  surface for that) and present by readback, or report the renderer unavailable through the fallback history so the
  application can pick another; today the device creation exception is the only signal.

### StorageDevice and StorageContainer skip XNA's argument, state and disposal checks
Measured by the `ct-storage` button "Error paths and async results, checked against XNA 4.0" (scenario
`tests/scenarios/storage_contracts.json`); each expectation is the code in `../xna4-decomp/.../Microsoft.Xna.Framework.Storage`:
- `EndShowSelector` / `EndOpenContainer` called twice on one result: XNA `InvalidOperationException` (CannotEndTwice), CNA
  returns a second device/container.
- `BeginShowSelector(sizeInBytes < 0, ...)`: XNA `ArgumentOutOfRangeException`, CNA accepts it.
- `EndOpenContainer` on another device than the one whose `BeginOpenContainer` produced the result: XNA `ArgumentException`,
  CNA opens the container.
- A result from the other Begin method, an empty display name, and empty paths (`CreateFile("")`, `FileExists("")`): XNA
  `ArgumentNullException`, CNA the base `ArgumentException` (code catching `ArgumentNullException` misses it).
- `DeleteDirectory` of a non-empty directory: XNA `IOException`, CNA lets `std::filesystem::filesystem_error` escape.
- **Needed change:** an end-once flag on both result types,
  the device check in `EndOpenContainer`, the size check, `ArgumentNullException` for null/empty names, and IOException
  for the directory case. Agreeing already: async callbacks and `AsyncState`, path containment, `FileNotFoundException`,
  `DeleteFile` of a missing file, `Disposing` raised once.

### StorageDevice.SetAppNameEXT throws when no storage root can be created
- **What happens:** `SetAppNameEXT` stores the name and then resolves and creates the root at once (for the isolated
  storage override); with no writable per-user data folder it throws `StorageDeviceNotConnectedException`. Called
  unguarded at start-up (as the museum did), it ends the program before any game code runs. Later storage calls throw the
  same exception, which is the XNA place for it; local GamerServices achievements then fail with the same message.
- **Needed change (suggestion):** resolve the root lazily (the isolated-storage override too), so that only the calls that
  need the folder report the missing device.
- **Museum handling:** `main` and `ct-storage` catch it; scenario `storage_not_connected` checks `IsConnected false` and
  the exception from `EndOpenContainer`.

### Content pipeline divergences found by the Pipeline Workshop (wk-* rooms; each is a FAIL check there)
1. **Nested build names collide** (wk-processor): `Convert(material, "MaterialProcessor")` builds `plaque.png` as
   `plaque_0`; a later `BuildAsset(plaque.png, "TextureProcessor")` with other parameters fails with
   `nested build output 'plaque_0' collides with another output of this node`. XNA chooses names unique across the whole
   build (`BuildCoordinator.ChooseOutputFilename`), so the second would be `plaque_1`. Cause: each
   `XnaBridgeProcessorContext` keeps its own `generatedNames_`. Needed: share the generated-name set per build.
2. **`BuildAndLoadAsset<TextureContent, TextureContent>(png, "TextureProcessor")`** throws PipelineException
   (`processed to 'CNA.Content.Cnb.Texture2DData' where '...TextureContent' was expected`): the "TextureProcessor" alias
   produces a CNA runtime type; XNA returns a Texture2DContent.
3. **Convert errors:** an unknown processor name throws PipelineException (XNA: InvalidOperationException from
   `ProcessorManager.ValidateProcessorTypes`); an empty name throws PipelineException (XNA: ArgumentNullException("processorName")).
4. **AddDependency:** an absolute path outside the source root throws `std::invalid_argument` (XNA records it; CNA's
   `ResolveSourceDependency` sandboxing is deliberate but undocumented as a difference); `"../escape.txt"` throws
   `std::invalid_argument` (XNA: PipelineException, the path is not absolute). A `std::` exception escaping a public
   pipeline API is itself unlike XNA.
5. **MaterialProcessor with EffectMaterialContent:** with no Effect CNA skips silently; with an empty Effect file name it
   throws PipelineException from BuildAsset. XNA throws InvalidContentException in both cases.
6. **`ContentReader.ReadRawObject<Int32>`** throws "no content type reader is registered for the requested type" although
   the manifest lists Int32Reader; XNA resolves it through `ContentTypeReaderManager.GetTypeReader(typeof(T))`.
7. **Truncated XNB body** (size field patched to match): CNA throws `System::IO::EndOfStreamException`; XNA wraps the
   IOException in ContentLoadException.
8. **XNB version 4** loads without complaint; XNA 4.0 accepts only 5 (deliberate in CNA, `XnbHeader.hpp:48`; listed so
   the difference is visible).
9. **XmlSerializer has no member list for Color or Rectangle** (`XmlSerializationEXT.hpp` covers vectors, Quaternion and
   Matrix): `XmlSerializer<Color>` and `XmlSerializer<Rectangle>` do not compile; .NET serializes both by reflection.
10. **`BoneWeightCollection.NormalizeWeights()` keeps zero weights** (wk-animation): for A .2, B .5, C 0, D .1 CNA answers
    B .625, A .25, D .125, **C 0**; XNA 4.0 (`BoneWeightCollection.NormalizeWeights(int)`) removes every weight `<= 0`
    (and every one at index `>= maxWeights`) before summing, so C is gone. Needed in `VertexCollections.cpp`
    `Normalize`: drop entries whose weight is `<= 0` in the same pass that truncates to `maxWeights`, and scale with
    `weight * (1 / total)` as XNA does (CNA divides, which can differ in the last bit).
11. **`PipelineComponentScanner.Update` answers false for a changed list** (wk-processor): after `Update({"Museum.Workshop"})`,
    `Update({"Museum.Workshop", "No.Such.Catalog"})` records the error but returns false. XNA returns true whenever the
    (empty-filtered) list differs from the previous call's (`AlreadyUpToDate` compares the list and the timestamps) and
    only answers false for the same list. CNA's header documents "true when the set of components changed". Needed:
    remember the previous list and return true whenever it differs, as XNA does.
12. **Two `ContentTypeSerializerAttribute` instances are not `Equals`/`Match`** (wk-cnbloader): .NET `Attribute.Equals`
    compares the type and every instance field, so two instances of this field-less attribute are equal (and `Match`
    defaults to `Equals`). CNA inherits sharp-runtime's `System::Attribute::Equals`, which is reference equality.
    Needed: `ContentTypeSerializerAttribute` (or `System::Attribute` for field-less attributes) compares
    `typeid(other) == typeid(*this)`.
13. **A built glTF whose images are embedded cannot be loaded** (wk-routes): `ContentPipeline::Build(uvsets.gltf)` (images as
    data URIs, default `ModelProcessor`) succeeds with one output, no deployment files and runtime references
    `uvsets_tex0.png`, `uvsets_tex1.png` that nothing produces; `cna-content build` publishes the same single
    `uvsets.cnb`. `ContentManager.Load<Model>` then throws `ContentLoadException: could not load asset 'uvsets_tex0.png'`.
    With `generateChildAssets = true` the two images are child outputs and the model loads. Needed: when the images are
    embedded, deploy the extracted files (`ImportedModelDocument::generatedTextureFiles`) under the referenced names, or
    generate the child assets automatically, or fail the build naming the dangling references.
14. **`DecodeWavAsCnbSoundEffect` does not do what `CnbSourceImport.hpp` says** (wk-incremental): an 8-bit WAV comes back as
    `CnbAudioFormat::Pcm8` with its bytes unchanged (documented: "8-bit unsigned PCM (widened exactly)" to Pcm16), and a
    24-bit WAV is accepted and narrowed to Pcm16 (documented: "Refused, by name: 24-bit, 32-bit, IEEE float").
    `ProcessImportedSoundEffect`'s comment also still promises Pcm16. The behaviour matches the later XNASWEEP-197 work
    (Pcm8 in schema 2, `ImportedPcmNarrowsToPcm16`). Needed: correct the comments of `DecodeWavAsCnbSoundEffect`,
    `ImportWavAsCnbSoundEffect` and `ProcessImportedSoundEffect` (or restore the documented refusal of wide PCM).
15. **`Tasks::TaskItem::operator==` cannot be called** (wk-tasks): it is declared `= default`, but the base
    `System::Object` has no `operator==`, so the defaulted operator is implicitly deleted and `item == other` does not
    compile. Needed: define it explicitly over `itemSpec_` and `metadata_`.
16. **BuildContent, CleanContent, GetLastOutputs and ContentProject do not link from `cna_content_pipeline`** (wk-tasks):
    their definitions (`BuildContent.cpp`, `CleanContent.cpp`; `ContentProject.cpp` calls BuildContent's setters) call
    `CNA::Content::Pipeline::RunContentCompiler`, which is defined only in `cna_content_compiler`
    (`tools/content/content.cpp`), and `cna_content_pipeline` does not link it (the dependency runs the other way). A
    game linking `cna_content_pipeline`, as this museum does, gets `undefined reference to RunContentCompiler` as soon as
    it names one of them (`nm`: `U ...RunContentCompiler` in `BuildContent.cpp.o` and `CleanContent.cpp.o`). The same holds
    for `RegisterBuiltInContentPipeline`. Needed in CNA: move the four task sources into `cna_content_compiler` (and say
    "link CNA::ContentCompiler" in their headers), or move the coordinator into a library `cna_content_pipeline` links.
    The museum now links `cna_content_compiler` explicitly (`MUSEUM_CONTENT_COMPILER`), and wk-contentproject runs all four.
17. **A clean leaves the coordinator's lock file behind** (wk-contentproject): after `CleanContent.Execute` (and
    `RunContentCompiler({"clean", out})`) the output directory still holds `.cna-content.lock`, which a build or the clean
    itself created; the outputs and the manifest are removed correctly and foreign files are kept. XNA's CleanContent
    deletes the outputs and its own cache file and leaves no pipeline file (`BuildItemCollection.DeleteStaleOutputFiles`,
    `PathUtils.SafeDeleteFile(CacheFilename)`). Needed: the clean removes the lock file after releasing it (or the lock
    lives under the intermediate directory).

### Vendored ENet shifts a signed 1 by 31 places (UBSan)
- **Where:** `third_party/enet/protocol.c`, `enet_protocol_handle_send_unsequenced`: `1 << (index % 32)` at lines 492
  and 498, on the `enet_uint32` unsequenced window.
- **What happens:** with an unsequenced packet whose window index is 31 the shift is undefined behaviour in C
  (`runtime error: left shift of 1 by 31 places cannot be represented in type 'int'`, 9 times each in the
  ASan+UBSan `tools/net_check.sh` run on 2026-10-20, packet round with `SendDataOptions.None`). GCC and Clang produce
  the expected bit today, so no packet was lost, but the compiler may assume it never happens.
- **Needed change:** `1u << (index % 32)` in both places.

### Data races found by ThreadSanitizer (build-tsan, 2026-10-20)
Seen in the targeted TSan run (`tools/scenarios.py notifications inspector`, a 30-frame press-all tour of the au-*, dev-notify,
pl-inspector, net-packets and net-session rooms, `tools/audio_check.sh`, `tools/net_check.sh`; the network rounds were clean).
- **`HttpNotificationChannel::Close` and its listener thread:** `Close()` (`modules/phone/src/HttpNotificationChannel.cpp:223`)
  writes `listenSocket_ = -1` under the channel mutex while the listener thread started by `Open()` reads `listenSocket_` for
  `::accept` in `Listen()` (line 180) without it (dev-notify, "Close, then send to the closed channel"). Needed: the listener works on the socket
  value it was started with (or `listenSocket_` becomes atomic), and `Close` only shuts it down and joins.
- **`SetMixerTrackMixCallback` against the mixer thread:** `context->mixCallback` and `mixUserdata`
  (`modules/audio/src/Backend/Sdl3Mixer/MixerEngine.cpp:559-560`) are written by the game thread
  (`SoundEffectInstance::Apply3D` → `INTERNAL_applyComposedTrackProperties` → `EnsureTrackDspState`) while SDL's audio thread
  reads them in `MixCallback` (line 245) for a track that is already playing (every au-* room with 3D voices, 12 reports per
  audio_check run). Needed: write them under the mixer lock the cooked callback runs under, or set them before the track plays.
- **Static teardown of pending pan state while the audio thread still mixes:** at process exit `~PendingPanStateCleanup`
  (`modules/audio/src/Xna/SoundEffect.cpp:236`, `DrainClaimed` line 218) deletes pending pan states while the SDL audio thread
  still runs `FireAndForgetPanCallback` (line 154) through `MixCallback` (seen at the end of the notifications and Inspector
  scenarios, which played fire-and-forget sounds). Needed: stop or lock the mixer before the static drain.

### HttpNotificationChannel fails silently where no socket can listen (browser build)
- **Where:** `modules/phone/src/HttpNotificationChannel.cpp`, `Open()`: when `socket`, `bind`/`listen` or `getsockname`
  fails it logs a warning (or nothing, for `getsockname`) and returns.
- **What happens:** in the browser build (Emscripten, no listening sockets) `dev-notify` opens the channel and nothing
  follows: `ChannelUriUpdated` is raised 0 times, `ChannelUri` stays null and `Find` finds nothing, and the game is told
  nothing (CNA's channel declares only `ChannelUriUpdated` and `HttpNotificationReceived`, no `ErrorOccurred`); a raw send to the unreachable `http://127.0.0.1:9/notify` reports accepted, where the native build reports it
  refused (`tools/web_scenarios.py notifications`, 2026-10-20). XNA's channel reports a failed open through
  `ErrorOccurred` with `NotificationChannelErrorType.ChannelOpenFailed`.
- **Needed change:** add the channel's `ErrorOccurred` event (`NotificationChannelErrorEventArgs`), raise it with
  ChannelOpenFailed when the channel cannot listen, and complete a send that could not connect as refused on every platform.

### Float16RenderTargets is reported on the Android emulator, whose half-float target reads back as NaN
- **Where:** EasyGL's capability profile (OPENGLES3) on the Android emulator's guest GLES (`-gpu guest`, Android 15 image).
- **What happens:** `GraphicsDevice` reports `GraphicsCapability.Float16RenderTargets` as supported, but in rv-capabilities
  two additive white quads drawn into a `HalfVector4` render target read back as `(0.000, NaN, 0.000, NaN)` instead of 2.0
  per channel (`tools/android_run.sh`, 2026-10-20). On the desktop OPENGLES3 renderer the same check passes.
- **Needed change:** claim the capability only when the context can render to and read back RGBA16F
  (`EXT_color_buffer_half_float`/`EXT_color_buffer_float`, and a half-float `glReadPixels` path that the implementation
  accepts), or convert on readback.

### Native C API
The cx-capi room's C17 clients of CNA's C ABI (`src/CApi/`: the in-museum client, and the game probe `bin/cna-c-probe`
that the room starts as a child process because a process hosts one game) check every C header family against the
header documentation and, for XNA-mirroring routes, against XNA 4.0. The divergences below are FAIL checks there; the
scenario `capi_native` expects exactly these FAIL lines (logged as `[museum][check] cx-capi <name>: FAIL`, and as
`cx-capi game: <name>: FAIL` for the game probe).

- **One process carries two CNA states.** `modules/c-api/CMakeLists.txt` links `cna_c_api` against the static component
  archives (`target_link_libraries(cna_c_api PRIVATE cna_core cna_platform cna_runtime ...)`), so `libcna_c_api.so` embeds
  its own copy of every process-wide CNA singleton, while a C++ host links the same code again through `libcna.so`. In the
  museum (both libraries loaded) the C ABI does not see the host's state: the title the museum declares at static
  initialisation (`CNA::AssemblyTitleAttributeEXT kAssemblyTitle{"CNA 3D Museum"}`, `src/Main.cpp`) reads back as `""`
  through `cna_assembly_copy_title_ext`, although `core_ext.h` documents the title as process-wide; the renderer selection
  reports not latched and `cna_graphics_renderer_get_active_ext` answers `CNA_RESULT_INVALID_STATE` although the museum's
  GraphicsDevice exists; XNA's built-in type readers are not in the C copy's registry until a C content manager exists.
  **Needed change:** when CNA itself is built as a shared library, link `cna_c_api` against that library (the `CNA`
  target) instead of the component archives, so the process-wide state exists once; if two copies are intended, document
  in `docs/c-api/CONSUMING.md` that a process must not combine the C ABI with CNA's C++ library and drop "process-wide"
  from the affected route docs. Check: "one process, one CNA: cna_assembly_copy_title_ext sees the title the host
  declared through CNA's C++ API".
- **`cna_cnb_copy_decompressed` with `CNA_CNB_COMPRESSION_NONE` ignores the declared size.** `cnb.h` says the payload
  "must expand to exactly `uncompressed_size`", checked against `max_uncompressed_size` before anything is allocated;
  `DecompressCnbChunk` (`modules/content/src/Cnb/CnbChunkCompression.cpp`) returns the stored bytes unchanged for `None`
  before either check, so 6 stored bytes declared as 5 succeed with 6 bytes. (The document parser checks
  `stored_size == uncompressed_size` itself, so a parsed file is not affected; the standalone route is.)
  **Needed change:** for `None`, throw `ContentLoadException` unless `uncompressedSize <= maxUncompressedSize` and
  `stored.size() == uncompressedSize`. Check: "cna_cnb_is_compression_supported, copy_compression_name, ...".
- **`MouseState.ToString` uses another format.** `modules/input/src/Xna/MouseState.cpp` returns
  `[MouseState X=10, Y=20, Buttons=Left, Wheel=120]`; XNA 4.0 returns `{X:10 Y:20 Buttons:Left Wheel:120}`
  (`string.Format("{{X:{0} Y:{1} Buttons:{2} Wheel:{3}}}", ...)`, the buttons space-separated or `None`), the same
  `{Name:value}` style as every other XNA value type CNA already matches. **Needed change:** that format. Check:
  "cna_mouse_state_get_string_size/copy_string(10, 20, 120, LEFT)".
- **`GamePadState.ToString` returns the type name.** `modules/input/src/Xna/GamePadState.cpp` follows FNA
  (`base.ToString()`), and `input_gamepad.h` states that "the canonical type does not override its string conversion";
  XNA 4.0 does override it: `string.Format("{{IsConnected:{0}}}", _connected)`, i.e. `{IsConnected:True}`.
  **Needed change:** return `{IsConnected:True|False}` and correct the header comment. Check: "cna_gamepad_state_copy_string
  of a connected snapshot".
- **`cna_achievement_collection_get_by_key` answers `CNA_RESULT_INTERNAL` for an unknown key.** The header documents
  `CNA_RESULT_INVALID_ARGUMENT`; `AchievementCollectionGetByKey` (`modules/c-api/src/CnaCApiAchievements.cpp`) indexes
  `AchievementCollection::operator[](const std::string&)`, which throws `System::IndexOutOfRangeException`, and the barrier
  reports that as an internal fault ("Index was outside the bounds of the array."). **Needed change:** look the key up
  first and return `CNA_RESULT_INVALID_ARGUMENT` when it is absent (or map `IndexOutOfRangeException` from that indexer
  to `INVALID_ARGUMENT`). Check: "cna_achievement_collection_get_by_key with a key no achievement has".
- **`cna_presentation_parameters_init` is not XNA's default.** Documented as "XNA-compatible defaults", it returns an
  800x480 windowed back buffer (`PresentationParameters::PresentationParameters()` in
  `modules/graphics/src/Xna/PresentationParameters.cpp`); XNA 4.0's constructor sets only `IsFullScreen = true` and leaves
  the back buffer 0x0 (the 800x480 belongs to `GraphicsDeviceManager`'s defaults). **Needed change:** `isFullScreen_(true)`
  and a 0x0 back buffer in the constructor, or document the desktop defaults as CNA's own instead of XNA-compatible.
  Check (game probe): "cna_presentation_parameters_init (documented as XNA-compatible defaults), cna_presentation_parameters_clone".
- **A missing content asset leaves no canonical exception type.** `core.h` names
  `Microsoft.Xna.Framework.Content.ContentLoadException` as the example `cna_error_copy_last_exception_type_ext` reports,
  but the content loaders in `modules/c-api/src/CnaCApiContent.cpp` catch `ContentLoadException` themselves and return
  `IoError(exception.what())`, so after `cna_content_manager_load_texture2d` of a missing asset the type is empty.
  **Needed change:** record the canonical type in those catch blocks (an `IoError` overload taking the exception and
  storing `CanonicalTypeName(exception)`, or let the exception reach the barrier, which maps it to `CNA_RESULT_IO`).
  Check (game probe): "cna_error_copy_last_exception_type_ext after cna_content_manager_load_texture2d of a missing asset".

Not counted as failures (the checks follow CNA's own documentation): `devices.h` says "supported physical host targets
report `CNA_DEVICE_TYPE_DEVICE`", while `Microsoft/Devices/Environment.hpp` and the implementation report `Emulator` on
every desktop and browser target (the museum checks EMULATOR on Linux; the C header sentence should say desktops report
`EMULATOR`). `cna_plane_get_hash_code` (and the other bounding-volume hashes) use CNA's own `size_t` combination rather
than XNA's sum of float bit patterns, and `cna_mouse_state_get_hash_code` mixes only position and wheel as `input_mouse.h`
documents, where XNA also XORs the button states; the checks test the hash contract (equal values, equal hashes) instead.

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

## Fixed in CNA after the museum reported them
- StorageContainer members after `Dispose` now throw `ObjectDisposedException` as in XNA (CNA `5865ae938`); verified by
  `tests/scenarios/storage_contracts.json` (FileExists, CreateFile and GetFileNames after Dispose: PASS).
See the "CNA findings fixed in CNA" list in the project memory and `plan.md`; entries move here when their fix is
verified by a museum run.
