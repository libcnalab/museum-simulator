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
- Every member after `Dispose`: XNA `ObjectDisposedException` (`VerifyNotDisposed`), CNA keeps working (it even creates files).
- **Needed change:** a `VerifyNotDisposed()` at the top of each container member, an end-once flag on both result types,
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
- `MeshHelper.CalculateNormals` sums the normalised normal of every triangle at a position (a quad split into two
  triangles counts twice at the corners its diagonal touches), exactly like XNA.

## Fixed in CNA after the museum reported them
See the "CNA findings fixed in CNA" list in the project memory and `plan.md`; entries move here when their fix is
verified by a museum run.
