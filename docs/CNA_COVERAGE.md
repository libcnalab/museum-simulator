# CNA coverage

Which parts of CNA the museum demonstrates, which it does not, and why. The table at the end is **generated**
(`tools/gen_docs.sh`) from the `apis` list of every room in `src/Catalogue/`; a room only claims an API it actually
calls. Rooms are labelled **XNA 4.0** (a feature XNA 4.0 has) or **CNA extension** (CNAEXT: not in XNA 4.0).

## Coverage audit (hand-written, updated 2026-10-14)

| Area | Where | State |
|---|---|---|
| Game, GameTime, components, GameServiceContainer, content, viewport | floor 1 *Application Model* | covered |
| Vector, Matrix (billboards, reflection, normal matrix, off-centre projections), Quaternion, Curve, barycentric, projection, Viewport.Project/Unproject | floor 1 | covered |
| BoundingBox/Sphere/Frustum, Ray, Plane, picking | floor 1 *Collision* | covered (`BoundingFrustum.Intersects(Ray)` throws NotImplemented in CNA and is avoided) |
| Cameras: orbit, chase, top-down, security, FOV, clip, transition, split, mirror | floor 1 *Cameras* | covered |
| SpriteBatch (blend, sort, sampler, scissor, camera matrix, animation by SourceRectangle, per-pixel collision), SpriteFont | floor 2 | covered |
| VertexBuffer, IndexBuffer (16/32 bit), primitive types, DynamicVertexBuffer, user primitives, instancing, heightmap terrain | floor 3 | covered |
| Texture2D (mips, filter, address, formats, dynamic, SaveAsPng/SaveAsJpeg/FromStream), TextureCube, Texture3D | floor 3 | covered |
| BasicEffect (all features), AlphaTestEffect, DualTextureEffect, EnvironmentMapEffect, SkinnedEffect, Effect object model (parameters, techniques, IEffect interfaces, Clone) | floors 4 and 6 | covered |
| Punctual point/spot lights (CNAEXT) | floor 4 | covered |
| Shadow mapping with `IShadowReceiverEXT`, PCF filtering and cascaded shadow maps (`ShadowCascadeStateEXT`) | floor 8 | covered |
| ShaderEffect (GLSL source at run time; twelve fragment/vertex shaders), compile errors as `ShaderDiagnosticEXT` records, `parseCompilerLog` | floor 4 *Shader Lab*, *Shader Compile Errors* | covered; compiled `.fxb` effects have their own room (FNA3D only) |
| RasterizerState, DepthStencilState (incl. stencil), BlendState, SamplerState | floor 5 | covered |
| RenderTarget2D, RenderTargetCube, MRT, float formats, mirrors, multisampling, OcclusionQuery, stencil counting and two-sided stencil | floor 5 | covered |
| glTF models, PbrEffect, image-based lighting, morph targets, material variants, rigid and skinned animation, instancing, import report | floor 6 | covered |
| Particle systems (application code on CNA dynamic buffers) | floor 6 | covered |
| SoundEffect, instances, 3D audio, DynamicSoundEffectInstance, Song/MediaPlayer, Microphone, XACT (AudioEngine, WaveBank, SoundBank, Cue, AudioCategory) | floor 7 | covered (the XACT files are written by `tools/gen_xact.py`: the binary formats need no Microsoft tool) |
| Keyboard, Mouse (relative mode, cursors), GamePad (+ keyboard emulation), PlayerIndex slots, TextInputEXT, Clipboard, Joysticks, TouchPanel gestures, devices | floor 7 | covered; every room works with no pad, joystick or touch screen |
| ContentManager (cache, Unload, loose formats, failures, custom ContentTypeReader for .xnb), ResourceContentManager, cna-content (.cnb/.xnb), TitleContainer, StorageDevice | floor 8 | covered |
| DebugDraw, CRTEffect, DepthEffect, AsciiPostProcessEffect, ShaderPackageEXT (CNA_CNAEXT) | floor 8 | covered |
| Diagnostics counters and resource list (CNA_DIAGNOSTICS=STATS) | floors 8-9 | covered |
| CNA_DEVICES: SystemInfo, Locale, PowerInfo, DisplayInfo | floor 8 | covered |
| Renderer capability profile and report text, platform capabilities and `IPlatform`, GraphicsAdapter and device-type flags, `CNA::Logger` (levels, categories, sink), renderer selection and fallback | floor 8 | covered |
| XACT variables and RPC curves (`Cue.SetVariable`, `AudioEngine.SetGlobalVariable`), cue flags | floor 7 | covered (`tools/gen_xact.py` writes the curves) |
| Type converters of the Design library (Vector2/3/4, Quaternion, Color, Rectangle, Point, BoundingBox, BoundingSphere, Plane, Ray, Matrix), all 141 named colours | floor 1 | covered |
| Per-part glTF sampler state and topology (`ModelMeshPart`), applied by the application | floor 6 | covered |
| Renderer self-checks (50 pixel checks) | floor 9 | covered |
| Time of day, sky dome | floors 0 and 9 | application code |

### Gaps (not demonstrated yet) and non-goals

| Feature | Status |
|---|---|
| haptic effects (devices are listed, none played) | planned: see HANDOFF.md |
| `MediaLibrary`, albums, playlists, pictures (`Microsoft.Xna.Framework.Media`) | deliberately not demonstrated: CNA builds them by scanning the visitor's own Music and Pictures folders, and the museum does not read personal files (the only way to point it elsewhere is a test-only hook inside CNA) |
| GamerServices voice chat, a shared party between three or more accounts, host migration | not demonstrated: the *Online Service* wing needs the sibling CNA gamer-services server (`tools/online_stack.sh`); it demonstrates accounts, friends, presence, messages, parties, online leaderboards, PlayerMatch sessions with find, join and invitations between two accounts. Voice needs a microphone and a second machine |
| `FileDialog`, `MessageBox`, `SystemTray`, `UrlLauncher`, `Camera` (CNA_DEVICES) | not implemented: they open real dialogs or hardware; the room reports whether each is supported |
| C API | not implemented: disabled in CNA's default build, and the museum is a C++ program |
| Inspector (`CNA::Inspector::Agent`, `Client`) | `pl-inspector`: the museum builds CNA's optional Inspector module (`MUSEUM_INSPECTOR`, default ON) and the room starts the agent on a button press and plays the client in the same process. The separate `cna-inspector` bridge and browser page are not exercised |
| Compiled XNA effects (`.fxb`) | demonstrated in *Compiled XNA Effects*, which runs a real XNA-compiled effect on FNA3D (the only renderer with `CompiledEffects`) and says so elsewhere |
| Retired renderers and removed CNA features (for example VULKAN, the post-process chain) | deliberately not demonstrated |

## Generated: API names claimed by rooms

<!-- BEGIN COVERAGE -->
1459 distinct API names are claimed by at least one room.

| API | Rooms |
|---|---|
| Accelerometer | `in-phone` |
| Accelerometer.IsSupported | `in-phone` |
| Accelerometer.KeyboardEmulationEnabledEXT | `in-phone` |
| Accelerometer.Start | `in-phone` |
| Accelerometer.State | `in-phone` |
| Accelerometer.Stop | `in-phone` |
| AccelerometerReading.Acceleration | `in-phone` |
| Achievement | `gs-achievements` |
| AchievementCollection | `gs-achievements` |
| ActivatedEventArgs | `pl-window` |
| Agent.GetAuthenticationToken | `pl-inspector` |
| Agent.GetPort | `pl-inspector` |
| Agent.Start | `pl-inspector` |
| Agent.Stop | `pl-inspector` |
| AgentConfiguration | `pl-inspector` |
| Album.Artist | `dev-medialib` |
| Album.Duration | `dev-medialib` |
| Album.HasArt | `dev-medialib` |
| Album.Name | `dev-medialib` |
| Album.Songs | `dev-medialib` |
| AlbumCollection | `dev-medialib` |
| AlphaModeEXT | `model-material` |
| AlphaTestEffect | `tex-alpha`, `rv-effects` |
| AnimationClipEXT | `model-skinext` |
| AnimationPlayer | `model-skinned` |
| ApplyBindPoseBoneTransformsEXT | `model-skinned` |
| ApplyClipToBonesEXT | `model-rigid` |
| Artist.Albums | `dev-medialib` |
| Artist.Name | `dev-medialib` |
| Artist.Songs | `dev-medialib` |
| ArtistCollection | `dev-medialib` |
| AsciiQuantizeMode | `cx-ascii` |
| AttitudeReading | `in-phone` |
| Audio::AudioFormat | `ct-audiopipeline` |
| Audio::AudioFormat.BitsPerSample | `ct-audiopipeline` |
| Audio::AudioFormat.ChannelCount | `ct-audiopipeline` |
| Audio::AudioFormat.NativeWaveFormat | `ct-audiopipeline` |
| Audio::AudioFormat.SampleRate | `ct-audiopipeline` |
| AudioCategory.Pause | `au-xact` |
| AudioCategory.SetVolume | `au-xact` |
| AudioChannels | `au-synth`, `au-generate` |
| AudioContent | `ct-audiopipeline` |
| AudioContent.Data | `ct-audiopipeline` |
| AudioContent.Format | `ct-audiopipeline` |
| AudioEmitter | `audio`, `au-spatial` |
| AudioEngine | `au-xact` |
| AudioEngine.GetCategory | `au-xact` |
| AudioEngine.GetGlobalVariable | `au-xact` |
| AudioEngine.RendererDetails | `au-xact` |
| AudioEngine.SetGlobalVariable | `au-xact` |
| AudioEngine.Update | `au-xact` |
| AudioListener | `audio`, `au-spatial` |
| AudioStopOptions | `au-xact` |
| AvailableNetworkSession | `net-session` |
| AvailableNetworkSession.CurrentGamerCount | `net-session` |
| AvailableNetworkSession.HostGamertag | `net-session` |
| AvailableNetworkSession.OpenPrivateGamerSlots | `net-session` |
| AvailableNetworkSession.OpenPublicGamerSlots | `net-session` |
| AvailableNetworkSession.QualityOfService | `net-session` |
| AvailableNetworkSession.SessionProperties | `net-session` |
| AvailableNetworkSessionCollection | `net-session` |
| AvailableNetworkSessionCollection.IsDisposed | `net-session` |
| AvatarAnimation | `gs-avatar` |
| AvatarAnimationPreset | `gs-avatar` |
| AvatarBodyType | `gs-avatar` |
| AvatarBone | `gs-avatar` |
| AvatarDescription | `gs-avatar` |
| AvatarDescription.BeginGetFromGamer | `gs-avatar` |
| AvatarDescription.CreateRandom | `gs-avatar` |
| AvatarExpression | `gs-avatar` |
| AvatarEye | `gs-avatar` |
| AvatarEyebrow | `gs-avatar` |
| AvatarMouth | `gs-avatar` |
| AvatarRenderer | `gs-avatar` |
| AvatarRendererState | `gs-avatar` |
| BasicEffect | `lighting`, `light-types`, `light-falloff`, `model-effects`, `rv-effects` |
| BasicEffect.Alpha | `be-combos`, `be-alpha` |
| BasicEffect.AmbientLightColor | `be-lights` |
| BasicEffect.DiffuseColor | `be-combos` |
| BasicEffect.DirectionalLight0 | `be-lights`, `rv-shading` |
| BasicEffect.EmissiveColor | `grounds-sculpture`, `be-material`, `light-emissive`, `rf-deck` |
| BasicEffect.EnableDefaultLighting | `be-lights` |
| BasicEffect.FogColor | `be-fog` |
| BasicEffect.FogEnabled | `be-fog`, `rv-shading`, `rf-telescope` |
| BasicEffect.FogEnd | `be-fog` |
| BasicEffect.FogStart | `be-fog` |
| BasicEffect.LightViewProjectionEXT | `cx-shadow` |
| BasicEffect.LightingEnabled | `be-lights` |
| BasicEffect.PreferPerPixelLighting | `lighting`, `be-perpixel` |
| BasicEffect.ShadowCascadesEXT | `cx-shadow` |
| BasicEffect.ShadowDepthBiasEXT | `cx-shadow` |
| BasicEffect.ShadowFilterRadiusEXT | `cx-shadow` |
| BasicEffect.ShadowMapEXT | `cx-shadow` |
| BasicEffect.SpecularColor | `be-material` |
| BasicEffect.SpecularPower | `be-material` |
| BasicEffect.Texture | `pf-state` |
| BasicEffect.TextureEnabled | `geo-vertextypes`, `be-combos` |
| BasicEffect.VertexColorEnabled | `geo-vertextypes`, `be-combos`, `cx-shadow`, `rv-shading` |
| BasicEffect.World | `grounds-sculpture`, `rv-camera` |
| BasicEffect.setPunctualLightEXT | `be-punctual` |
| BasicMaterialContent | `ct-modelimport` |
| BasicMaterialContent.DiffuseColor | `ct-modelimport` |
| Bgr565 | `vt-packed` |
| Bgra4444 | `vt-packed` |
| Bgra5551 | `vt-packed` |
| BitmapContent | `ct-processor` |
| Blend | `sprite-blend`, `rs-blend` |
| Blend.BlendFactor | `rs-blendfactor`, `rv-blend` |
| Blend.DestinationColor | `sprite-lights` |
| Blend.InverseBlendFactor | `rs-blendfactor` |
| BlendFunction | `sprite-blend`, `rs-blend`, `rv-blend` |
| BlendFunction.ReverseSubtract | `rt-bloom` |
| BlendState | `sky`, `renderstates`, `particles`, `sprite-blend`, `rs-blend`, `rv-blend` |
| BlendState.Additive | `sprite-blend`, `sprite-lights`, `light-coloured`, `light-many`, `light-emissive`, `rt-bloom`, `pt-fire`, `pt-smoke`, `pt-sparks`, `pt-snow`, `pt-rain`, `pt-fountain`, `pt-dust`, `pt-fireworks`, `pt-bubbles`, `pt-leaves`, `pt-magic` |
| BlendState.AlphaBlend | `sprite-blend` |
| BlendState.BlendFactor | `rs-blendfactor` |
| BlendState.ColorWriteChannels | `rs-colorwrite` |
| BlendState.ColorWriteChannels1 | `rt-mrt` |
| BlendState.ColorWriteChannels2 | `rt-mrt` |
| BlendState.ColorWriteChannels3 | `rt-mrt` |
| BlendState.MultiSampleMask | `rt-msaa` |
| BlendState.NonPremultiplied | `grounds-fountain`, `sprite-tint`, `tex-alpha`, `be-alpha`, `hist-compare`, `pf-overdraw`, `rf-garden` |
| BoneContent | `wk-meshhelper` |
| BoneTrackEXT | `model-skinext` |
| BoundingBox | `col-ray-box`, `col-pick`, `an-conveyor`, `sim-balls` |
| BoundingBox.Contains | `col-box-box` |
| BoundingBox.CreateFromPoints | `col-merge` |
| BoundingBox.CreateFromSphere | `col-merge` |
| BoundingBox.Intersects | `col-box-box` |
| BoundingBoxConverter | `vt-converters` |
| BoundingFrustum | `mat-view`, `col-frustum`, `cam-orbit`, `cam-security`, `model-cameras` |
| BoundingFrustum.Contains | `mat-perspective`, `col-frustum`, `model-instances` |
| BoundingFrustum.GetCorners | `col-frustum` |
| BoundingSphere | `col-ray-sphere`, `col-pick` |
| BoundingSphere.Contains | `col-sphere-box` |
| BoundingSphere.CreateFromBoundingBox | `col-merge` |
| BoundingSphere.CreateFromFrustum | `col-frustum` |
| BoundingSphere.CreateFromPoints | `col-merge` |
| BoundingSphere.CreateMerged | `col-merge` |
| BoundingSphere.Intersects | `col-sphere-box`, `sim-balls` |
| BoundingSphere.Transform | `model-bounds` |
| BoundingSphereConverter | `vt-converters` |
| ButtonState | `in-mouse` |
| Buttons | `in-pad` |
| Buttons.BigButton | `in-pad` |
| Buttons.LeftThumbstickUp | `in-pad` |
| Buttons.RightThumbstickLeft | `in-pad` |
| Byte4 | `vt-packed` |
| CNA::DesktopOS | `dev-identity` |
| CNA::Devices::Camera | `dev-camera` |
| CNA::Devices::DisplayInfo | `cx-devices` |
| CNA::Devices::FileDialog | `dev-dialogs` |
| CNA::Devices::FileDialogFilter | `dev-dialogs` |
| CNA::Devices::Locale | `cx-devices` |
| CNA::Devices::MessageBox | `dev-dialogs` |
| CNA::Devices::PowerInfo | `cx-devices` |
| CNA::Devices::SystemInfo | `cx-devices` |
| CNA::Devices::SystemTray | `dev-tray` |
| CNA::Devices::UrlLauncher | `dev-tray` |
| CNA::Diagnostics::CounterHandle | `pl-diagnostics` |
| CNA::Diagnostics::EventBatch | `pl-diagnostics` |
| CNA::Diagnostics::EventRecord | `pl-diagnostics` |
| CNA::Diagnostics::FrameCounterHandle | `pl-diagnostics` |
| CNA::Diagnostics::FrameStatisticsSink | `pl-diagnostics` |
| CNA::Diagnostics::GaugeHandle | `pl-diagnostics` |
| CNA::Diagnostics::GetProvider | `cx-stats`, `pl-diagnostics`, `pf-dashboard`, `pf-state`, `pf-targets` |
| CNA::Diagnostics::IDiagnosticsProvider.CaptureSnapshot | `pf-dashboard`, `pf-state`, `pf-targets` |
| CNA::Diagnostics::IDiagnosticsSource | `pl-diagnostics` |
| CNA::Diagnostics::MarkEvent | `pl-diagnostics` |
| CNA::Diagnostics::MetricKind | `pl-diagnostics` |
| CNA::Diagnostics::MetricSample.name | `pf-dashboard` |
| CNA::Diagnostics::MetricSample.value | `pf-dashboard` |
| CNA::Diagnostics::NameHandle | `pl-diagnostics` |
| CNA::Diagnostics::RegisterSource | `pl-diagnostics` |
| CNA::Diagnostics::ResourceDescriptor | `pl-diagnostics` |
| CNA::Diagnostics::ResourceHandle | `pl-diagnostics` |
| CNA::Diagnostics::ResourceKind | `pl-diagnostics` |
| CNA::Diagnostics::SetRuntimeMode | `pl-diagnostics` |
| CNA::Diagnostics::Snapshot.metrics | `pf-dashboard`, `pf-state`, `pf-targets` |
| CNA::Diagnostics::SourceRegistration | `pl-diagnostics` |
| CNA::Diagnostics::StartRecording | `pl-diagnostics` |
| CNA::Diagnostics::StopRecording | `pl-diagnostics` |
| CNA::Diagnostics::Trace | `pl-diagnostics` |
| CNA::Diagnostics::ZoneScope | `pl-diagnostics` |
| CNA::GamerServices::resolveConfiguration | `on-account` |
| CNA::Graphics::AsciiPostProcessEffect | `cx-ascii` |
| CNA::Graphics::CRTEffect | `cx-crt` |
| CNA::Graphics::DebugDraw | `cx-debugdraw` |
| CNA::Graphics::DepthEffect | `cx-depth` |
| CNA::Graphics::ShaderPackageEXT | `cx-package` |
| CNA::Graphics::VertexTypeRegistryEXT | `rv-vertextype` |
| CNA::GraphicsRendererCategory | `dev-identity` |
| CNA::GraphicsRendererFallbackReason | `hist-cna` |
| CNA::GraphicsRendererSelection.GetActive | `hist-cna` |
| CNA::GraphicsRendererSelection.GetAvailable | `hist-cna` |
| CNA::GraphicsRendererSelection.GetFallbackHistory | `hist-cna` |
| CNA::GraphicsRendererSelection.GetSelected | `hist-cna` |
| CNA::GraphicsRendererSelection.IsFallbackEnabled | `hist-cna` |
| CNA::Inspector::Agent | `pl-inspector` |
| CNA::Inspector::Client | `pl-inspector` |
| CNA::LogCategory | `pl-log` |
| CNA::LogLevel | `pl-log` |
| CNA::Logger | `pl-log` |
| CNA::Platform::IPlatform | `pl-platform` |
| CNA::Platform::NativeWindowHandle | `pl-window` |
| CNA::ShaderDiagnosticEXT | `fx-errors` |
| CNA::TargetPlatform | `dev-identity` |
| CNA::Unsupported3DGraphicsCallBehavior | `dev-identity` |
| CNA::getCurrentDesktopOS | `dev-identity` |
| CNA::getCurrentGraphicsRendererCategory | `dev-identity` |
| CNA::getCurrentGraphicsRendererType | `dev-identity` |
| CNA::getCurrentPlatform | `dev-identity` |
| CNA::getCurrentPlatformName | `dev-identity` |
| CNA::getGraphicsRendererCategory | `dev-identity` |
| CNA::getGraphicsRendererMaturity | `hist-cna` |
| CNA::getGraphicsRendererName | `dev-identity`, `hist-cna` |
| CNA::isApplePlatform | `dev-identity` |
| CNA::isMobilePlatform | `dev-identity` |
| CRTMaskType | `cx-crt` |
| Camera.AvailableCameras | `dev-camera` |
| Camera.FrameHeight | `dev-camera` |
| Camera.FrameWidth | `dev-camera` |
| Camera.IsSupported | `dev-camera` |
| Camera.State | `dev-camera` |
| Camera.TryAcquireFrame | `dev-camera` |
| CameraDeviceInfo | `dev-camera` |
| CameraPosition | `dev-camera` |
| CameraState | `dev-camera` |
| Capability | `pl-inspector` |
| Client.CaptureSnapshot | `pl-inspector` |
| Client.Connect | `pl-inspector` |
| Client.Ping | `pl-inspector` |
| Client.ReadEvents | `pl-inspector` |
| Client.RequestPreview | `pl-inspector` |
| Clipboard.GetMimeTypesEXT | `in-text` |
| Clipboard.GetPrimarySelectionTextEXT | `in-text` |
| Clipboard.GetTextEXT | `in-text` |
| Clipboard.HasPrimarySelectionTextEXT | `in-text` |
| Clipboard.SetDataEXT | `in-text` |
| Clipboard.SetPrimarySelectionTextEXT | `in-text` |
| Clipboard.SetTextEXT | `in-text` |
| ClosingEventArgs | `pl-window` |
| Color | `vt-color`, `vt-colors`, `sprite-tint` |
| Color.A | `vt-colors` |
| Color.AliceBlue | `vt-colors` |
| Color.B | `vt-colors` |
| Color.Black | `vt-colors` |
| Color.Blue | `vt-colors` |
| Color.CornflowerBlue | `vt-colors` |
| Color.FromNonPremultiplied | `vt-color` |
| Color.G | `vt-colors` |
| Color.Gold | `vt-colors` |
| Color.Green | `vt-colors` |
| Color.Lerp | `vt-color`, `sprite-tint` |
| Color.Multiply | `vt-color` |
| Color.PackFromVector4 | `vt-color` |
| Color.PackedValue | `vt-color`, `vt-colors` |
| Color.R | `vt-colors` |
| Color.Red | `vt-colors` |
| Color.ToString | `vt-colors` |
| Color.ToVector3 | `vt-color` |
| Color.Transparent | `vt-colors` |
| Color.White | `vt-colors` |
| Color.YellowGreen | `vt-colors` |
| ColorConverter | `vt-converters` |
| ColorMatrixEffect | `fx-colormatrix` |
| ColorMatrixEffect.SetColorMatrix | `fx-colormatrix` |
| ColorMatrixEffect.SetColorOffset | `fx-colormatrix` |
| ColorWriteChannels | `rs-portal`, `rs-colorwrite`, `rv-blend` |
| CompareFunction | `tex-alpha`, `rs-depthfunc`, `rv-depth` |
| CompareFunction.NotEqual | `rs-outline` |
| Compass | `in-phone` |
| Compass.IsSupported | `in-phone` |
| CompassReading.MagneticHeading | `in-phone` |
| CompileOptions | `ct-compiler` |
| CompileOptions.compressContent | `ct-compiler` |
| CompileOptions.targetPlatform | `ct-compiler` |
| CompileOptions.targetProfile | `ct-compiler` |
| CompiledEffectContent | `fx-compiled` |
| CompiledEffectContent.GetEffectCode | `fx-compiled` |
| ContainmentType | `col-box-box` |
| ContentCompiler | `ct-compiler` |
| ContentCompiler.Compile | `ct-compiler` |
| ContentCompiler.Compile(CompiledEffectContent) | `fx-compiled` |
| ContentCompiler.Compile(SongContent) | `ct-audiopipeline` |
| ContentCompiler.Compile(SoundEffectContent) | `ct-audiopipeline` |
| ContentCompiler.Compile(VideoContent) | `au-video` |
| ContentCompiler.GetTypeWriter | `ct-compiler` |
| ContentIdentity | `ct-fontbuild` |
| ContentImporterAttribute | `ct-intermediate`, `ct-modelimport`, `ct-audiopipeline` |
| ContentItem | `ct-intermediate` |
| ContentLoadException | `app-content`, `ct-failure`, `ct-custom`, `ct-resource`, `pl-exceptions` |
| ContentManager | `models`, `app-content` |
| ContentManager.GetContentManifest | `ct-pipeline` |
| ContentManager.Load | `app-content`, `ct-cache`, `ct-failure`, `ct-pipeline`, `ct-reader`, `ct-resource` |
| ContentManager.Load<Effect> | `fx-compiled` |
| ContentManager.Load<Model> | `model-load`, `ct-load` |
| ContentManager.Load<SoundEffect> | `au-basics`, `ct-load` |
| ContentManager.Load<Texture2D> | `ct-load`, `ct-formats` |
| ContentManager.Load<Video> | `au-video` |
| ContentManager.OpenStream | `ct-compiler` |
| ContentManager.ReadAsset | `ct-compiler` |
| ContentManager.RegisterCnjLoader | `ct-custom` |
| ContentManager.RegisterTypeReader | `ct-custom` |
| ContentManager.RootDirectory | `ct-load` |
| ContentManager.RouteLoadsThroughOpenStreamEXT | `ct-compiler` |
| ContentManager.ServiceProvider | `ct-load` |
| ContentManager.Unload | `app-content`, `ct-cache`, `ct-lifetime` |
| ContentProcessorContext.Convert | `ct-modelimport` |
| ContentReader | `ct-reader` |
| ContentReader.AssetName | `ct-reader` |
| ContentReader.ContentManager | `ct-reader` |
| ContentReader.Platform | `ct-reader` |
| ContentReader.ReadBoundingSphere | `ct-reader` |
| ContentReader.ReadColor | `ct-reader` |
| ContentReader.ReadMatrix | `ct-reader` |
| ContentReader.ReadQuaternion | `ct-reader` |
| ContentReader.ReadVector2 | `ct-reader` |
| ContentReader.ReadVector3 | `ct-reader` |
| ContentReader.ReadVector4 | `ct-reader` |
| ContentReader.Version | `ct-reader` |
| ContentSerializerAttribute | `ct-intermediate` |
| ContentSerializerCollectionItemNameAttribute | `ct-intermediate` |
| ContentSerializerCollectionItemNameAttribute.CollectionItemName | `ct-intermediate` |
| ContentTypeDescriptor | `ct-intermediate` |
| ContentTypeReader | `ct-reader` |
| ContentTypeReaderManager.AddTypeCreator | `ct-reader` |
| ContentTypeReaderManager.GetTypeReader | `ct-reader` |
| ConversionQuality | `ct-audiopipeline` |
| CubeMapFace | `tex-cube`, `rt-cube` |
| Cue.GetVariable | `au-xact` |
| Cue.IsCreated | `au-xact` |
| Cue.IsPreparing | `au-xact` |
| Cue.IsStopping | `au-xact` |
| Cue.Pause | `au-xact` |
| Cue.Play | `au-xact` |
| Cue.Resume | `au-xact` |
| Cue.SetVariable | `au-xact` |
| Cue.Stop | `au-xact` |
| CullMode | `rs-cull` |
| Curve | `curve-class` |
| Curve.ComputeTangent | `curve-class` |
| Curve.ComputeTangents | `curve-class` |
| Curve.IsConstant | `curve-class` |
| Curve.PostLoop | `curve-class` |
| Curve.PreLoop | `curve-class` |
| CurveContinuity | `curve-class` |
| CurveKey | `curve-class` |
| CurveKey.TangentIn | `curve-class` |
| CurveKey.TangentOut | `curve-class` |
| CurveKeyCollection | `curve-class` |
| CurveLoopType | `curve-class` |
| CurveTangent | `curve-class` |
| DeactivatedEventArgs | `pl-window` |
| DebugDraw.addBox | `cx-debugdraw` |
| DebugDraw.addFrustum | `cx-debugdraw` |
| DebugDraw.addSphere | `cx-debugdraw` |
| DebugDraw.setDepthTested | `cx-debugdraw` |
| DepthEffectMode | `cx-depth` |
| DepthFormat | `cam-clip`, `rt-basic` |
| DepthFormat.Depth24 | `rv-depth` |
| DepthStencilState | `sky`, `renderstates`, `cam-clip`, `rv-depth` |
| DepthStencilState.CounterClockwiseStencilFunction | `rs-stencilcount` |
| DepthStencilState.CounterClockwiseStencilPass | `rs-stencilcount` |
| DepthStencilState.DepthBufferEnable | `rs-depthfunc` |
| DepthStencilState.DepthBufferFunction | `light-many`, `rs-depthfunc`, `rs-stencilcount` |
| DepthStencilState.DepthBufferWriteEnable | `rs-depthread` |
| DepthStencilState.DepthRead | `be-alpha`, `rs-depthread` |
| DepthStencilState.ReferenceStencil | `rs-outline` |
| DepthStencilState.StencilDepthBufferFail | `rs-stencilcount` |
| DepthStencilState.StencilEnable | `rs-portal` |
| DepthStencilState.StencilFail | `rs-stencilcount` |
| DepthStencilState.StencilFunction | `rs-portal` |
| DepthStencilState.StencilMask | `rs-stencilcount` |
| DepthStencilState.StencilPass | `rs-outline`, `rs-shadow`, `rs-stencilcount` |
| DepthStencilState.StencilWriteMask | `rs-stencilcount` |
| DepthStencilState.TwoSidedStencilMode | `rs-stencilcount` |
| DeviceType | `in-phone` |
| DirectionalLight | `lighting`, `be-lights`, `light-types` |
| DitherMode | `cx-depth` |
| DrawableGameComponent | `app-components` |
| DualTextureEffect | `be-dual`, `rv-effects` |
| DualTextureEffect.Texture2 | `be-dual` |
| DynamicIndexBuffer | `geo-dynindex` |
| DynamicSoundEffectInstance | `au-synth` |
| DynamicSoundEffectInstance.BufferNeeded | `au-synth` |
| DynamicSoundEffectInstance.GetSampleSizeInBytes | `au-synth` |
| DynamicSoundEffectInstance.PendingBufferCount | `au-synth` |
| DynamicSoundEffectInstance.SubmitBuffer | `au-synth` |
| DynamicSoundEffectInstance.SubmitFloatBufferEXT | `au-synth` |
| DynamicVertexBuffer | `grounds-fountain`, `particles`, `geo-dynamic`, `pt-fire`, `pt-smoke`, `pt-sparks`, `pt-snow`, `pt-rain`, `pt-fountain`, `pt-dust`, `pt-fireworks`, `pt-bubbles`, `pt-leaves`, `pt-magic`, `cx-shadow`, `rf-garden`, `rf-weather` |
| DynamicVertexBuffer(GraphicsDevice, Type, int, BufferUsage) | `rv-vertextype` |
| DynamicVertexBuffer.SetData | `pf-buffers` |
| Effect | `be-model`, `fx-compiled` |
| Effect(GraphicsDevice, byte[]) | `fx-compiled` |
| Effect.Clone | `be-model` |
| Effect.Parameters | `fx-compiled` |
| Effect.Techniques | `fx-compiled` |
| EffectContent | `fx-compiled` |
| EffectContent.EffectCode | `fx-compiled` |
| EffectImporter | `fx-compiled` |
| EffectImporter.Import | `fx-compiled` |
| EffectParameter | `be-model`, `fx-compiled` |
| EffectParameter.GetValueInt32 | `be-model` |
| EffectParameter.GetValueMatrix | `be-model` |
| EffectParameter.GetValueMatrixArray | `be-model` |
| EffectParameter.GetValueMatrixTranspose | `be-model` |
| EffectParameter.GetValueMatrixTransposeArray | `be-model` |
| EffectParameter.GetValueSingle | `fx-compiled` |
| EffectParameter.GetValueTexture2D | `be-model` |
| EffectParameter.GetValueVector2 | `fx-compiled` |
| EffectParameter.GetValueVector3 | `be-model` |
| EffectParameter.GetValueVector4 | `be-model` |
| EffectParameter.SetValue | `fx-compiled` |
| EffectParameterClass | `be-model`, `fx-compiled` |
| EffectParameterCollection | `be-model` |
| EffectParameterType | `be-model`, `fx-compiled` |
| EffectPass | `be-model`, `fx-compiled` |
| EffectPass.Apply | `pf-state` |
| EffectProcessor | `fx-compiled` |
| EffectProcessor.DebugMode | `fx-compiled` |
| EffectProcessor.Defines | `fx-compiled` |
| EffectProcessor.Process | `fx-compiled` |
| EffectTechnique | `be-model`, `fx-compiled` |
| EnvironmentMapEffect | `tex-cube`, `rt-cube` |
| EnvironmentMapEffect.EnvironmentMap | `rt-cube` |
| EvaluateMorphWeightsEXT | `model-morph` |
| EventsResponse | `pl-inspector` |
| FbxImporter | `ct-modelimport` |
| FileDialog.IsSupported | `dev-dialogs` |
| FileDialog.ResultCallback | `dev-dialogs` |
| FileDialog.ShowOpenFile | `dev-dialogs` |
| FileDialog.ShowOpenFolder | `dev-dialogs` |
| FileDialog.ShowSaveFile | `dev-dialogs` |
| FriendCollection | `on-friends` |
| FriendCollection.IsDisposed | `on-friends` |
| FriendGamer | `on-friends` |
| FriendGamer.IsAway | `on-friends` |
| FriendGamer.IsBusy | `on-friends` |
| FriendGamer.IsJoinable | `on-friends` |
| FriendGamer.IsOnline | `on-friends` |
| FriendGamer.IsPlaying | `on-friends` |
| FriendGamer.Presence | `on-friends` |
| Game | `app-lifecycle` |
| Game.Activated | `pl-window` |
| Game.Components | `app-components` |
| Game.GetPlatformCapabilitiesEXT | `pl-platform` |
| Game.GetPlatformEXT | `pl-platform` |
| Game.InactiveSleepTime | `app-gametime` |
| Game.IsFixedTimeStep | `app-lifecycle` |
| Game.LaunchParameters | `app-lifecycle` |
| Game.ResetElapsedTime | `app-gametime` |
| Game.Services | `app-services` |
| Game.SuppressDraw | `app-gametime` |
| Game.TargetElapsedTime | `app-lifecycle` |
| GameComponent | `app-components` |
| GameComponentCollection.ComponentAdded | `app-components` |
| GameComponentCollection.ComponentRemoved | `app-components` |
| GameComponentCollectionEventArgs | `app-components` |
| GameComponentCollectionEventArgs.GameComponent | `app-components` |
| GameDefaults.AccelerateWithButtons | `gs-players` |
| GameDefaults.AutoCenter | `gs-players` |
| GameDefaults.ControllerSensitivity | `gs-players` |
| GameDefaults.ManualTransmission | `gs-players` |
| GameDefaults.PrimaryColor | `gs-players` |
| GameDefaults.RacingCameraAngle | `gs-players` |
| GamePad.GetButtonLabelEXT | `in-pad` |
| GamePad.GetCapabilities | `in-pad`, `in-players` |
| GamePad.GetConnectionStateEXT | `in-devices` |
| GamePad.GetFirmwareVersionEXT | `in-devices` |
| GamePad.GetGUIDEXT | `in-pad` |
| GamePad.GetGyroEXT | `in-pad` |
| GamePad.GetNameEXT | `in-players` |
| GamePad.GetPlayerIndexEXT | `in-pad` |
| GamePad.GetPowerInfoEXT | `in-devices` |
| GamePad.GetSerialEXT | `in-devices` |
| GamePad.GetState | `in-pad` |
| GamePad.GetTouchpadCountEXT | `in-devices` |
| GamePad.GetTouchpadFingerCountEXT | `in-pad` |
| GamePad.KeyboardEmulationEnabledEXT | `in-pad` |
| GamePad.SetLightBarEXT | `in-pad` |
| GamePad.SetTriggerVibrationEXT | `in-pad` |
| GamePad.SetVibration | `in-pad` |
| GamePadCapabilities | `in-pad`, `in-players` |
| GamePadCapabilities.GamePadType | `in-pad` |
| GamePadCapabilities.HasAButton | `in-pad` |
| GamePadCapabilities.HasBButton | `in-pad` |
| GamePadCapabilities.HasBackButton | `in-pad` |
| GamePadCapabilities.HasBigButton | `in-pad` |
| GamePadCapabilities.HasDPadDownButton | `in-pad` |
| GamePadCapabilities.HasDPadLeftButton | `in-pad` |
| GamePadCapabilities.HasDPadRightButton | `in-pad` |
| GamePadCapabilities.HasDPadUpButton | `in-pad` |
| GamePadCapabilities.HasLeftShoulderButton | `in-pad` |
| GamePadCapabilities.HasLeftStickButton | `in-pad` |
| GamePadCapabilities.HasLeftTrigger | `in-pad` |
| GamePadCapabilities.HasLeftVibrationMotor | `in-pad` |
| GamePadCapabilities.HasLeftXThumbStick | `in-pad` |
| GamePadCapabilities.HasLeftYThumbStick | `in-pad` |
| GamePadCapabilities.HasRightShoulderButton | `in-pad` |
| GamePadCapabilities.HasRightStickButton | `in-pad` |
| GamePadCapabilities.HasRightTrigger | `in-pad` |
| GamePadCapabilities.HasRightVibrationMotor | `in-pad` |
| GamePadCapabilities.HasRightXThumbStick | `in-pad` |
| GamePadCapabilities.HasRightYThumbStick | `in-pad` |
| GamePadCapabilities.HasStartButton | `in-pad` |
| GamePadCapabilities.HasVoiceSupport | `in-pad` |
| GamePadCapabilities.HasXButton | `in-pad` |
| GamePadCapabilities.HasYButton | `in-pad` |
| GamePadCapabilities.IsConnected | `in-pad` |
| GamePadDeadZone | `in-pad` |
| GamePadState | `in-pad` |
| GamePadState.Equals | `in-pad` |
| GamePadState.IsButtonDown | `in-pad` |
| GamePadState.IsButtonUp | `in-pad` |
| GameServiceContainer | `app-services` |
| GameServiceContainer.AddService | `app-services` |
| GameServiceContainer.GetService | `app-services` |
| GameServiceContainer.RemoveService | `app-services` |
| GameTime | `app-lifecycle`, `cam-chase`, `an-pendulum`, `sim-integrators` |
| GameTime.ElapsedGameTime | `app-gametime`, `pf-dashboard` |
| GameTime.IsRunningSlowly | `pf-dashboard` |
| GameTime.TotalGameTime | `app-gametime` |
| GameWindow.AllowUserResizing | `pl-window` |
| GameWindow.ClientBounds | `app-viewport`, `pl-window` |
| GameWindow.ClientSizeChanged | `pl-window` |
| GameWindow.CurrentOrientation | `pl-window` |
| GameWindow.FileDropEXT | `pl-window` |
| GameWindow.GetNativeWindowHandleEXT | `pl-window` |
| GameWindow.MinimizeEXT | `dev-window` |
| GameWindow.OrientationChanged | `pl-window` |
| GameWindow.RestoreEXT | `dev-window` |
| GameWindow.TextDropEXT | `dev-window` |
| GameWindow.Title | `pl-window` |
| Gamer.GetProfile | `gs-players` |
| Gamer.LeaderboardWriter | `gs-leaderboards` |
| Gamer.SignedInGamers | `gs-players` |
| GamerPresenceMode | `gs-players` |
| GamerPrivilegeException | `pl-exceptions` |
| GamerProfile | `gs-players` |
| GamerProfile.GamerScore | `on-account` |
| GamerProfile.GetGamerPicture | `gs-players` |
| GamerProfile.Motto | `gs-players` |
| GamerProfile.Region | `gs-players` |
| GamerProfile.Reputation | `on-account` |
| GamerProfile.TotalAchievements | `on-account` |
| GamerServicesDispatcher.IsInitialized | `gs-guide` |
| GamerServicesDispatcher.WindowHandle | `gs-guide` |
| GamerServicesNotAvailableException | `pl-exceptions` |
| Genre.Name | `dev-medialib` |
| Genre.Songs | `dev-medialib` |
| GenreCollection | `dev-medialib` |
| GeometryContent | `ct-modelimport`, `wk-meshhelper` |
| GeometryContent.Indices | `wk-meshhelper` |
| GeometryContent.Vertices | `wk-meshhelper` |
| GestureSample.Delta2 | `in-touch` |
| GestureSample.Position2 | `in-touch` |
| GestureSample.Timestamp | `in-touch` |
| GestureType | `in-touch` |
| GltfImportDiagnosticEXT | `model-report` |
| GltfImportReportEXT | `model-report` |
| Graphics::DisplayMode | `pl-adapter` |
| Graphics::FontDescription | `ct-fontbuild` |
| Graphics::FontDescription.Characters | `ct-fontbuild` |
| Graphics::FontDescription.DefaultCharacter | `ct-fontbuild` |
| Graphics::FontDescription.FontName | `ct-fontbuild` |
| Graphics::FontDescription.Size | `ct-fontbuild` |
| Graphics::FontDescription.Spacing | `ct-fontbuild` |
| Graphics::FontDescription.Style | `ct-fontbuild` |
| Graphics::FontDescription.UseKerning | `ct-fontbuild` |
| Graphics::FontDescriptionStyle | `ct-fontbuild` |
| GraphicsAdapter | `app-device`, `pl-adapter` |
| GraphicsAdapter.Adapters | `pl-adapter` |
| GraphicsAdapter.GetCurrentDeviceTypeEXT | `pl-adapter` |
| GraphicsAdapter.GetRequiredRendererEXT | `pl-adapter` |
| GraphicsAdapter.IsProfileSupported | `pl-adapter` |
| GraphicsAdapter.IsWideScreenAspectRatioEXT | `pl-adapter` |
| GraphicsAdapter.QueryBackBufferFormat | `pl-adapter` |
| GraphicsAdapter.QueryRenderTargetFormat | `pl-adapter` |
| GraphicsAdapter.UseNullDevice | `pl-adapter` |
| GraphicsAdapter.UseReferenceDevice | `pl-adapter` |
| GraphicsCapability | `app-device` |
| GraphicsCapability.CompiledEffects | `fx-compiled` |
| GraphicsCapability.Instancing | `geo-instancing`, `pf-objects` |
| GraphicsCapability.MultiSampleAntiAliasing | `rt-msaa` |
| GraphicsCapability.OcclusionQuery | `rs-occlusion` |
| GraphicsCapability.WireFrame | `rs-fill` |
| GraphicsDevice | `info`, `app-device` |
| GraphicsDevice.Clear | `rs-stencilcount` |
| GraphicsDevice.DeviceLost | `app-viewport`, `dev-window` |
| GraphicsDevice.DeviceReset | `dev-window` |
| GraphicsDevice.DeviceResetting | `dev-window` |
| GraphicsDevice.DrawIndexedPrimitives | `geo-indexed`, `geo-manybuffers`, `geo-dynindex`, `geo-terrain`, `pf-objects` |
| GraphicsDevice.DrawInstancedPrimitives | `geo-instancing`, `pf-objects` |
| GraphicsDevice.DrawInstancedPrimitivesBaseInstanceEXT | `geo-instancing` |
| GraphicsDevice.DrawPrimitives | `geo-indexed`, `rv-vertextype` |
| GraphicsDevice.DrawUserIndexedPrimitives | `geo-dynamic` |
| GraphicsDevice.DrawUserPrimitives | `geo-triangle`, `geo-primitives`, `cx-points`, `pf-buffers`, `pf-particles`, `rv-geometry` |
| GraphicsDevice.ExecutesShaderEffectSourceEXT | `fx-errors` |
| GraphicsDevice.GetDisplayColorSpaceEXT | `pl-renderer` |
| GraphicsDevice.GetGraphicsRendererName | `pl-renderer` |
| GraphicsDevice.GetGraphicsRendererType | `dev-identity` |
| GraphicsDevice.GetLogicalSizeEXT | `pl-renderer` |
| GraphicsDevice.GetRenderTargets | `rt-mrt`, `rt-cube` |
| GraphicsDevice.GetRendererCapabilityProfileEXT | `pl-renderer` |
| GraphicsDevice.GetRendererCapabilityReportEXT | `pl-renderer` |
| GraphicsDevice.GetShaderDialectEXT | `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-swirl`, `fx-chromatic`, `fx-kernel`, `fx-halftone`, `fx-vertexwave`, `fx-colormatrix`, `fx-errors` |
| GraphicsDevice.GetTrackedResourceCount | `pl-renderer` |
| GraphicsDevice.GetUnsupported3DGraphicsCallBehavior | `dev-identity` |
| GraphicsDevice.GetVertexBuffers | `geo-instancing` |
| GraphicsDevice.GraphicsProfile | `pl-adapter` |
| GraphicsDevice.Present | `app-viewport` |
| GraphicsDevice.ResourceCreated | `ct-lifetime` |
| GraphicsDevice.ResourceDestroyed | `ct-lifetime` |
| GraphicsDevice.SamplerStates | `model-sampler` |
| GraphicsDevice.ScissorRectangle | `sprite-scissor`, `rs-scissor`, `rv-geometry` |
| GraphicsDevice.SetDisplayColorSpaceEXT | `pl-renderer` |
| GraphicsDevice.SetRenderTarget | `rendertarget`, `rt-basic`, `rt-control`, `rt-cube`, `pf-targets`, `rv-targets` |
| GraphicsDevice.SetRenderTargets | `rt-mrt` |
| GraphicsDevice.SetStringMarkerEXT | `cx-points` |
| GraphicsDevice.SetUnsupported3DGraphicsCallBehavior | `dev-identity` |
| GraphicsDevice.SetVertexBuffers | `geo-instancing` |
| GraphicsDevice.SupportsDisplayColorSpaceEXT | `pl-renderer` |
| GraphicsDevice.SupportsImageBasedLightingEXT | `model-ibl`, `pl-renderer` |
| GraphicsDevice.SupportsShaderLanguageEXT | `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-swirl`, `fx-chromatic`, `fx-kernel`, `fx-halftone`, `fx-vertexwave`, `fx-colormatrix` |
| GraphicsDevice.SupportsSurfaceFormatAsRenderTargetEXT | `rt-formats` |
| GraphicsDevice.VertexSamplerStates | `fx-compiled` |
| GraphicsDevice.VertexTextures | `app-viewport`, `fx-compiled` |
| GraphicsDevice.Viewport | `cam-split`, `rv-geometry` |
| GraphicsDeviceManager | `app-viewport` |
| GraphicsDeviceManager.DeviceReset | `app-viewport` |
| GraphicsDeviceManager.DeviceResetting | `app-viewport` |
| GraphicsDeviceManager.PreparingDeviceSettings | `app-viewport` |
| GraphicsDeviceManager.SupportedOrientations | `pl-window` |
| GraphicsDeviceStatus | `app-viewport` |
| GraphicsResource.Dispose | `ct-lifetime` |
| GraphicsResource.Disposing | `ct-lifetime` |
| GraphicsResource.IsDisposed | `ct-cache`, `ct-lifetime` |
| Guide.BeginShowKeyboardInput | `gs-guide` |
| Guide.BeginShowMessageBox | `gs-guide` |
| Guide.DelayNotifications | `gs-guide` |
| Guide.EndShowMessageBox | `gs-guide` |
| Guide.IsScreenSaverEnabled | `gs-guide` |
| Guide.IsTrialMode | `gs-guide` |
| Guide.IsVisible | `gs-guide` |
| Guide.NotificationPosition | `gs-guide` |
| Guide.ShowAchievementsEXT | `on-account` |
| Guide.ShowComposeMessage | `on-messages` |
| Guide.ShowFriendRequest | `on-friends` |
| Guide.ShowFriends | `on-friends` |
| Guide.ShowGamerCard | `on-friends` |
| Guide.ShowMarketplace | `pl-exceptions`, `on-account` |
| Guide.ShowMessages | `on-messages` |
| Guide.ShowParty | `on-party` |
| Guide.ShowPartySessions | `on-party` |
| Guide.ShowPlayerReview | `on-friends` |
| Guide.ShowPlayers | `on-friends` |
| Guide.ShowSignIn | `on-account` |
| Guide.SimulateTrialMode | `gs-guide` |
| GuideAlreadyVisibleException | `pl-exceptions` |
| Gyroscope | `in-phone` |
| Gyroscope.IsSupported | `in-phone` |
| GyroscopeReading.RotationRate | `in-phone` |
| HalfVector2 | `vt-packed` |
| HalfVector4 | `vt-packed` |
| HapticCapabilitiesEXT | `dev-haptics` |
| HapticDevice | `dev-haptics` |
| HapticDevice.CreateEffectEXT | `dev-haptics` |
| HapticDevice.DestroyEffectEXT | `dev-haptics` |
| HapticDevice.Dispose | `dev-haptics` |
| HapticDevice.GetCapabilitiesEXT | `dev-haptics` |
| HapticDevice.GetEffectStatusEXT | `dev-haptics` |
| HapticDevice.GetNameEXT | `dev-haptics` |
| HapticDevice.InitRumbleEXT | `dev-haptics` |
| HapticDevice.IsEffectSupportedEXT | `dev-haptics` |
| HapticDevice.IsOpenEXT | `dev-haptics` |
| HapticDevice.PauseEXT | `dev-haptics` |
| HapticDevice.PlayRumbleEXT | `dev-haptics` |
| HapticDevice.ResumeEXT | `dev-haptics` |
| HapticDevice.RunEffectEXT | `dev-haptics` |
| HapticDevice.SetAutocenterEXT | `dev-haptics` |
| HapticDevice.SetGainEXT | `dev-haptics` |
| HapticDevice.StopAllEffectsEXT | `dev-haptics` |
| HapticDevice.StopEffectEXT | `dev-haptics` |
| HapticDevice.StopRumbleEXT | `dev-haptics` |
| HapticDevice.UpdateEffectEXT | `dev-haptics` |
| HapticEffectEXT | `dev-haptics` |
| HapticEffectTypeEXT | `dev-haptics` |
| HapticInfoEXT | `dev-haptics` |
| Haptics.GetHapticsEXT | `in-devices`, `dev-haptics` |
| Haptics.IsJoystickHapticEXT | `in-devices` |
| Haptics.IsMouseHapticEXT | `in-devices`, `dev-haptics` |
| Haptics.OpenEXT | `dev-haptics` |
| HelloResponse | `pl-inspector` |
| HostChangedEventArgs | `net-session` |
| HostChangedEventArgs.NewHost | `net-session` |
| HostChangedEventArgs.OldHost | `net-session` |
| HttpNotificationChannel | `dev-notify` |
| HttpNotificationChannel.BindToShellToast | `dev-notify` |
| HttpNotificationChannel.ChannelUri | `dev-notify` |
| HttpNotificationChannel.ChannelUriUpdated | `dev-notify` |
| HttpNotificationChannel.Close | `dev-notify` |
| HttpNotificationChannel.DispatchPendingNotificationsEXT | `dev-notify` |
| HttpNotificationChannel.Find | `dev-notify` |
| HttpNotificationChannel.HttpNotificationReceived | `dev-notify` |
| HttpNotificationChannel.IsShellToastBound | `dev-notify` |
| HttpNotificationChannel.Open | `dev-notify` |
| HttpNotificationEventArgs | `dev-notify` |
| HttpNotificationEventArgs.Body | `dev-notify` |
| HttpNotificationEventArgs.BodyAsString | `dev-notify` |
| IAvatarAnimation | `gs-avatar` |
| IDiagnosticsProvider.ReadEvents | `pl-diagnostics` |
| IDiagnosticsProvider.ResolveName | `pl-diagnostics` |
| IDrawable | `app-components` |
| IEffectFog | `be-model` |
| IEffectLights | `be-model` |
| IEffectMatrices | `be-model` |
| IGameComponent | `app-components` |
| IGameComponent.Initialize | `app-components` |
| IGraphicsDeviceService | `app-services` |
| IPackedVector | `vt-packed` |
| IPackedVectorT.PackedValue | `vt-packed` |
| IPlatform.GetKeyboard | `pl-platform` |
| IPlatform.GetName | `pl-platform` |
| IPlatform.GetPerformanceFrequency | `pl-platform` |
| IPlatform.GetTicksMilliseconds | `pl-platform` |
| IPlatform.IsSubsystemInitialized | `pl-platform` |
| IShadowReceiverEXT | `cx-shadow` |
| IUpdateable | `app-components` |
| IVertexType | `rv-vertextype` |
| IVertexType.VertexDeclaration | `rv-vertextype` |
| ImageBasedLightEXT | `model-ibl` |
| IndexBuffer | `particles`, `geo-indexed`, `geo-procedural`, `geo-index32`, `geo-dynindex`, `geo-terrain`, `model-skinfx`, `pt-fire`, `pt-smoke`, `pt-sparks`, `pt-snow`, `pt-rain`, `pt-fountain`, `pt-dust`, `pt-fireworks`, `pt-bubbles`, `pt-leaves`, `pt-magic` |
| IndexElementSize | `geo-index32` |
| InputDevices.GetMiceEXT | `in-devices` |
| IntermediateSerializer | `ct-intermediate` |
| IntermediateSerializer.Deserialize | `ct-intermediate` |
| IntermediateSerializer.GetTypeSerializer | `ct-intermediate` |
| IntermediateSerializer.Serialize | `ct-intermediate` |
| InvalidContentException | `ct-intermediate` |
| InviteAcceptedEventArgs | `net-session` |
| InviteAcceptedEventArgs.Gamer | `net-session` |
| InviteAcceptedEventArgs.IsCurrentSession | `net-session` |
| Joysticks.GetCapabilitiesEXT | `in-joystick` |
| Joysticks.GetJoysticksEXT | `in-joystick` |
| Joysticks.GetStateEXT | `in-joystick` |
| Keyboard.GetKeyFromNameEXT | `in-keyboard` |
| Keyboard.GetKeyFromScancodeEXT | `in-keyboard` |
| Keyboard.GetKeyNameEXT | `in-keyboard` |
| Keyboard.GetModStateEXT | `in-keyboard` |
| Keyboard.GetScancodeFromNameEXT | `in-keyboard` |
| Keyboard.GetScancodeNameEXT | `in-keyboard` |
| Keyboard.GetState | `in-keyboard` |
| KeyboardState | `in-edges` |
| KeyboardState.GetPressedKeys | `in-keyboard` |
| KeyboardState.IsKeyDown | `in-keyboard` |
| KeyframeEXT | `model-skinext` |
| Keys | `in-edges` |
| LaunchParameters | `app-lifecycle` |
| LaunchingEventArgs | `pl-window` |
| LeaderboardEntry | `gs-leaderboards` |
| LeaderboardEntry.Columns | `gs-leaderboards` |
| LeaderboardEntry.Gamer | `gs-leaderboards` |
| LeaderboardEntry.Rating | `gs-leaderboards` |
| LeaderboardIdentity | `gs-leaderboards` |
| LeaderboardKey | `gs-leaderboards` |
| LeaderboardOutcome | `gs-leaderboards` |
| LeaderboardReader | `gs-leaderboards` |
| LeaderboardWriter | `gs-leaderboards` |
| LeaderboardWriter.GetLeaderboard | `gs-leaderboards` |
| LocalNetworkGamer.ReceiveData | `net-session` |
| LocalNetworkGamer.SendData | `net-session` |
| Logger.GetMinimumLevel | `pl-log` |
| Logger.InfoIf | `pl-log` |
| Logger.Log | `pl-log` |
| Logger.ResetSink | `pl-log` |
| Logger.SetMinimumLevel | `pl-log` |
| Logger.SetSink | `pl-log` |
| Logger.WarnIf | `pl-log` |
| LooseFileContentTypeReader | `ct-custom` |
| MaterialContent | `ct-modelimport` |
| MathHelper | `an-pendulum`, `sim-integrators`, `rf-deck` |
| MathHelper.CatmullRom | `curve-easing` |
| MathHelper.Hermite | `curve-easing` |
| MathHelper.Lerp | `curve-easing`, `geo-terrain`, `an-doors` |
| MathHelper.SmoothStep | `curve-easing`, `an-doors` |
| MathHelper.ToDegrees | `sim-ik` |
| MathHelper.ToRadians | `sim-ik` |
| MathHelper.WrapAngle | `an-walker` |
| MathTypeConverter | `vt-converters` |
| MathTypeConverter.CanConvertFrom | `vt-converters` |
| MathTypeConverter.GetProperties | `vt-converters` |
| Matrix | `models`, `animation`, `fx-colormatrix` |
| Matrix.CreateBillboard | `mat-billboard` |
| Matrix.CreateConstrainedBillboard | `mat-billboard` |
| Matrix.CreateFromQuaternion | `quat-euler` |
| Matrix.CreateLookAt | `mat-view`, `cam-orbit`, `cam-chase`, `cam-topdown`, `cam-mirror`, `rt-mirror`, `rv-camera`, `rf-telescope` |
| Matrix.CreateOrthographic | `mat-ortho`, `cam-topdown`, `cx-shadow`, `rv-camera` |
| Matrix.CreateOrthographicOffCenter | `mat-offcenter` |
| Matrix.CreatePerspective | `mat-offcenter` |
| Matrix.CreatePerspectiveFieldOfView | `mat-perspective`, `mat-ortho`, `col-frustum`, `cam-security`, `cam-fov`, `cam-clip`, `rv-camera`, `rf-telescope` |
| Matrix.CreatePerspectiveOffCenter | `mat-offcenter` |
| Matrix.CreateReflection | `mat-reflection` |
| Matrix.CreateRotationX | `quat-euler`, `an-gyroscope` |
| Matrix.CreateRotationY | `grounds-sculpture`, `mat-order`, `an-gears`, `an-gyroscope` |
| Matrix.CreateRotationZ | `sprite-camera`, `an-clock`, `an-kinetic`, `an-gyroscope`, `an-walker` |
| Matrix.CreateScale | `grounds-sculpture`, `mat-order`, `sprite-camera`, `sprite-tilemap`, `sim-flock` |
| Matrix.CreateShadow | `rs-shadow` |
| Matrix.CreateTranslation | `mat-order`, `sprite-camera`, `sim-flock` |
| Matrix.CreateWorld | `cam-transition` |
| Matrix.Decompose | `mat-hierarchy` |
| Matrix.Determinant | `mat-reflection` |
| Matrix.Invert | `mat-view`, `mat-normals`, `model-cameras` |
| Matrix.Multiply | `mat-order`, `mat-hierarchy`, `mat-screen`, `an-clock`, `an-gears`, `an-kinetic`, `an-walker` |
| Matrix.Translation | `mat-hierarchy` |
| Matrix.Transpose | `mat-normals` |
| MatrixConverter | `vt-converters` |
| MediaLibrary | `dev-medialib` |
| MediaLibrary.Albums | `dev-medialib` |
| MediaLibrary.Artists | `dev-medialib` |
| MediaLibrary.Dispose | `dev-medialib` |
| MediaLibrary.Genres | `dev-medialib` |
| MediaLibrary.IsDisposed | `dev-medialib` |
| MediaLibrary.MediaSource | `dev-medialib` |
| MediaLibrary.Pictures | `dev-medialib` |
| MediaLibrary.Playlists | `dev-medialib` |
| MediaLibrary.RootPictureAlbum | `dev-medialib` |
| MediaLibrary.SavePicture | `dev-medialib` |
| MediaLibrary.SavedPictures | `dev-medialib` |
| MediaLibrary.Songs | `dev-medialib` |
| MediaPlayer.ActiveSongChanged | `au-music` |
| MediaPlayer.GameHasControl | `au-music` |
| MediaPlayer.GetVisualizationData | `au-music` |
| MediaPlayer.IsShuffled | `au-music` |
| MediaPlayer.IsVisualizationEnabled | `au-music` |
| MediaPlayer.MediaStateChanged | `au-music` |
| MediaPlayer.MoveNext | `au-music` |
| MediaPlayer.MovePrevious | `au-music` |
| MediaPlayer.Play | `au-music` |
| MediaPlayer.Queue | `au-music` |
| MediaPlayer.Volume | `au-music` |
| MediaQueue | `au-music` |
| MediaSource.Name | `dev-medialib` |
| MediaState | `au-music` |
| MeshBuilder | `ct-meshbuilder`, `wk-meshhelper` |
| MeshBuilder.AddTriangleVertex | `ct-meshbuilder`, `wk-meshhelper` |
| MeshBuilder.CreatePosition | `ct-meshbuilder`, `wk-meshhelper` |
| MeshBuilder.CreateVertexChannel | `ct-meshbuilder`, `wk-meshhelper` |
| MeshBuilder.FinishMesh | `ct-meshbuilder`, `wk-meshhelper` |
| MeshBuilder.MergeDuplicatePositions | `ct-meshbuilder` |
| MeshBuilder.SetVertexChannelData | `ct-meshbuilder`, `wk-meshhelper` |
| MeshBuilder.StartMesh | `ct-meshbuilder`, `wk-meshhelper` |
| MeshBuilder.SwapWindingOrder | `ct-meshbuilder` |
| MeshContent | `ct-meshbuilder`, `ct-modelimport`, `wk-meshhelper` |
| MeshContent.Geometry | `wk-meshhelper` |
| MeshContent.Positions | `wk-meshhelper` |
| MeshHelper | `wk-meshhelper` |
| MeshHelper.CalculateNormals | `wk-meshhelper` |
| MeshHelper.CalculateTangentFrames | `wk-meshhelper` |
| MeshHelper.FindSkeleton | `wk-meshhelper` |
| MeshHelper.FlattenSkeleton | `wk-meshhelper` |
| MeshHelper.MergeDuplicatePositions | `wk-meshhelper` |
| MeshHelper.MergeDuplicateVertices(MeshContent) | `wk-meshhelper` |
| MeshHelper.OptimizeForCache | `wk-meshhelper` |
| MeshHelper.SwapWindingOrder | `wk-meshhelper` |
| MeshHelper.TransformScene | `wk-meshhelper` |
| MessageBox.IsSupported | `dev-dialogs` |
| MessageBox.Show | `dev-dialogs` |
| MessageBox.ShowSimple | `dev-dialogs` |
| MessageBoxIcon | `gs-guide` |
| MessageBoxType | `dev-dialogs` |
| MessageSendPriority | `dev-notify` |
| Microphone | `au-mic` |
| Microphone.BufferDuration | `au-mic` |
| Microphone.GetData | `au-mic` |
| Microphone.GetSampleSizeInBytes | `au-mic` |
| Microphone.IsHeadset | `au-mic` |
| Microphone.SampleRate | `au-mic` |
| Microsoft.Devices.Environment.DeviceType | `in-phone` |
| Microsoft::Xna::Framework::Content::Pipeline::ContentImporter | `ct-modelimport` |
| Microsoft::Xna::Framework::Content::Pipeline::ContentImporterContext | `ct-modelimport` |
| Microsoft::Xna::Framework::Content::Pipeline::FontDescriptionImporter | `ct-fontbuild` |
| Microsoft::Xna::Framework::Content::Pipeline::FontDescriptionImporter.Import | `ct-fontbuild` |
| Microsoft::Xna::Framework::Content::Pipeline::TargetPlatform | `ct-compiler` |
| Microsoft::Xna::Framework::Content::Pipeline::WavImporter | `ct-audiopipeline` |
| Microsoft::Xna::Framework::Content::Pipeline::WavImporter.Import | `ct-audiopipeline` |
| MipmapChain | `ct-processor` |
| Model | `models`, `model-load` |
| Model.BoundingSphereEXT | `model-load`, `model-instances`, `model-bounds` |
| Model.CamerasEXT | `model-cameras` |
| Model.CopyAbsoluteBoneTransformsTo | `model-rigid`, `model-sampler` |
| Model.CopyBoneTransformsFrom | `model-load` |
| Model.CopyBoneTransformsTo | `model-load` |
| Model.Draw | `model-instances` |
| Model.MaterialVariantEXT | `model-variants` |
| Model.MaterialVariantNamesEXT | `model-variants` |
| Model.Root | `model-load` |
| ModelAnimationsEXT | `model-rigid` |
| ModelBone | `model-rigid` |
| ModelBone.Children | `model-load` |
| ModelBoneContent | `ct-modelimport` |
| ModelCameraEXT | `model-cameras` |
| ModelCameraEXT.HasInfiniteFarPlane | `model-cameras` |
| ModelCameraEXT.IsPerspective | `model-cameras` |
| ModelCameraEXT.Projection | `model-cameras` |
| ModelCameraEXT.WorldTransform | `model-cameras` |
| ModelContent | `ct-meshbuilder`, `ct-modelimport` |
| ModelMesh | `models`, `model-load` |
| ModelMesh.BoundingSphere | `model-bounds` |
| ModelMesh.Draw | `model-sampler` |
| ModelMeshContent | `ct-meshbuilder`, `ct-modelimport` |
| ModelMeshPart | `model-load` |
| ModelMeshPart.Effect | `model-effects` |
| ModelMeshPart.PrimitiveTypeEXT | `model-load` |
| ModelMeshPart.SamplerStatesEXT | `model-load`, `model-sampler` |
| ModelMeshPart.Tag | `model-morph` |
| ModelMeshPartContent | `ct-meshbuilder`, `ct-modelimport` |
| MorphTargetDataEXT | `model-morph` |
| Motion | `in-phone` |
| Motion.IsSupported | `in-phone` |
| MotionReading.Attitude | `in-phone` |
| MotionReading.Gravity | `in-phone` |
| Mouse.GetGlobalPositionEXT | `in-devices` |
| Mouse.GetState | `in-mouse` |
| Mouse.IsRelativeMouseModeEXT | `in-mouse` |
| Mouse.SetCaptureEXT | `in-mouse` |
| Mouse.SetCursor | `in-mouse` |
| Mouse.SetPosition | `in-mouse` |
| Mouse.WindowHandle | `in-mouse` |
| MouseCursor | `in-mouse` |
| MouseCursor.FromTexture2D | `in-mouse` |
| MouseState | `in-mouse` |
| Mp3Importer | `ct-audiopipeline` |
| Mp3Importer.Attribute | `ct-audiopipeline` |
| Mp3Importer.Import | `ct-audiopipeline` |
| NetworkGamer.HasLeftSession | `net-session` |
| NetworkGamer.HasVoice | `net-session` |
| NetworkGamer.Id | `net-session` |
| NetworkGamer.IsGuest | `net-session` |
| NetworkGamer.IsHost | `net-session` |
| NetworkGamer.IsLocal | `net-session` |
| NetworkGamer.IsMutedByLocalUser | `net-session` |
| NetworkGamer.IsPrivateSlot | `net-session` |
| NetworkGamer.IsReady | `net-session` |
| NetworkGamer.IsTalking | `net-session` |
| NetworkGamer.Machine | `net-session` |
| NetworkGamer.RoundtripTime | `net-session` |
| NetworkGamer.Session | `net-session` |
| NetworkMachine | `net-session` |
| NetworkMachine.Gamers | `net-session` |
| NetworkMachine.RemoveFromSession | `net-session` |
| NetworkSession.AddLocalGamer | `net-session` |
| NetworkSession.AllGamers | `net-session` |
| NetworkSession.AllowHostMigration | `net-session` |
| NetworkSession.AllowJoinInProgress | `net-session` |
| NetworkSession.BeginFind | `net-session` |
| NetworkSession.BeginJoin | `net-session` |
| NetworkSession.BeginJoinInvited | `net-session` |
| NetworkSession.Create | `net-session` |
| NetworkSession.EndFind | `net-session` |
| NetworkSession.EndGame | `net-session` |
| NetworkSession.EndJoin | `net-session` |
| NetworkSession.EndJoinInvited | `net-session` |
| NetworkSession.FindGamerById | `net-session` |
| NetworkSession.GameStarted | `net-session` |
| NetworkSession.GamerJoined | `net-session` |
| NetworkSession.Host | `net-session` |
| NetworkSession.HostChanged | `net-session` |
| NetworkSession.InviteAccepted | `net-session` |
| NetworkSession.MaxGamers | `net-session` |
| NetworkSession.PreviousGamers | `net-session` |
| NetworkSession.PrivateGamerSlots | `net-session` |
| NetworkSession.RemoteGamers | `net-session` |
| NetworkSession.ResetReady | `net-session` |
| NetworkSession.SessionEnded | `net-session` |
| NetworkSession.SessionProperties | `net-session` |
| NetworkSession.SessionState | `net-session` |
| NetworkSession.SessionType | `net-session` |
| NetworkSession.SimulatedLatency | `net-session` |
| NetworkSession.SimulatedPacketLoss | `net-session` |
| NetworkSession.StartGame | `net-session` |
| NetworkSession.Update | `net-session` |
| NetworkSession.WriteArbitratedLeaderboard | `gs-leaderboards` |
| NetworkSession.WriteTrueSkill | `gs-leaderboards` |
| NetworkSession.WriteUnarbitratedLeaderboard | `gs-leaderboards` |
| NetworkSessionEndReason | `net-session` |
| NetworkSessionEndedEventArgs | `net-session` |
| NetworkSessionEndedEventArgs.EndReason | `net-session` |
| NetworkSessionJoinError | `net-session` |
| NetworkSessionJoinException | `net-session` |
| NetworkSessionProperties | `net-session` |
| NetworkSessionType.PlayerMatch | `net-session` |
| NodeContent | `ct-modelimport` |
| NodeContent.AbsoluteTransform | `wk-meshhelper` |
| NodeContent.Children | `ct-modelimport`, `wk-meshhelper` |
| NormalizedByte2 | `vt-packed` |
| NormalizedByte4 | `vt-packed` |
| NormalizedShort2 | `vt-packed` |
| NormalizedShort4 | `vt-packed` |
| NotificationChannelUriEventArgs | `dev-notify` |
| NotificationChannelUriEventArgs.ChannelUri | `dev-notify` |
| OcclusionQuery | `rs-occlusion` |
| OcclusionQuery.IsComplete | `rs-occlusion` |
| OcclusionQuery.PixelCount | `rs-occlusion` |
| PackedVector.Alpha8 | `tex-formats` |
| PackedVector.Bgr565 | `tex-formats`, `rv-formats` |
| PackedVector.Bgra4444 | `tex-formats` |
| PackedVector.HalfVector4 | `tex-formats`, `rv-formats` |
| PackedVector.Rgba64 | `rv-formats` |
| PacketReader | `net-packets` |
| PacketReader.Length | `net-packets` |
| PacketReader.Position | `net-packets` |
| PacketReader.ReadMatrix | `net-packets` |
| PacketReader.ReadQuaternion | `net-packets` |
| PacketReader.ReadVector3 | `net-packets` |
| PacketWriter | `net-packets` |
| PacketWriter.Length | `net-packets` |
| PacketWriter.Position | `net-packets` |
| PacketWriter.Write | `net-packets` |
| PbrEffect | `model-pbr`, `model-effects`, `model-variants` |
| PbrEffect.AlphaCutoffEXT | `model-material` |
| PbrEffect.EmissiveFactor | `model-pbr` |
| PbrEffect.EmissiveMap | `model-material` |
| PbrEffect.ImageBasedLightEXT | `model-ibl` |
| PbrEffect.MetallicFactor | `model-pbr`, `model-ibl` |
| PbrEffect.NormalMap | `model-material` |
| PbrEffect.RoughnessFactor | `model-pbr`, `model-ibl` |
| PhoneApplicationService | `pl-window` |
| PhoneApplicationService.Activated | `pl-window` |
| PhoneApplicationService.AttachEXT | `pl-window` |
| PhoneApplicationService.Closing | `pl-window` |
| PhoneApplicationService.Deactivated | `pl-window` |
| PhoneApplicationService.Launching | `pl-window` |
| PhoneApplicationService.State | `pl-window` |
| Picture.GetThumbnail | `dev-medialib` |
| Picture.Height | `dev-medialib` |
| Picture.Name | `dev-medialib` |
| Picture.Width | `dev-medialib` |
| PictureAlbum | `dev-medialib` |
| PictureAlbum.Albums | `dev-medialib` |
| PictureAlbum.Name | `dev-medialib` |
| PictureAlbum.Parent | `dev-medialib` |
| PictureAlbum.Pictures | `dev-medialib` |
| PictureAlbumCollection | `dev-medialib` |
| PictureCollection | `dev-medialib` |
| PixelBitmapContent | `ct-processor` |
| Plane | `vec-reflect`, `col-ray-plane` |
| Plane.DotCoordinate | `col-plane-box` |
| Plane.DotNormal | `col-plane-box` |
| Plane.Intersects | `col-plane-box` |
| Plane.Transform | `mat-reflection` |
| PlaneConverter | `vt-converters` |
| PlaneIntersectionType | `col-plane-box` |
| PlatformCapabilities | `pl-platform` |
| PlayerIndex | `in-players` |
| Playlist.Duration | `dev-medialib` |
| Playlist.Name | `dev-medialib` |
| Playlist.Songs | `dev-medialib` |
| PlaylistCollection | `dev-medialib` |
| Point | `vt-rectangle` |
| PointConverter | `vt-converters` |
| Power.GetInfoEXT | `in-devices` |
| PresentInterval | `app-viewport` |
| PresentationParameters | `app-device`, `app-viewport`, `pl-adapter` |
| PreviewResponse | `pl-inspector` |
| PrimitiveType | `geo-primitives` |
| PrimitiveType.LineList | `rf-weather` |
| PrimitiveType.PointListEXT | `cx-points`, `pf-particles` |
| Processors::FontDescriptionProcessor | `ct-fontbuild` |
| Processors::ModelProcessor | `ct-meshbuilder`, `ct-modelimport` |
| Processors::ModelProcessor.GenerateTangentFrames | `ct-meshbuilder` |
| Processors::ModelProcessor.RotationY | `ct-meshbuilder` |
| Processors::ModelProcessor.Scale | `ct-meshbuilder` |
| Processors::ModelProcessor.SwapWindingOrder | `ct-meshbuilder` |
| Processors::SongProcessor | `ct-audiopipeline` |
| Processors::SoundEffectProcessor | `ct-audiopipeline` |
| Processors::SoundEffectProcessor.Quality | `ct-audiopipeline` |
| Processors::TextureProcessor | `ct-processor`, `ct-compiler` |
| Processors::TextureProcessor.ColorKeyColor | `ct-processor` |
| Processors::TextureProcessor.ColorKeyEnabled | `ct-processor` |
| Processors::TextureProcessor.GenerateMipmaps | `ct-processor` |
| Processors::TextureProcessor.PremultiplyAlpha | `ct-processor` |
| Processors::TextureProcessor.Process | `ct-processor` |
| Processors::TextureProcessor.ResizeToPowerOfTwo | `ct-processor` |
| Processors::TextureProcessor.TextureFormat | `ct-processor` |
| Processors::VideoProcessor | `au-video` |
| Processors::VideoProcessor.Process | `au-video` |
| Processors::VideoProcessor.VideoSoundtrackType | `au-video` |
| PropertyDictionary | `gs-leaderboards` |
| PropertyDictionary.ContainsKey | `gs-leaderboards` |
| PropertyDictionary.Count | `gs-leaderboards` |
| PropertyDictionary.GetValueInt32 | `gs-leaderboards` |
| PropertyDictionary.GetValueOutcome | `gs-leaderboards` |
| PropertyDictionary.Keys | `gs-leaderboards` |
| PropertyDictionary.SetValue | `gs-leaderboards` |
| PropertyDictionary.Values | `gs-leaderboards` |
| PunctualLightEXT | `be-punctual`, `light-types`, `light-many`, `model-skinfx` |
| PunctualLightEXT.DiffuseColor | `light-coloured` |
| PunctualLightEXT.OuterAngle | `light-flashlight` |
| PunctualLightEXT.Range | `light-falloff` |
| PunctualLightKindEXT.Spot | `light-flashlight` |
| QualityOfService | `net-session` |
| QualityOfService.BytesPerSecondDownstream | `net-session` |
| QualityOfService.MinimumRoundtripTime | `net-session` |
| Quaternion.Concatenate | `quat-axis` |
| Quaternion.Conjugate | `quat-axis` |
| Quaternion.CreateFromAxisAngle | `quat-axis` |
| Quaternion.CreateFromYawPitchRoll | `quat-euler` |
| Quaternion.Dot | `quat-slerp` |
| Quaternion.Inverse | `quat-axis` |
| Quaternion.Lerp | `quat-slerp` |
| Quaternion.Normalize | `quat-slerp` |
| Quaternion.Slerp | `quat-slerp`, `cam-transition` |
| QuaternionConverter | `vt-converters` |
| RasterizerState | `renderstates` |
| RasterizerState.CullCounterClockwise | `rt-mirror` |
| RasterizerState.CullMode | `geo-triangle`, `rs-cull`, `rv-geometry` |
| RasterizerState.DepthBias | `rs-depthbias` |
| RasterizerState.FillMode | `geo-procedural`, `geo-terrain`, `rs-fill` |
| RasterizerState.MultiSampleAntiAlias | `rt-msaa` |
| RasterizerState.ScissorTestEnable | `sprite-scissor`, `rs-scissor` |
| RasterizerState.SlopeScaleDepthBias | `rs-depthbias` |
| RawPushNotificationMessage | `dev-notify` |
| RawPushNotificationMessage.RawData | `dev-notify` |
| RawPushNotificationMessage.SendAsync | `dev-notify` |
| Ray.Intersects | `col-ray-box`, `col-ray-sphere`, `col-ray-plane`, `col-pick` |
| RayConverter | `vt-converters` |
| Rectangle | `vt-rectangle`, `sprite-sheet`, `sprite-ui`, `sprite-anim` |
| Rectangle.Bottom | `vt-rectangle` |
| Rectangle.Contains | `vt-rectangle` |
| Rectangle.Inflate | `vt-rectangle` |
| Rectangle.Intersect | `vt-rectangle`, `sprite-hit` |
| Rectangle.Intersects | `vt-rectangle`, `sprite-hit`, `sprite-tilemap` |
| Rectangle.IsEmpty | `vt-rectangle` |
| Rectangle.Location | `vt-rectangle` |
| Rectangle.Union | `vt-rectangle` |
| RectangleConverter | `vt-converters` |
| RenderTarget2D | `rendertarget`, `cam-orbit`, `cam-fov`, `cam-split`, `cam-mirror`, `sprite-basics`, `sprite-lights`, `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-swirl`, `fx-chromatic`, `fx-kernel`, `fx-halftone`, `fx-vertexwave`, `fx-colormatrix`, `rs-scissor`, `rt-basic`, `rt-lowres`, `rt-control`, `rt-recursive`, `rt-mirror`, `rt-bloom`, `rt-msaa`, `pf-targets`, `rv-targets`, `rf-telescope` |
| RenderTarget2D.ContentLost | `dev-window` |
| RenderTarget2D.IsContentLost | `dev-window` |
| RenderTarget2D.MultiSampleCount | `rt-msaa` |
| RenderTarget2D.NotifyContentLostEXT | `dev-window` |
| RenderTargetBinding | `rt-mrt`, `rt-cube` |
| RenderTargetBinding.CubeMapFace | `rt-cube` |
| RenderTargetBinding.RenderTarget | `rt-mrt`, `rt-cube` |
| RenderTargetCube | `rt-cube` |
| RenderTargetUsage.PreserveContents | `rt-recursive` |
| RendererCapabilityProfile | `app-device` |
| RendererDetail.FriendlyName | `au-xact` |
| RendererFeature | `pl-renderer` |
| RendererFeature.BaseInstanceDrawing | `geo-instancing` |
| RendererFormatUsage | `pl-renderer` |
| RendererLimit | `pl-renderer` |
| ResourceContentManager | `ct-resource` |
| ResourceContentManager.OpenStream | `ct-resource` |
| ResourceCreatedEventArgs | `ct-lifetime` |
| ResourceCreatedEventArgs.Resource | `ct-lifetime` |
| ResourceDestroyedEventArgs | `ct-lifetime` |
| ResourceDestroyedEventArgs.Name | `ct-lifetime` |
| ResourceDestroyedEventArgs.Tag | `ct-lifetime` |
| Rg32 | `vt-packed` |
| Rgba1010102 | `vt-packed` |
| Rgba64 | `vt-packed` |
| SamplerState | `materials`, `sprite-sampler`, `model-sampler` |
| SamplerState.AddressU | `tex-address`, `tex-volshader`, `rv-sampling` |
| SamplerState.AddressV | `tex-volshader` |
| SamplerState.AddressW | `tex-volshader` |
| SamplerState.AnisotropicClamp | `tex-filter` |
| SamplerState.Filter | `tex-mip` |
| SamplerState.LinearClamp | `sprite-lights`, `tex-filter`, `rt-bloom`, `rv-sprites` |
| SamplerState.LinearWrap | `sprite-parallax`, `tex-uv`, `an-conveyor` |
| SamplerState.MaxAnisotropy | `tex-mip` |
| SamplerState.MaxMipLevel | `tex-mip` |
| SamplerState.MipMapLevelOfDetailBias | `tex-mip` |
| SamplerState.PointClamp | `sprite-sheet`, `tex-filter`, `rt-lowres`, `rv-sprites` |
| SamplerStateCollection | `materials` |
| SamplerStateCollection.operator[] | `fx-compiled` |
| SendDataOptions | `net-session` |
| SensorBase.CurrentValue | `in-phone` |
| SensorBase.CurrentValueChanged | `in-phone` |
| SensorReadingEventArgs | `in-phone` |
| SensorState | `in-phone` |
| Sensors.GetSensorsEXT | `in-devices` |
| SetDataOptions | `geo-dynamic` |
| SetDataOptions.Discard | `geo-dynindex`, `pt-fire`, `pt-smoke`, `pt-sparks`, `pt-snow`, `pt-rain`, `pt-fountain`, `pt-dust`, `pt-fireworks`, `pt-bubbles`, `pt-leaves`, `pt-magic`, `pf-buffers`, `rf-weather` |
| SetMorphWeightsEXT | `model-morph` |
| SetRuntimeMode | `cx-stats` |
| ShaderCodeEXT | `cx-package` |
| ShaderDiagnosticEXT.parseCompilerLog | `fx-errors` |
| ShaderEffect | `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-swirl`, `fx-chromatic`, `fx-kernel`, `fx-halftone`, `fx-vertexwave`, `fx-colormatrix`, `fx-errors`, `rt-mrt` |
| ShaderEffect(device, package) | `cx-package` |
| ShaderEffect.DeclareUniformBlockEXT | `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-swirl`, `fx-chromatic`, `fx-kernel`, `fx-halftone`, `fx-vertexwave`, `fx-colormatrix`, `cx-package` |
| ShaderEffect.GetCompileErrorEXT | `fx-errors` |
| ShaderEffect.GetShaderDiagnosticsEXT | `fx-errors` |
| ShaderEffect.IsEffectValid | `fx-errors` |
| ShaderEffect.SetTexture(Texture3D) | `tex-volshader` |
| ShaderEffect.SetUniformFloat | `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-swirl`, `fx-chromatic`, `fx-kernel`, `fx-halftone`, `fx-vertexwave` |
| ShaderLanguageEXT | `cx-package` |
| ShaderPackageEXT.selectFor | `cx-package` |
| ShadowCascadeStateEXT | `cx-shadow` |
| Short2 | `vt-packed` |
| Short4 | `vt-packed` |
| SignedInGamer.AwardAchievement | `gs-achievements` |
| SignedInGamer.GameDefaults | `gs-players` |
| SignedInGamer.GetAchievements | `gs-achievements` |
| SignedInGamer.GetFriends | `on-friends` |
| SignedInGamer.IsFriend | `pl-exceptions` |
| SignedInGamer.IsSignedInToLive | `on-account` |
| SignedInGamer.PartySize | `on-party` |
| SignedInGamer.Presence | `gs-players` |
| SignedInGamer.Privileges | `gs-players` |
| SignedInGamer.SignedIn | `gs-players` |
| SkinnedEffect | `be-model`, `model-skinfx` |
| SkinnedEffect.SetBoneTransforms | `model-skinfx`, `model-skinext` |
| SkinnedEffect.WeightsPerVertex | `model-skinfx` |
| SkinnedEffect.setPunctualLightEXT | `model-skinfx` |
| SkinnedModelEXT | `model-skinext` |
| SkinnedModelEXT.AddPartEXT | `model-skinext` |
| SkinnedModelEXT.AttachPartEXT | `model-skinext` |
| SkinnedModelEXT.ComputeBoneTransformsEXT | `model-skinext` |
| SkinnedModelEXT.RemovePartEXT | `model-skinext` |
| SkinnedPbrEffect.SetBoneTransforms | `model-skinned` |
| SkinningData | `model-skinned` |
| Snapshot.metrics | `cx-stats` |
| Snapshot.resources | `cx-stats` |
| SnapshotPart | `pl-inspector` |
| SnapshotResponse | `pl-inspector` |
| Song | `au-music` |
| Song.Album | `dev-medialib` |
| Song.Artist | `au-music`, `dev-medialib` |
| Song.Duration | `au-music`, `dev-medialib` |
| Song.Genre | `dev-medialib` |
| Song.IsProtected | `au-music` |
| Song.IsRated | `au-music` |
| Song.Name | `dev-medialib` |
| Song.PlayCount | `au-music` |
| Song.Rating | `au-music` |
| Song.TrackNumber | `au-music`, `dev-medialib` |
| SongCollection | `au-music`, `dev-medialib` |
| SongContent | `ct-audiopipeline` |
| SoundBank | `au-xact` |
| SoundBank.GetCue | `au-xact` |
| SoundBank.IsInUse | `au-xact` |
| SoundBank.PlayCue | `au-xact` |
| SoundEffect | `audio`, `au-basics` |
| SoundEffect(buffer, sampleRate, channels) | `au-generate` |
| SoundEffect.DistanceScale | `au-spatial`, `au-master` |
| SoundEffect.DopplerScale | `au-spatial`, `au-master` |
| SoundEffect.FromStream | `ct-audiopipeline` |
| SoundEffect.GetSampleDuration | `au-synth` |
| SoundEffect.MasterVolume | `au-master` |
| SoundEffect.Play | `au-basics` |
| SoundEffect.SpeedOfSound | `au-spatial`, `au-master` |
| SoundEffectContent | `ct-audiopipeline` |
| SoundEffectInstance | `grounds-fountain`, `audio`, `au-layers`, `rf-garden` |
| SoundEffectInstance.Apply3D | `au-spatial` |
| SoundEffectInstance.IsLooped | `au-layers` |
| SoundEffectInstance.Pitch | `au-layers` |
| SoundEffectInstance.Volume | `au-layers` |
| SoundState | `au-layers` |
| SpriteBatch | `sprite-stress`, `rf-weather` |
| SpriteBatch.Begin | `sprite-basics`, `sprite-camera`, `sprite-tilemap`, `fx-compiled`, `rt-bloom`, `pl-exceptions`, `pf-sprites` |
| SpriteBatch.Begin(effect) | `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-swirl`, `fx-chromatic`, `fx-kernel`, `fx-halftone`, `fx-vertexwave`, `fx-colormatrix`, `cx-crt` |
| SpriteBatch.Draw | `sprite-basics`, `sprite-origin`, `sprite-sheet`, `sprite-sort`, `sprite-parallax`, `sprite-ui`, `sprite-atlas`, `sprite-hit`, `sprite-anim`, `sprite-tilemap`, `sprite-lights`, `sprite-stress`, `rt-recursive`, `hist-compare`, `pf-sprites`, `pf-overdraw`, `rv-sprites` |
| SpriteBatch.DrawString | `text-align`, `text-transform`, `text-hud`, `text-labels`, `ct-fontbuild`, `hist-xna`, `hist-compare`, `vr-index` |
| SpriteBatch.End | `pl-exceptions` |
| SpriteEffect | `be-model` |
| SpriteEffects | `sprite-basics`, `text-transform`, `rv-sprites` |
| SpriteEffects.FlipHorizontally | `sprite-anim` |
| SpriteFont | `ct-fontbuild` |
| SpriteFont.Characters | `text-glyphs` |
| SpriteFont.CroppingEXT | `text-glyphs` |
| SpriteFont.DefaultCharacter | `text-glyphs` |
| SpriteFont.GlyphBoundsEXT | `text-glyphs` |
| SpriteFont.KerningEXT | `text-glyphs` |
| SpriteFont.LineSpacing | `text-measure` |
| SpriteFont.MeasureString | `text-align`, `text-measure`, `ct-fontbuild` |
| SpriteFont.Spacing | `text-measure` |
| SpriteFont.TextureEXT | `text-glyphs` |
| SpriteFontContent | `ct-fontbuild` |
| SpriteSortMode | `sprite-sort`, `pf-sprites` |
| StencilOperation | `rs-portal`, `rs-stencilcount` |
| StencilOperation.Increment | `rs-shadow` |
| StorageContainer | `ct-storage`, `pl-exceptions` |
| StorageContainer.CreateDirectory | `ct-storage` |
| StorageContainer.CreateFile | `ct-storage` |
| StorageContainer.DeleteDirectory | `ct-storage` |
| StorageContainer.DeleteFile | `ct-storage` |
| StorageContainer.DirectoryExists | `ct-storage` |
| StorageContainer.DisplayName | `ct-storage` |
| StorageContainer.FileExists | `ct-storage` |
| StorageContainer.GetDirectoryNames | `ct-storage` |
| StorageContainer.GetFileNames | `ct-storage` |
| StorageContainer.OpenFile | `ct-storage` |
| StorageDevice | `ct-storage` |
| StorageDevice.BeginOpenContainer | `ct-storage` |
| StorageDevice.BeginShowSelector | `ct-storage` |
| StorageDevice.DeleteContainer | `ct-storage` |
| StorageDevice.DeviceChanged | `ct-storage` |
| StorageDevice.EndOpenContainer | `ct-storage` |
| StorageDevice.EndShowSelector | `ct-storage` |
| StorageDevice.FreeSpace | `ct-storage` |
| StorageDevice.GetStorageRootEXT | `ct-storage` |
| StorageDevice.IsConnected | `ct-storage` |
| StorageDevice.SetAppNameEXT | `ct-storage` |
| StorageDevice.TotalSpace | `ct-storage` |
| SurfaceFormat | `tex-formats`, `rt-formats`, `rv-formats` |
| SurfaceFormat.Dxt1 | `ct-formats` |
| SurfaceFormat.Single | `tex-volume` |
| SurfaceFormat.Vector4 | `tex-volume` |
| System.ArgumentOutOfRangeException | `pl-exceptions` |
| System.IO.FileNotFoundException | `pl-exceptions` |
| System.IO.MemoryStream | `tex-io` |
| System.InvalidOperationException | `pl-exceptions` |
| System.ObjectDisposedException | `pl-exceptions` |
| System.Resources.ResourceManager | `ct-resource` |
| System.Text.StringBuilder | `text-hud` |
| System::ArgumentException | `rv-vertextype` |
| SystemTray.AddEntry | `dev-tray` |
| SystemTray.GetEntryChecked | `dev-tray` |
| SystemTray.GetEntryEnabled | `dev-tray` |
| SystemTray.IsSupported | `dev-tray` |
| SystemTray.SetEntryChecked | `dev-tray` |
| SystemTray.SetEntryEnabled | `dev-tray` |
| SystemTray.SetEntryLabel | `dev-tray` |
| SystemTray.Tooltip | `dev-tray` |
| TextDropEventArgsEXT | `dev-window` |
| TextDropEventArgsEXT.Text | `dev-window` |
| TextInputEXT.StartTextInputWithTypeEXT | `in-text` |
| TextInputEXT.TextInput | `in-text` |
| TextInputTypeEXT | `in-text` |
| Texture.Format | `rt-formats` |
| Texture2D | `materials`, `grounds-fountain`, `app-content`, `tex-mip`, `rf-garden` |
| Texture2D.DDSFromStreamEXT | `ct-formats` |
| Texture2D.FromStream | `tex-io`, `dev-medialib` |
| Texture2D.GetData | `sprite-atlas`, `sprite-hit`, `sprite-tilemap`, `geo-terrain`, `tex-dynamic`, `ct-formats`, `rv-targets`, `rv-formats` |
| Texture2D.SaveAsJpeg | `tex-io` |
| Texture2D.SaveAsPng | `tex-io` |
| Texture2D.SetData | `sprite-ui`, `sprite-atlas`, `sprite-anim`, `tex-dynamic`, `rv-formats` |
| Texture2DContent | `ct-processor`, `ct-compiler` |
| Texture3D | `tex-volume`, `tex-volshader` |
| Texture3D.GetData | `tex-volume` |
| Texture3D.GetData(level, left, front) | `tex-volume` |
| Texture3D.SetData | `tex-volume` |
| TextureAddressMode | `sprite-sampler`, `tex-address`, `tex-volshader`, `model-sampler`, `rv-sampling` |
| TextureCube | `tex-cube`, `model-ibl` |
| TextureCube.GetData | `tex-cube` |
| TextureCube.SetData | `tex-cube` |
| TextureFilter | `tex-volshader` |
| TextureFilter.MinLinearMagPointMipLinear | `tex-mip` |
| TextureFilter.MinPointMagLinearMipPoint | `tex-mip` |
| TextureImporter | `ct-processor`, `ct-compiler` |
| TextureImporter.Import | `ct-processor` |
| TextureProcessorOutputFormat | `ct-processor` |
| TextureTransformEXT | `model-material` |
| TitleContainer.OpenStream | `ct-storage`, `pl-exceptions` |
| ToastPushNotificationMessage | `dev-notify` |
| ToastPushNotificationMessage.SendAsync | `dev-notify` |
| ToastPushNotificationMessage.SubTitle | `dev-notify` |
| ToastPushNotificationMessage.Title | `dev-notify` |
| TouchLocation.Id | `in-touch` |
| TouchLocation.State | `in-touch` |
| TouchLocation.TryGetPreviousLocation | `in-touch` |
| TouchPanel.EnabledGestures | `in-touch` |
| TouchPanel.GetState | `in-touch`, `in-devices` |
| TouchPanel.MouseTouchEmulationEnabledEXT | `in-touch`, `in-devices` |
| TouchPanel.ReadGesture | `in-touch` |
| Trace.ReadBinary | `pl-diagnostics` |
| Trace.WriteBinary | `pl-diagnostics` |
| Trace.WriteChromeTrace | `pl-diagnostics` |
| UrlLauncher.Open | `dev-tray` |
| Vector2 | `sprite-origin` |
| Vector2Converter | `vt-converters` |
| Vector3 | `an-pendulum`, `sim-flock`, `sim-integrators`, `rf-deck` |
| Vector3.Add | `vec-add` |
| Vector3.Barycentric | `curve-barycentric` |
| Vector3.CatmullRom | `vec-lerp`, `curve-spline` |
| Vector3.Cross | `vec-cross`, `sim-ik` |
| Vector3.Distance | `vec-normalize`, `an-doors` |
| Vector3.Dot | `vec-dot`, `vec-project`, `sim-balls`, `sim-ik` |
| Vector3.Length | `vec-cross`, `vec-normalize` |
| Vector3.LengthSquared | `sim-flock` |
| Vector3.Lerp | `vec-lerp`, `cam-chase` |
| Vector3.Multiply | `vec-project` |
| Vector3.Normalize | `vec-reflect`, `vec-normalize`, `sim-flock` |
| Vector3.Reflect | `vec-reflect` |
| Vector3.SmoothStep | `vec-lerp`, `cam-transition` |
| Vector3.Subtract | `vec-add` |
| Vector3.TransformNormal | `mat-normals` |
| Vector3Converter | `vt-converters` |
| Vector3Converter.ConvertFrom | `vt-converters` |
| Vector3Converter.ConvertTo | `vt-converters` |
| Vector4.Transform | `mat-screen` |
| Vector4Converter | `vt-converters` |
| VertexBuffer | `geo-vertextypes`, `geo-indexed`, `geo-procedural`, `geo-manybuffers`, `geo-terrain`, `model-skinfx`, `pf-buffers` |
| VertexBuffer(GraphicsDevice, Type, int, BufferUsage) | `rv-vertextype` |
| VertexBufferBinding | `geo-instancing`, `pf-objects` |
| VertexBufferContent | `ct-meshbuilder` |
| VertexChannelCollection.Contains | `wk-meshhelper` |
| VertexChannelCollection.Get | `wk-meshhelper` |
| VertexChannelNames | `ct-meshbuilder` |
| VertexChannelNames.Binormal | `wk-meshhelper` |
| VertexChannelNames.Normal | `wk-meshhelper` |
| VertexChannelNames.Tangent | `wk-meshhelper` |
| VertexChannelNames.TextureCoordinate | `wk-meshhelper` |
| VertexContent.Channels | `wk-meshhelper` |
| VertexContent.PositionIndices | `wk-meshhelper` |
| VertexContent.VertexCount | `wk-meshhelper` |
| VertexDeclaration | `geo-vertextypes`, `rv-vertextype` |
| VertexDeclaration.GetVertexElements | `geo-vertextypes`, `rv-vertextype` |
| VertexDeclaration.VertexStride | `geo-vertextypes`, `rv-vertextype` |
| VertexDeclarationContent | `ct-meshbuilder` |
| VertexElement | `geo-vertextypes`, `rv-vertextype` |
| VertexElementFormat | `geo-vertextypes` |
| VertexElementUsage | `geo-vertextypes` |
| VertexPositionColor | `geo-triangle`, `geo-vertextypes`, `cx-shadow` |
| VertexPositionColorTexture | `geo-vertextypes`, `tex-uv`, `an-conveyor` |
| VertexPositionNormalTexture | `geo-vertextypes`, `geo-dynamic`, `geo-terrain` |
| VertexPositionNormalTextureSkinned | `model-skinfx`, `model-skinext` |
| VertexPositionTexture | `geo-vertextypes` |
| VertexTypeRegistryEXT.Register | `rv-vertextype` |
| VertexTypeRegistryEXT.Resolve | `rv-vertextype` |
| VertexTypeRegistryEXT.TryResolve | `rv-vertextype` |
| VibrateController | `in-phone` |
| VibrateController.Default | `in-phone` |
| VibrateController.IsSupported | `in-phone` |
| VibrateController.Start | `in-phone` |
| Video | `au-video` |
| VideoContent | `au-video` |
| VideoPlayer | `au-video` |
| VideoPlayer.GetTexture | `au-video` |
| VideoPlayer.IsDisposed | `au-video` |
| VideoPlayer.IsLooped | `au-video` |
| VideoPlayer.IsMuted | `au-video` |
| VideoPlayer.Pause | `au-video` |
| VideoPlayer.Play | `au-video` |
| VideoPlayer.PlayPosition | `au-video` |
| VideoPlayer.Resume | `au-video` |
| VideoPlayer.State | `au-video` |
| VideoPlayer.Stop | `au-video` |
| VideoPlayer.Video | `au-video` |
| VideoPlayer.Volume | `au-video` |
| VideoSoundtrackType | `au-video` |
| Viewport | `app-viewport`, `cam-split` |
| Viewport.MaxDepth | `app-viewport` |
| Viewport.MinDepth | `app-viewport` |
| Viewport.Project | `app-viewport`, `mat-screen`, `text-labels` |
| Viewport.Unproject | `mat-screen` |
| VisualizationData | `au-music` |
| WaveBank | `au-xact` |
| WaveBank.IsInUse | `au-xact` |
| WmvImporter | `au-video` |
| WmvImporter.Import | `au-video` |
| WriteLeaderboardsEventArgs.Gamer | `gs-leaderboards` |
| WriteLeaderboardsEventArgs.IsLeaving | `gs-leaderboards` |
| XImporter | `ct-modelimport` |
| XmlImporter | `ct-intermediate` |
| XmlImporter.Attribute | `ct-intermediate` |
| XmlImporter.Import | `ct-intermediate` |
<!-- END COVERAGE -->
