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
| Renderer self-checks (106 results on OPENGLES3: geometry, sprites, blending, depth and stencil, shading and PbrEffect, sampling, targets, camera, formats, effects, vertex types, capability declarations) | floor 9 | covered |
| Time of day, sky dome | floors 0 and 9 | application code |

### Gaps (not demonstrated yet) and non-goals

| Feature | Status |
|---|---|
| Haptic devices (`CNA::Input::Haptics`, `HapticDevice`) | *Haptic Devices* (`dev-haptics`, floor 7): list, open, rumble, a constant effect's whole lifetime, gain, autocenter, pause and resume behind manual buttons; on a machine without a device `tools/devices_check.sh` verifies the refusals. Playing on real hardware is a manual check |
| `MediaLibrary`, albums, playlists, pictures (`Microsoft.Xna.Framework.Media`) | *The Media Library* (`dev-medialib`, floor 7) reads the library only when the visitor presses its button, because CNA scans the visitor's own Music and Pictures folders (the public API cannot point it elsewhere). `tools/devices_check.sh` runs it with HOME pointing at the fixture library in `Content/MediaLibrary` and checks the exact songs, albums, artists, genres, playlist and pictures |
| Online services, voice, host migration | The *Online Service* wing needs the sibling CNA gamer-services server (`tools/online_stack.sh`). `tools/online_check.sh` signs an account in through the Guide, writes a rating and an achievement to a private service and reads the board and the friend list back. Network voice between two museum processes is verified with a test tone through SDL's disk audio driver (`tools/voice_check.sh`), host migration, packets and session lifecycle across processes by `tools/net_check.sh` (two CNA defects recorded in `docs/CNA_FINDINGS.md`). `tools/online_party_check.sh` runs three museums signed in as the stack's three accounts: a game invitation accepted on the Guide's card joins with JoinInvited into a private slot, a third gamer takes the public one, two accounts form a party through the Guide's party pages, and signing out in the Guide raises SignedInGamer.SignedOut |
| `FileDialog`, `MessageBox`, `SystemTray`, `UrlLauncher`, `Camera` (CNA_DEVICES) | *Native Dialogs*, *Tray Icon and URLs* and *The Camera* (floor 7) open them only from manual buttons, which the automatic sweep never presses. `tools/devices_check.sh` presses them on a private display with fake `zenity`/`xdg-open` and no session bus: the dialogs and the URL launcher are verified; a tray needs a StatusNotifier host and a camera is opened only with `MUSEUM_ALLOW_CAMERA=1` |
| C API | *Native C API* (`cx-capi`, `MUSEUM_C_API`, which turns on `CNA_BUILD_C_API`): every C header is claimed. C17 code in the museum (`src/CApi/`) checks the value and handle families, and a separate C17 program, `cna-c-probe`, which the room runs as a child process, creates its own game through `cna_game_create` for graphics, effects, models, content, audio, media, input, storage and sessions (128 checks, 7 CNA divergences) |
| Inspector (`CNA::Inspector::Agent`, `Client`) | `pl-inspector`: the museum builds CNA's optional Inspector module (`MUSEUM_INSPECTOR`, default ON) and the room starts the agent on a button press and plays the client in the same process. The separate `cna-inspector` bridge and browser page are not exercised |
| Compiled XNA effects (`.fxb`) | *Compiled XNA Effects* runs real XNA-compiled effects wherever `GraphicsCapability::CompiledEffects` is true. In CNA's default configuration that is FNA3D only (measured 2026-10-19 on OPENGLES3, FNA3D, WEBGPU and SDL_GPU); EasyGL, WebGPU, SDL_GPU, SOFTWARE and DIRECTX9 implement it behind `CNA_<RENDERER>_COMPILED_EFFECTS` build options, which are off by default (the `build-probe` coverage tree turns them on for EasyGL, WebGPU, SDL_GPU and SOFTWARE; on SOFTWARE's CPU executor all four checks of the `compiled_effects` scenario pass, vertex texture fetch included, 2026-10-20) |
| Retired renderers and removed CNA features (for example VULKAN, the post-process chain) | deliberately not demonstrated |

## Generated: API names claimed by rooms

<!-- BEGIN COVERAGE -->
2904 distinct API names are claimed by at least one room.

| API | Rooms |
|---|---|
| Accelerometer | `in-phone` |
| Accelerometer.InjectSyntheticSensorUpdate | `in-phone` |
| Accelerometer.IsSupported | `in-phone` |
| Accelerometer.KeyboardEmulationEnabledEXT | `in-phone` |
| Accelerometer.ReadingChanged | `in-phone` |
| Accelerometer.Start | `in-phone` |
| Accelerometer.State | `in-phone` |
| Accelerometer.Stop | `in-phone` |
| AccelerometerReading.Acceleration | `in-phone` |
| AccelerometerReadingEventArgs | `in-phone` |
| AccelerometerReadingEventArgs.Timestamp | `in-phone` |
| AccelerometerReadingEventArgs.X | `in-phone` |
| AccelerometerReadingEventArgs.Y | `in-phone` |
| AccelerometerReadingEventArgs.Z | `in-phone` |
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
| AlphaTestMaterialContent | `wk-material` |
| AnimationChannel | `wk-animation` |
| AnimationChannel.Add | `wk-animation` |
| AnimationChannel.Clear | `wk-animation` |
| AnimationChannel.Contains | `wk-animation` |
| AnimationChannel.Count | `wk-animation` |
| AnimationChannel.IndexOf | `wk-animation` |
| AnimationChannel.Remove | `wk-animation` |
| AnimationChannel.RemoveAt | `wk-animation` |
| AnimationChannelDictionary | `wk-animation` |
| AnimationClipEXT | `model-skinext` |
| AnimationContent | `wk-animation` |
| AnimationContent.Channels | `wk-animation` |
| AnimationContent.Duration | `wk-animation` |
| AnimationContentDictionary | `wk-animation` |
| AnimationKeyframe | `wk-animation` |
| AnimationKeyframe.CompareTo | `wk-animation` |
| AnimationKeyframe.Time | `wk-animation` |
| AnimationKeyframe.Transform | `wk-animation` |
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
| AudioContent | `ct-audiopipeline`, `wk-importers` |
| AudioContent.Data | `ct-audiopipeline`, `wk-importers` |
| AudioContent.Duration | `wk-importers` |
| AudioContent.FileType | `wk-importers` |
| AudioContent.Format | `ct-audiopipeline`, `wk-importers` |
| AudioEmitter | `audio`, `au-spatial` |
| AudioEngine | `au-xact` |
| AudioEngine.GetCategory | `au-xact` |
| AudioEngine.GetGlobalVariable | `au-xact` |
| AudioEngine.RendererDetails | `au-xact` |
| AudioEngine.SetGlobalVariable | `au-xact` |
| AudioEngine.Update | `au-xact` |
| AudioFileType | `wk-importers` |
| AudioFormat.BitsPerSample | `wk-importers` |
| AudioFormat.ChannelCount | `wk-importers` |
| AudioFormat.Format | `wk-importers` |
| AudioFormat.SampleRate | `wk-importers` |
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
| BasicMaterialContent | `ct-modelimport`, `wk-material`, `wk-processor` |
| BasicMaterialContent.Alpha | `wk-material` |
| BasicMaterialContent.DiffuseColor | `ct-modelimport`, `wk-material` |
| BasicMaterialContent.EmissiveColor | `wk-material` |
| BasicMaterialContent.SpecularColor | `wk-material` |
| BasicMaterialContent.SpecularPower | `wk-material` |
| BasicMaterialContent.Texture | `wk-material` |
| BasicMaterialContent.VertexColorEnabled | `wk-material` |
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
| BoneWeight | `wk-animation` |
| BoneWeight.BoneName | `wk-animation` |
| BoneWeight.Weight | `wk-animation` |
| BoneWeightCollection | `wk-animation` |
| BoneWeightCollection.NormalizeWeights | `wk-animation` |
| BoundingBox | `col-ray-box`, `col-pick`, `an-conveyor`, `sim-balls` |
| BoundingBox.Contains | `col-box-box`, `math-checks` |
| BoundingBox.CreateFromPoints | `col-merge` |
| BoundingBox.CreateFromSphere | `col-merge`, `math-checks` |
| BoundingBox.CreateMerged | `math-checks` |
| BoundingBox.Intersects | `col-box-box`, `math-checks` |
| BoundingBoxConverter | `vt-converters` |
| BoundingFrustum | `mat-view`, `col-frustum`, `cam-orbit`, `cam-security`, `model-cameras` |
| BoundingFrustum.Contains | `mat-perspective`, `col-frustum`, `math-checks`, `model-instances` |
| BoundingFrustum.GetCorners | `col-frustum` |
| BoundingFrustum.Intersects | `math-checks` |
| BoundingSphere | `col-ray-sphere`, `col-pick` |
| BoundingSphere.Contains | `col-sphere-box`, `math-checks` |
| BoundingSphere.CreateFromBoundingBox | `col-merge`, `math-checks` |
| BoundingSphere.CreateFromFrustum | `col-frustum` |
| BoundingSphere.CreateFromPoints | `col-merge` |
| BoundingSphere.CreateMerged | `col-merge`, `math-checks` |
| BoundingSphere.Intersects | `col-sphere-box`, `math-checks`, `sim-balls` |
| BoundingSphere.Transform | `math-checks`, `model-bounds` |
| BoundingSphereConverter | `vt-converters` |
| BuildContent | `wk-contentproject` |
| BuildContent.BuildConfiguration | `wk-contentproject` |
| BuildContent.CancelEventNameFormat | `wk-contentproject` |
| BuildContent.CompressContent | `wk-contentproject` |
| BuildContent.ContentProjectGUID | `wk-contentproject` |
| BuildContent.Execute | `wk-contentproject` |
| BuildContent.IntermediateDirectory | `wk-contentproject` |
| BuildContent.IntermediateFiles | `wk-contentproject` |
| BuildContent.OutputContentFiles | `wk-contentproject` |
| BuildContent.OutputDirectory | `wk-contentproject` |
| BuildContent.PipelineAssemblies | `wk-contentproject` |
| BuildContent.RebuildAll | `wk-contentproject` |
| BuildContent.RebuiltContentFiles | `wk-contentproject` |
| BuildContent.RootDirectory | `wk-contentproject` |
| BuildContent.SourceAssets | `wk-contentproject` |
| BuildContent.TargetPlatform | `wk-contentproject` |
| BuildContent.TargetProfile | `wk-contentproject` |
| BuildTimeMedia::AudioSourceFormat | `wk-importers` |
| BuildTimeMedia::DecodeAudio | `wk-importers` |
| BuildTimeMedia::DecodedAudio | `wk-importers` |
| BuildTimeMedia::EncodeWindowsMediaAudio | `wk-importers` |
| BuildTimeMedia::Identity | `wk-importers` |
| BuildTimeMedia::IsAvailable | `wk-importers` |
| BuildTimeMedia::MakeCompressedSoundDecoder | `wk-routes` |
| BuildTimeMedia::MakeSongDurationProbe | `wk-routes` |
| BuildTimeMedia::MakeVideoMetadataProbe | `wk-routes` |
| BuildTimeMedia::ProbeAudioDurationTicks | `wk-importers` |
| BuildTimeMedia::ProbeVideo | `wk-importers` |
| BuildTimeMedia::ProbedVideo | `wk-importers` |
| BuildTimeMedia::UnavailableReason | `wk-importers` |
| BuildXact | `wk-tasks` |
| BuildXact.Execute | `wk-tasks` |
| BuildXact.HasXactCompilerEXT | `wk-tasks` |
| BuildXact.IntermediateFiles | `wk-tasks` |
| BuildXact.OutputXactFiles | `wk-tasks` |
| BuildXact.RebuildAll | `wk-tasks` |
| BuildXact.RebuiltXactFiles | `wk-tasks` |
| BuildXact.TargetPlatform | `wk-tasks` |
| BuildXact.XactProjects | `wk-tasks` |
| BuildXact.XnaTypeName | `wk-tasks` |
| BuildXmaEncoder | `wk-importers` |
| ButtonState | `in-mouse` |
| Buttons | `in-pad` |
| Buttons.BigButton | `in-pad` |
| Buttons.LeftThumbstickUp | `in-pad` |
| Buttons.RightThumbstickLeft | `in-pad` |
| Byte4 | `vt-packed` |
| CNA::AssemblyTitleAttributeEXT | `app-device` |
| CNA::Content::Pipeline::ContentProcessorContext | `wk-material`, `wk-processor` |
| CNA::Content::Pipeline::ModelProcessor | `wk-routes` |
| CNA::Content::Pipeline::ModelProcessor.ValidateParameters | `wk-routes` |
| CNA::Content::Pipeline::SongProcessor | `wk-routes` |
| CNA::Content::Pipeline::SoundEffectProcessor | `wk-routes` |
| CNA::Content::Pipeline::VideoImporter | `wk-routes` |
| CNA::Content::Pipeline::VideoProcessor | `wk-routes` |
| CNA::Content::Pipeline::VideoProcessor.ValidateParameters | `wk-routes` |
| CNA::Content::Pipeline::WavImporter | `wk-routes` |
| CNA::DesktopOS | `dev-identity` |
| CNA::Devices::Camera | `dev-camera` |
| CNA::Devices::Clipboard | `in-text` |
| CNA::Devices::Clipboard.HasText | `in-text` |
| CNA::Devices::Clipboard.Text | `in-text` |
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
| CNA::GetAssemblyTitleEXT | `app-device` |
| CNA::GetProjectGraphicsProfileEXT | `app-device` |
| CNA::GetRendererFeatureName | `rv-capabilities` |
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
| CNA::ProjectGraphicsProfileEXT | `app-device` |
| CNA::RendererFeature | `rv-capabilities` |
| CNA::RendererFeatureSupport | `rv-capabilities` |
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
| CNA_BUFFER_USAGE_NONE | `cx-capi` |
| CNA_BUFFER_USAGE_WRITE_ONLY | `cx-capi` |
| CNA_BoundingBox | `cx-capi` |
| CNA_BoundingFrustum | `cx-capi` |
| CNA_BoundingSphere | `cx-capi` |
| CNA_COLOR_BLACK | `cx-capi` |
| CNA_COLOR_CORNFLOWER_BLUE | `cx-capi` |
| CNA_COLOR_RED | `cx-capi` |
| CNA_COLOR_RGBA | `cx-capi` |
| CNA_COLOR_TRANSPARENT | `cx-capi` |
| CNA_COLOR_WHITE | `cx-capi` |
| CNA_CONTAINMENT_CONTAINS | `cx-capi` |
| CNA_CONTAINMENT_DISJOINT | `cx-capi` |
| CNA_CONTAINMENT_INTERSECTS | `cx-capi` |
| CNA_ContainmentType | `cx-capi` |
| CNA_CurveKey | `cx-capi` |
| CNA_DIAGNOSTICS_COUNTER_ADD | `pl-diagnostics` |
| CNA_DIAGNOSTICS_EVENT | `pl-diagnostics` |
| CNA_DIAGNOSTICS_FRAME_COUNTER_ADD | `pl-diagnostics` |
| CNA_DIAGNOSTICS_GAUGE_ADD | `pl-diagnostics` |
| CNA_DIAGNOSTICS_GAUGE_SET | `pl-diagnostics` |
| CNA_ErrorInfo | `cx-capi` |
| CNA_INDEX_ELEMENT_SIZE_SIXTEEN_BITS | `cx-capi` |
| CNA_Matrix | `cx-capi` |
| CNA_PLANE_INTERSECTION_BACK | `cx-capi` |
| CNA_PLANE_INTERSECTION_FRONT | `cx-capi` |
| CNA_PLANE_INTERSECTION_INTERSECTING | `cx-capi` |
| CNA_PRIMITIVE_TRIANGLE_LIST | `cx-capi` |
| CNA_PROFILE_SCOPE | `pl-diagnostics` |
| CNA_Plane | `cx-capi` |
| CNA_PlaneIntersectionType | `cx-capi` |
| CNA_Quaternion | `cx-capi` |
| CNA_Ray | `cx-capi` |
| CNA_VERTEX_ELEMENT_FORMAT_COLOR | `cx-capi` |
| CNA_VERTEX_ELEMENT_FORMAT_VECTOR2 | `cx-capi` |
| CNA_VERTEX_ELEMENT_FORMAT_VECTOR3 | `cx-capi` |
| CNA_VERTEX_ELEMENT_USAGE_COLOR | `cx-capi` |
| CNA_VERTEX_ELEMENT_USAGE_POSITION | `cx-capi` |
| CNA_VERTEX_ELEMENT_USAGE_TEXTURE_COORDINATE | `cx-capi` |
| CNA_Vector3 | `cx-capi` |
| CNA_Vector4 | `cx-capi` |
| CNA_VertexElement | `cx-capi` |
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
| CanonicalReflectiveReaderNameEXT | `wk-typewriter` |
| Capability | `pl-inspector` |
| ChunkIdToString | `wk-cnbloader` |
| CleanContent | `wk-contentproject` |
| CleanContent.BuildConfiguration | `wk-contentproject` |
| CleanContent.ContentProjectGUID | `wk-contentproject` |
| CleanContent.Execute | `wk-contentproject` |
| CleanContent.IntermediateDirectory | `wk-contentproject` |
| CleanContent.OutputDirectory | `wk-contentproject` |
| CleanContent.RootDirectory | `wk-contentproject` |
| CleanContent.TargetPlatform | `wk-contentproject` |
| CleanContent.TargetProfile | `wk-contentproject` |
| CleanContent.XnaTypeName | `wk-contentproject` |
| ClearOptions.Stencil | `rv-depth` |
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
| CnbAssetTypeIdFromName | `wk-cnbloader` |
| CnbByteReader | `wk-cnbloader` |
| CnbByteReader.ReadCount | `wk-cnbloader` |
| CnbByteReader.ReadF32 | `wk-cnbloader` |
| CnbByteReader.ReadF64 | `wk-cnbloader` |
| CnbByteReader.ReadI32 | `wk-cnbloader` |
| CnbByteReader.ReadString | `wk-cnbloader` |
| CnbByteReader.ReadU64 | `wk-cnbloader` |
| CnbByteReader.RequireExhausted | `wk-cnbloader` |
| CnbByteWriter | `wk-cnbloader` |
| CnbByteWriter.Take | `wk-cnbloader` |
| CnbByteWriter.WriteF32 | `wk-cnbloader` |
| CnbByteWriter.WriteF64 | `wk-cnbloader` |
| CnbByteWriter.WriteI32 | `wk-cnbloader` |
| CnbByteWriter.WriteString | `wk-cnbloader` |
| CnbByteWriter.WriteU32 | `wk-cnbloader` |
| CnbByteWriter.WriteU64 | `wk-cnbloader` |
| CnbChunkEntry.IsMandatory | `wk-cnbloader` |
| CnbCompression | `wk-incremental` |
| CnbCompressionToString | `wk-incremental` |
| CnbDocument | `wk-cnbloader` |
| CnbDocument.AssetSchemaVersion | `wk-cnbloader` |
| CnbDocument.AssetTypeId | `wk-cnbloader` |
| CnbDocument.ChunkAt | `wk-cnbloader` |
| CnbDocument.ChunkData | `wk-cnbloader` |
| CnbDocument.FindSingle | `wk-cnbloader` |
| CnbDocument.HasMagic | `wk-cnbloader` |
| CnbDocument.Metadata | `wk-cnbloader`, `wk-incremental` |
| CnbDocument.OpenChunk | `wk-cnbloader` |
| CnbDocument.Parse | `wk-cnbloader`, `wk-routes` |
| CnbDocument.RequireAsset | `wk-cnbloader` |
| CnbDocument.RequireSingle | `wk-cnbloader` |
| CnbImageImportOptions | `wk-incremental` |
| CnbLoaderRegistry | `wk-cnbloader` |
| CnbLoaderRegistry.IsRegistered | `wk-cnbloader` |
| CnbLoaderRegistry.Register | `wk-cnbloader` |
| CnbLoaderRegistry.RegisteredTypeName | `wk-cnbloader` |
| CnbLoaderRegistry.Remove | `wk-cnbloader` |
| CnbWriter | `wk-cnbloader` |
| CnbWriter.AddChunk | `wk-cnbloader` |
| CnbWriter.Build | `wk-cnbloader` |
| CnbWriter.SetMetadata | `wk-cnbloader` |
| CnjImporter | `wk-routes` |
| CnjToCnbResult | `wk-incremental` |
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
| CompileCnjToCnb | `wk-incremental` |
| CompileOptions | `ct-compiler`, `wk-typewriter` |
| CompileOptions.compressContent | `ct-compiler` |
| CompileOptions.targetPlatform | `ct-compiler` |
| CompileOptions.targetProfile | `ct-compiler` |
| CompiledEffectContent | `fx-compiled` |
| CompiledEffectContent.GetEffectCode | `fx-compiled` |
| CompiledEffectHasNoCnbSchemaReason | `wk-routes` |
| CompiledEffectImporter | `wk-routes` |
| CompiledEffectProcessor | `wk-routes` |
| Compiler.ContentTypeWriter | `wk-typewriter`, `wk-processor` |
| CompressCnbChunk | `wk-incremental` |
| ComputeContentBuildDirectFingerprint | `wk-incremental` |
| ComputeContentBuildEffectiveFingerprint | `wk-incremental` |
| ComputeContentBuildFingerprint | `wk-incremental` |
| ContainmentType | `col-box-box`, `math-checks` |
| ContentAssetBuildConfiguration | `wk-incremental` |
| ContentBuildConfiguration | `wk-incremental` |
| ContentBuildConfiguration.Entries | `wk-incremental` |
| ContentBuildConfiguration.Find | `wk-incremental` |
| ContentBuildConfiguration.OutputFormat | `wk-incremental` |
| ContentBuildConfiguration.Parse | `wk-incremental` |
| ContentBuildConfiguration.SourceRoots | `wk-incremental` |
| ContentBuildConfigurationFileName | `wk-incremental` |
| ContentBuildConfigurationVersion | `wk-incremental` |
| ContentBuildEnvironment | `wk-processor` |
| ContentBuildFingerprintState | `wk-incremental` |
| ContentBuildLogger.LogImportantMessage | `wk-processor` |
| ContentBuildLogger.LogMessage | `wk-processor`, `wk-tasks` |
| ContentBuildManifest | `wk-incremental` |
| ContentBuildManifest.Clear | `wk-incremental` |
| ContentBuildManifest.Entries | `wk-incremental` |
| ContentBuildManifest.Find | `wk-incremental` |
| ContentBuildManifest.Parse | `wk-incremental`, `wk-contentproject` |
| ContentBuildManifest.Serialize | `wk-incremental` |
| ContentBuildManifest.Set | `wk-incremental` |
| ContentBuildManifestEntry | `wk-incremental` |
| ContentBuildManifestFileName | `wk-incremental` |
| ContentBuildManifestOutput | `wk-incremental` |
| ContentBuildManifestVersion | `wk-incremental` |
| ContentBuildRequest | `wk-processor` |
| ContentBuildResult | `wk-processor` |
| ContentCompiler | `ct-compiler`, `wk-typewriter` |
| ContentCompiler.AddTypeWriter | `wk-typewriter`, `wk-processor` |
| ContentCompiler.Compile | `ct-compiler`, `wk-typewriter` |
| ContentCompiler.Compile(CompiledEffectContent) | `fx-compiled` |
| ContentCompiler.Compile(SongContent) | `ct-audiopipeline` |
| ContentCompiler.Compile(SoundEffectContent) | `ct-audiopipeline` |
| ContentCompiler.Compile(VideoContent) | `au-video` |
| ContentCompiler.GetTypeWriter | `ct-compiler`, `wk-typewriter` |
| ContentCompilerOptions | `wk-tasks` |
| ContentCompilerOptions.buildConfiguration | `wk-tasks` |
| ContentDependency | `wk-processor`, `wk-incremental` |
| ContentDependencyCollector.Dependencies | `wk-material` |
| ContentDeploymentFile | `wk-processor` |
| ContentFileSha256 | `wk-incremental`, `wk-contentproject` |
| ContentIdentity | `ct-fontbuild`, `wk-processor` |
| ContentImporterAttribute | `ct-intermediate`, `ct-modelimport`, `ct-audiopipeline`, `wk-processor` |
| ContentImporterAttribute.DefaultProcessor | `wk-importers` |
| ContentImporterAttribute.DisplayName | `wk-processor` |
| ContentImporterAttribute.FileExtensions | `wk-importers` |
| ContentImporterContext.Logger | `wk-processor` |
| ContentItem | `ct-intermediate`, `wk-typewriter` |
| ContentLoadException | `app-content`, `ct-failure`, `ct-custom`, `ct-resource`, `pl-exceptions`, `wk-typewriter`, `wk-cnbloader`, `wk-gltf` |
| ContentLogMessage | `wk-processor` |
| ContentManager | `models`, `app-content` |
| ContentManager.GetContentManifest | `ct-pipeline` |
| ContentManager.Load | `app-content`, `ct-cache`, `ct-failure`, `ct-pipeline`, `ct-reader`, `ct-resource`, `wk-typewriter`, `wk-cnbloader`, `wk-processor`, `wk-gltf`, `wk-routes`, `wk-contentproject` |
| ContentManager.Load<Effect> | `fx-compiled` |
| ContentManager.Load<Model> | `model-load`, `ct-load` |
| ContentManager.Load<SoundEffect> | `au-basics`, `ct-load` |
| ContentManager.Load<Texture2D> | `ct-load`, `ct-formats` |
| ContentManager.Load<Video> | `au-video` |
| ContentManager.OpenStream | `ct-compiler`, `wk-typewriter` |
| ContentManager.ReadAsset | `ct-compiler` |
| ContentManager.RegisterCnjLoader | `ct-custom` |
| ContentManager.RegisterTypeReader | `ct-custom` |
| ContentManager.RootDirectory | `ct-load` |
| ContentManager.RouteLoadsThroughOpenStreamEXT | `ct-compiler` |
| ContentManager.ServiceProvider | `ct-load` |
| ContentManager.Unload | `app-content`, `ct-cache`, `ct-lifetime` |
| ContentOutputFormatExtension | `wk-routes` |
| ContentPipeline | `wk-material`, `wk-processor` |
| ContentPipeline.Build | `wk-processor`, `wk-routes` |
| ContentPipelineError | `wk-routes` |
| ContentPipelineError.Stage | `wk-routes` |
| ContentPipelineRegistry | `wk-material`, `wk-processor` |
| ContentPipelineRegistry.AbsentWriterReason | `wk-tasks` |
| ContentPipelineRegistry.DocumentAbsentWriter | `wk-tasks` |
| ContentPipelineRegistry.HasImporterForSource | `wk-routes` |
| ContentPipelineRegistryFactory | `wk-tasks`, `wk-contentproject` |
| ContentPipelineStageName | `wk-routes` |
| ContentProcessor.InputTypeName | `wk-importers` |
| ContentProcessor.OutputTypeName | `wk-importers` |
| ContentProcessorAttribute | `wk-processor` |
| ContentProcessorAttribute.DisplayName | `wk-processor` |
| ContentProcessorContext.AddDependency | `wk-processor` |
| ContentProcessorContext.AddOutputFile | `wk-processor` |
| ContentProcessorContext.BuildAndLoadAsset | `wk-processor` |
| ContentProcessorContext.BuildAsset | `wk-processor` |
| ContentProcessorContext.BuildConfiguration | `wk-processor` |
| ContentProcessorContext.Convert | `ct-modelimport`, `wk-processor` |
| ContentProcessorContext.IntermediateDirectory | `wk-processor` |
| ContentProcessorContext.NestedOutputs | `wk-material` |
| ContentProcessorContext.OutputDirectory | `wk-processor` |
| ContentProcessorContext.OutputFilename | `wk-processor` |
| ContentProcessorContext.TargetPlatform | `wk-processor` |
| ContentProcessorContext.TargetProfile | `wk-processor` |
| ContentProcessorParameters | `wk-processor` |
| ContentProcessorParameters.Find | `wk-incremental` |
| ContentProcessorParameters.Set | `wk-incremental` |
| ContentProject.CopiedFiles | `wk-contentproject` |
| ContentProject.DirectoryEXT | `wk-contentproject` |
| ContentProject.Item.Get | `wk-contentproject` |
| ContentProject.Items | `wk-contentproject` |
| ContentProject.Load | `wk-contentproject` |
| ContentProject.Parse | `wk-contentproject` |
| ContentProject.Property | `wk-contentproject` |
| ContentProject.SourceAssets | `wk-contentproject` |
| ContentProject.ToBuildContentEXT | `wk-contentproject` |
| ContentProject.UnroutableEXT | `wk-contentproject` |
| ContentReader | `ct-reader` |
| ContentReader.AssetName | `ct-reader`, `wk-typewriter` |
| ContentReader.ContentManager | `ct-reader` |
| ContentReader.Platform | `ct-reader` |
| ContentReader.ReadBoundingSphere | `ct-reader` |
| ContentReader.ReadColor | `ct-reader`, `wk-typewriter` |
| ContentReader.ReadMatrix | `ct-reader`, `wk-typewriter` |
| ContentReader.ReadObject | `wk-typewriter` |
| ContentReader.ReadQuaternion | `ct-reader`, `wk-typewriter` |
| ContentReader.ReadRawObject | `wk-typewriter` |
| ContentReader.ReadSharedResource | `wk-typewriter` |
| ContentReader.ReadVector2 | `ct-reader`, `wk-typewriter` |
| ContentReader.ReadVector3 | `ct-reader`, `wk-typewriter` |
| ContentReader.ReadVector4 | `ct-reader`, `wk-typewriter` |
| ContentReader.Version | `ct-reader` |
| ContentSerializerAttribute | `ct-intermediate` |
| ContentSerializerAttribute.ElementName | `wk-cnbloader` |
| ContentSerializerCollectionItemNameAttribute | `ct-intermediate` |
| ContentSerializerCollectionItemNameAttribute.CollectionItemName | `ct-intermediate` |
| ContentSha256 | `wk-incremental` |
| ContentTypeDescriptor | `ct-intermediate` |
| ContentTypeReader | `ct-reader`, `wk-typewriter`, `wk-processor` |
| ContentTypeReaderBase.TypeVersion | `wk-typewriter` |
| ContentTypeReaderManager.AddTypeCreator | `ct-reader`, `wk-typewriter`, `wk-processor` |
| ContentTypeReaderManager.GetTypeReader | `ct-reader` |
| ContentTypeReaderManager.IsRegistered | `wk-typewriter` |
| ContentTypeReaderManager.RemoveTypeCreatorEXT | `wk-typewriter` |
| ContentTypeSerializer | `wk-cnbloader` |
| ContentTypeSerializerAttribute | `wk-cnbloader` |
| ContentTypeWriterAttribute | `wk-typewriter` |
| ContentTypeWriterBase.GetRuntimeReader | `wk-typewriter` |
| ContentTypeWriterBase.TargetType | `wk-typewriter` |
| ContentTypeWriterBase.TypeVersion | `wk-typewriter` |
| ContentWriter | `wk-typewriter` |
| ContentWriter.Write | `wk-typewriter` |
| ContentWriter.Write7BitEncodedInt | `wk-typewriter` |
| ContentWriter.WriteExternalReference | `wk-processor` |
| ContentWriter.WriteObject | `wk-typewriter` |
| ContentWriter.WriteRawObject | `wk-typewriter` |
| ContentWriter.WriteSharedResource | `wk-typewriter` |
| ConversionQuality | `ct-audiopipeline` |
| Crc32c | `wk-cnbloader` |
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
| Curve.Evaluate | `wk-routes` |
| Curve.IsConstant | `curve-class` |
| Curve.PostLoop | `curve-class` |
| Curve.PreLoop | `curve-class` |
| CurveContentWriter | `wk-routes` |
| CurveContinuity | `curve-class` |
| CurveKey | `curve-class` |
| CurveKey.TangentIn | `curve-class` |
| CurveKey.TangentOut | `curve-class` |
| CurveKeyCollection | `curve-class` |
| CurveLoopType | `curve-class` |
| CurveProcessor | `wk-routes` |
| CurveTangent | `curve-class` |
| DeactivatedEventArgs | `pl-window` |
| DebugDraw.addBox | `cx-debugdraw` |
| DebugDraw.addFrustum | `cx-debugdraw` |
| DebugDraw.addSphere | `cx-debugdraw` |
| DebugDraw.setDepthTested | `cx-debugdraw` |
| DecodeWavAsCnbSoundEffect | `wk-incremental` |
| DecodeWavAsImportedSound | `wk-incremental` |
| DecompressCnbChunk | `wk-incremental` |
| DepthEffectMode | `cx-depth` |
| DepthFormat | `cam-clip`, `rt-basic` |
| DepthFormat.Depth24 | `rv-depth` |
| DepthFormat.Depth24Stencil8 | `rv-depth` |
| DepthStencilState | `sky`, `renderstates`, `cam-clip`, `rv-depth` |
| DepthStencilState.CounterClockwiseStencilFunction | `rs-stencilcount` |
| DepthStencilState.CounterClockwiseStencilPass | `rs-stencilcount` |
| DepthStencilState.DepthBufferEnable | `rs-depthfunc` |
| DepthStencilState.DepthBufferFunction | `light-many`, `rs-depthfunc`, `rs-stencilcount` |
| DepthStencilState.DepthBufferWriteEnable | `rs-depthread` |
| DepthStencilState.DepthRead | `be-alpha`, `rs-depthread` |
| DepthStencilState.ReferenceStencil | `rs-outline`, `rv-depth` |
| DepthStencilState.StencilDepthBufferFail | `rs-stencilcount` |
| DepthStencilState.StencilEnable | `rs-portal`, `rv-depth` |
| DepthStencilState.StencilFail | `rs-stencilcount` |
| DepthStencilState.StencilFunction | `rs-portal`, `rv-depth` |
| DepthStencilState.StencilMask | `rs-stencilcount` |
| DepthStencilState.StencilPass | `rs-outline`, `rs-shadow`, `rs-stencilcount`, `rv-depth` |
| DepthStencilState.StencilWriteMask | `rs-stencilcount` |
| DepthStencilState.TwoSidedStencilMode | `rs-stencilcount` |
| DeviceType | `in-phone` |
| DirectionalLight | `lighting`, `be-lights`, `light-types` |
| DitherMode | `cx-depth` |
| DrawableGameComponent | `app-components` |
| DualTextureEffect | `be-dual`, `rv-effects` |
| DualTextureEffect.Texture2 | `be-dual` |
| DualTextureMaterialContent | `wk-material` |
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
| EffectMaterialContent | `wk-material` |
| EffectMaterialContent.Effect | `wk-material` |
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
| EnumTypeReader | `wk-typewriter` |
| EnvironmentMapEffect | `tex-cube`, `rt-cube` |
| EnvironmentMapEffect.EnvironmentMap | `rt-cube` |
| EnvironmentMapMaterialContent | `wk-material` |
| EvaluateMorphWeightsEXT | `model-morph` |
| EventsResponse | `pl-inspector` |
| ExternalReference | `wk-material`, `wk-processor` |
| ExternalReference.Filename | `wk-material`, `wk-processor` |
| FbxImporter | `ct-modelimport` |
| FileDialog.IsSupported | `dev-dialogs` |
| FileDialog.ResultCallback | `dev-dialogs` |
| FileDialog.ShowOpenFile | `dev-dialogs` |
| FileDialog.ShowOpenFolder | `dev-dialogs` |
| FileDialog.ShowSaveFile | `dev-dialogs` |
| FillMode | `rv-capabilities` |
| Framework.Content.Pipeline.ContentImporter | `wk-processor` |
| Framework.Content.Pipeline.ContentImporter.Import | `wk-processor` |
| Framework.Content.Pipeline.ContentProcessor | `wk-processor` |
| Framework.Content.Pipeline.ContentProcessor.Process | `wk-processor` |
| Framework.Content.Pipeline.ContentProcessorContext | `wk-processor` |
| Framework.Content.Pipeline.ContentProcessorContext.Logger | `wk-processor` |
| Framework.Content.Pipeline.ContentProcessorContext.Parameters | `wk-processor` |
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
| GamerJoinedEventArgs | `net-session` |
| GamerJoinedEventArgs.Gamer | `net-session` |
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
| GetLastOutputs | `wk-contentproject` |
| GetLastOutputs.Execute | `wk-contentproject` |
| GetLastOutputs.IntermediateDirectory | `wk-contentproject` |
| GetLastOutputs.OutputContentFiles | `wk-contentproject` |
| GetLastOutputs.setOutputDirectoryEXT | `wk-contentproject` |
| GltfImportDiagnosticEXT | `model-report`, `wk-gltf` |
| GltfImportDiagnosticKindEXT | `wk-gltf` |
| GltfImportReportEXT | `model-report`, `wk-gltf` |
| GltfImportReportEXT.Diagnostics | `wk-gltf` |
| GltfImportReportEXT.MeshInstanceCount | `wk-gltf` |
| GltfImportReportEXT.NodeCount | `wk-gltf` |
| GltfImporter | `wk-routes` |
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
| GraphicsDevice.GetRendererCapabilityProfileEXT | `pl-renderer`, `rv-capabilities` |
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
| GraphicsDevice.SetRenderTargets | `rt-mrt`, `rv-capabilities` |
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
| ImportImageAsCnbTexture2D | `wk-incremental` |
| ImportWavAsCnbSoundEffect | `wk-incremental` |
| ImportWavAsImportedSound | `wk-routes`, `wk-incremental` |
| ImportedXnbVideoType | `wk-routes` |
| IndexBuffer | `particles`, `geo-indexed`, `geo-procedural`, `geo-index32`, `geo-dynindex`, `geo-terrain`, `model-skinfx`, `pt-fire`, `pt-smoke`, `pt-sparks`, `pt-snow`, `pt-rain`, `pt-fountain`, `pt-dust`, `pt-fireworks`, `pt-bubbles`, `pt-leaves`, `pt-magic` |
| IndexCollection | `wk-animation` |
| IndexCollection.AddRange | `wk-animation` |
| IndexElementSize | `geo-index32` |
| InputDevices.GetMiceEXT | `in-devices` |
| IntermediateReader.ReadObject | `wk-cnbloader` |
| IntermediateSerializer | `ct-intermediate` |
| IntermediateSerializer.AddTypeSerializer | `wk-cnbloader` |
| IntermediateSerializer.Deserialize | `ct-intermediate`, `wk-cnbloader` |
| IntermediateSerializer.FindTypeSerializer | `wk-cnbloader` |
| IntermediateSerializer.GetTypeSerializer | `ct-intermediate` |
| IntermediateSerializer.Serialize | `ct-intermediate`, `wk-cnbloader` |
| IntermediateWriter.WriteObject | `wk-cnbloader` |
| InvalidContentException | `ct-intermediate`, `wk-cnbloader`, `wk-material`, `wk-animation`, `wk-contentproject` |
| InviteAcceptedEventArgs | `net-session` |
| InviteAcceptedEventArgs.Gamer | `net-session` |
| InviteAcceptedEventArgs.IsCurrentSession | `net-session` |
| IsCnbCompressionSupported | `wk-incremental` |
| IsCompiledEffectBinary | `wk-routes` |
| IsCustomAssetTypeId | `wk-cnbloader` |
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
| Keys | `in-edges`, `wk-xml` |
| KnownXnaImporterNames | `wk-tasks` |
| KnownXnaProcessorNames | `wk-tasks` |
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
| LocalNetworkGamer.SendPartyInvites | `net-session` |
| Logger.ErrorIf | `pl-log` |
| Logger.GetMinimumLevel | `pl-log` |
| Logger.Info | `pl-log` |
| Logger.InfoIf | `pl-log` |
| Logger.Log | `pl-log` |
| Logger.ResetSink | `pl-log` |
| Logger.SetMinimumLevel | `pl-log` |
| Logger.SetSink | `pl-log` |
| Logger.Warn | `pl-log` |
| Logger.WarnIf | `pl-log` |
| LooseFileContentTypeReader | `ct-custom` |
| MakeBlockCompressionTextureEncoder | `wk-material`, `wk-routes` |
| MakeChunkId | `wk-cnbloader` |
| MakeContentBuildManifestEntry | `wk-incremental` |
| MakeExternalXmaEncoder | `wk-importers` |
| MakeProcessedSoundEffect | `wk-routes` |
| MakeUnavailableXmaEncoder | `wk-importers` |
| MapXnaImporterName | `wk-tasks` |
| MapXnaProcessorName | `wk-tasks` |
| MaterialContent | `ct-modelimport`, `wk-material` |
| MaterialContent.Textures | `wk-material`, `wk-processor` |
| MaterialProcessor | `wk-material` |
| MaterialProcessor.ColorKeyColor | `wk-material` |
| MaterialProcessor.ColorKeyEnabled | `wk-material` |
| MaterialProcessor.DefaultEffect | `wk-material` |
| MaterialProcessor.GenerateMipmaps | `wk-material` |
| MaterialProcessor.PremultiplyTextureAlpha | `wk-material` |
| MaterialProcessor.Process | `wk-material` |
| MaterialProcessor.ResizeTexturesToPowerOfTwo | `wk-material` |
| MaterialProcessor.TextureFormat | `wk-material` |
| MaterialProcessorDefaultEffect | `wk-material` |
| MathHelper | `an-pendulum`, `sim-integrators`, `rf-deck` |
| MathHelper.CatmullRom | `curve-easing` |
| MathHelper.Clamp | `math-checks` |
| MathHelper.Hermite | `curve-easing` |
| MathHelper.Lerp | `curve-easing`, `math-checks`, `geo-terrain`, `an-doors` |
| MathHelper.SmoothStep | `curve-easing`, `an-doors` |
| MathHelper.ToDegrees | `sim-ik` |
| MathHelper.ToRadians | `sim-ik` |
| MathHelper.WrapAngle | `math-checks`, `an-walker` |
| MathTypeConverter | `vt-converters` |
| MathTypeConverter.CanConvertFrom | `vt-converters` |
| MathTypeConverter.GetProperties | `vt-converters` |
| Matrix | `models`, `animation`, `fx-colormatrix`, `wk-xml` |
| Matrix.Add | `math-checks` |
| Matrix.CreateBillboard | `mat-billboard`, `math-checks` |
| Matrix.CreateConstrainedBillboard | `mat-billboard`, `math-checks` |
| Matrix.CreateFromAxisAngle | `math-checks` |
| Matrix.CreateFromQuaternion | `quat-euler`, `math-checks` |
| Matrix.CreateFromYawPitchRoll | `math-checks` |
| Matrix.CreateLookAt | `mat-view`, `math-checks`, `cam-orbit`, `cam-chase`, `cam-topdown`, `cam-mirror`, `rt-mirror`, `wk-xml`, `rv-camera`, `rf-telescope` |
| Matrix.CreateOrthographic | `mat-ortho`, `math-checks`, `cam-topdown`, `cx-shadow`, `rv-camera` |
| Matrix.CreateOrthographicOffCenter | `mat-offcenter`, `math-checks` |
| Matrix.CreatePerspective | `mat-offcenter`, `math-checks` |
| Matrix.CreatePerspectiveFieldOfView | `mat-perspective`, `mat-ortho`, `col-frustum`, `math-checks`, `cam-security`, `cam-fov`, `cam-clip`, `wk-xml`, `rv-camera`, `rf-telescope` |
| Matrix.CreatePerspectiveOffCenter | `mat-offcenter`, `math-checks` |
| Matrix.CreateReflection | `mat-reflection`, `math-checks` |
| Matrix.CreateRotationX | `quat-euler`, `math-checks`, `an-gyroscope` |
| Matrix.CreateRotationY | `grounds-sculpture`, `mat-order`, `math-checks`, `an-gears`, `an-gyroscope` |
| Matrix.CreateRotationZ | `math-checks`, `sprite-camera`, `an-clock`, `an-kinetic`, `an-gyroscope`, `an-walker` |
| Matrix.CreateScale | `grounds-sculpture`, `mat-order`, `math-checks`, `sprite-camera`, `sprite-tilemap`, `sim-flock` |
| Matrix.CreateShadow | `math-checks`, `rs-shadow` |
| Matrix.CreateTranslation | `mat-order`, `math-checks`, `sprite-camera`, `sim-flock` |
| Matrix.CreateWorld | `math-checks`, `cam-transition` |
| Matrix.Decompose | `mat-hierarchy` |
| Matrix.Determinant | `mat-reflection` |
| Matrix.Divide | `math-checks` |
| Matrix.Invert | `mat-view`, `mat-normals`, `math-checks`, `model-cameras` |
| Matrix.Lerp | `math-checks` |
| Matrix.Multiply | `mat-order`, `mat-hierarchy`, `mat-screen`, `math-checks`, `an-clock`, `an-gears`, `an-kinetic`, `an-walker` |
| Matrix.Negate | `math-checks` |
| Matrix.Subtract | `math-checks` |
| Matrix.Transform | `math-checks` |
| Matrix.Translation | `mat-hierarchy` |
| Matrix.Transpose | `mat-normals`, `math-checks` |
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
| MediaSource.MediaSourceType | `dev-medialib` |
| MediaSource.Name | `dev-medialib` |
| MediaSourceType | `dev-medialib` |
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
| Microphone.All | `au-mic` |
| Microphone.BufferDuration | `au-mic` |
| Microphone.BufferReady | `au-mic` |
| Microphone.Default | `au-mic` |
| Microphone.GetData | `au-mic` |
| Microphone.GetSampleSizeInBytes | `au-mic` |
| Microphone.IsHeadset | `au-mic` |
| Microphone.SampleRate | `au-mic` |
| Microphone.Start | `au-mic` |
| Microphone.Stop | `au-mic` |
| Microsoft.Devices.Environment.DeviceType | `in-phone` |
| Microsoft::Xna::Framework::Content::Pipeline::ContentImporter | `ct-modelimport` |
| Microsoft::Xna::Framework::Content::Pipeline::ContentImporterContext | `ct-modelimport` |
| Microsoft::Xna::Framework::Content::Pipeline::FontDescriptionImporter | `ct-fontbuild` |
| Microsoft::Xna::Framework::Content::Pipeline::FontDescriptionImporter.Import | `ct-fontbuild` |
| Microsoft::Xna::Framework::Content::Pipeline::TargetPlatform | `ct-compiler` |
| Microsoft::Xna::Framework::Content::Pipeline::WavImporter | `ct-audiopipeline` |
| Microsoft::Xna::Framework::Content::Pipeline::WavImporter.Import | `ct-audiopipeline` |
| MipmapChain | `ct-processor` |
| Model | `models`, `model-load`, `wk-gltf` |
| Model.BoundingSphereEXT | `model-load`, `model-instances`, `model-bounds` |
| Model.CamerasEXT | `model-cameras` |
| Model.CopyAbsoluteBoneTransformsTo | `model-rigid`, `model-sampler` |
| Model.CopyBoneTransformsFrom | `model-load` |
| Model.CopyBoneTransformsTo | `model-load` |
| Model.Draw | `model-instances` |
| Model.GltfImportReportEXT | `wk-gltf` |
| Model.MaterialVariantEXT | `model-variants` |
| Model.MaterialVariantNamesEXT | `model-variants` |
| Model.Meshes | `wk-gltf`, `wk-routes` |
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
| ModelContentWriter | `wk-routes` |
| ModelGenerateChildAssetsParameter | `wk-routes` |
| ModelMesh | `models`, `model-load` |
| ModelMesh.BoundingSphere | `model-bounds` |
| ModelMesh.Draw | `model-sampler` |
| ModelMesh.MeshParts | `wk-gltf` |
| ModelMeshContent | `ct-meshbuilder`, `ct-modelimport` |
| ModelMeshPart | `model-load` |
| ModelMeshPart.Effect | `model-effects`, `wk-gltf` |
| ModelMeshPart.NumVertices | `wk-gltf` |
| ModelMeshPart.PrimitiveTypeEXT | `model-load` |
| ModelMeshPart.SamplerStateEXT | `model-sampler` |
| ModelMeshPart.SamplerStatesEXT | `model-load`, `model-sampler` |
| ModelMeshPart.Tag | `model-morph`, `wk-gltf` |
| ModelMeshPart.VertexBuffer | `wk-gltf` |
| ModelMeshPart.VertexOffset | `wk-gltf` |
| ModelMeshPartContent | `ct-meshbuilder`, `ct-modelimport` |
| MorphTargetDataEXT | `model-morph`, `wk-gltf` |
| MorphTargetDataEXT.BaseVertexBytes | `wk-gltf` |
| MorphTargetDataEXT.PositionDeltas | `wk-gltf` |
| MorphTargetDataEXT.Stride | `wk-gltf` |
| MorphTargetDataEXT.Weights | `wk-gltf` |
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
| NetworkSession.GameEnded | `net-session` |
| NetworkSession.GameStarted | `net-session` |
| NetworkSession.GamerJoined | `net-session` |
| NetworkSession.GamerLeft | `net-session` |
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
| NodeContent.Animations | `wk-animation` |
| NodeContent.Children | `ct-modelimport`, `wk-meshhelper` |
| NormalizedByte2 | `vt-packed` |
| NormalizedByte4 | `vt-packed` |
| NormalizedShort2 | `vt-packed` |
| NormalizedShort4 | `vt-packed` |
| NotificationChannelUriEventArgs | `dev-notify` |
| NotificationChannelUriEventArgs.ChannelUri | `dev-notify` |
| OcclusionQuery | `rs-occlusion`, `rv-capabilities` |
| OcclusionQuery.Begin | `rv-capabilities` |
| OcclusionQuery.End | `rv-capabilities` |
| OcclusionQuery.IsComplete | `rs-occlusion`, `rv-capabilities` |
| OcclusionQuery.PixelCount | `rs-occlusion` |
| OpaqueDataDictionary | `wk-processor` |
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
| PassThroughProcessor | `wk-importers` |
| PassThroughProcessor.Process | `wk-importers` |
| PbrEffect | `model-pbr`, `model-effects`, `model-variants`, `wk-gltf`, `rv-shading` |
| PbrEffect.AlphaCutoffEXT | `model-material` |
| PbrEffect.DiffuseColor | `rv-shading` |
| PbrEffect.EmissiveFactor | `model-pbr`, `rv-shading` |
| PbrEffect.EmissiveMap | `model-material` |
| PbrEffect.ImageBasedLightEXT | `model-ibl` |
| PbrEffect.IorEXT | `wk-gltf` |
| PbrEffect.MetallicFactor | `model-pbr`, `model-ibl`, `wk-gltf`, `rv-shading` |
| PbrEffect.NormalMap | `model-material` |
| PbrEffect.RoughnessFactor | `model-pbr`, `model-ibl`, `wk-gltf`, `rv-shading` |
| PbrEffect.SpecularColorFactorEXT | `wk-gltf` |
| PbrEffect.SpecularFactorEXT | `wk-gltf` |
| PbrEffect.TextureCoordinateSetsEXT | `wk-gltf` |
| PbrEffect.TextureTransformsEXT | `wk-gltf` |
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
| PipelineComponentScanner | `wk-processor` |
| PipelineComponentScanner.Errors | `wk-processor` |
| PipelineComponentScanner.ImporterAttributes | `wk-processor` |
| PipelineComponentScanner.ImporterNames | `wk-processor` |
| PipelineComponentScanner.ImporterOutputTypes | `wk-processor` |
| PipelineComponentScanner.ProcessorAttributes | `wk-processor` |
| PipelineComponentScanner.ProcessorInputTypes | `wk-processor` |
| PipelineComponentScanner.ProcessorNames | `wk-processor` |
| PipelineComponentScanner.ProcessorOutputTypes | `wk-processor` |
| PipelineComponentScanner.ProcessorParameters | `wk-processor` |
| PipelineComponentScanner.Update | `wk-processor` |
| PipelineException | `wk-processor` |
| PixelBitmapContent | `ct-processor` |
| Plane | `vec-reflect`, `col-ray-plane` |
| Plane.Dot | `math-checks` |
| Plane.DotCoordinate | `col-plane-box`, `math-checks` |
| Plane.DotNormal | `col-plane-box`, `math-checks` |
| Plane.Intersects | `col-plane-box`, `math-checks` |
| Plane.Normalize | `math-checks` |
| Plane.Transform | `mat-reflection`, `math-checks` |
| PlaneConverter | `vt-converters` |
| PlaneIntersectionType | `col-plane-box`, `math-checks` |
| PlatformCapabilities | `pl-platform` |
| PlayerIndex | `in-players` |
| Playlist.Duration | `dev-medialib` |
| Playlist.Name | `dev-medialib` |
| Playlist.Songs | `dev-medialib` |
| PlaylistCollection | `dev-medialib` |
| Point | `vt-rectangle` |
| PointConverter | `vt-converters` |
| PositionCollection | `wk-animation` |
| Power.GetInfoEXT | `in-devices` |
| PresentInterval | `app-viewport` |
| PresentationParameters | `app-device`, `app-viewport`, `pl-adapter` |
| PreviewResponse | `pl-inspector` |
| PrimitiveType | `geo-primitives` |
| PrimitiveType.LineList | `rf-weather` |
| PrimitiveType.PointListEXT | `cx-points`, `pf-particles` |
| ProcessImportedSoundEffect | `wk-incremental` |
| ProcessedFrameBytes | `wk-routes` |
| ProcessedPcmBitsPerSample | `wk-routes` |
| ProcessedPcmEncoding | `wk-routes` |
| ProcessedPcmEncodingName | `wk-routes` |
| ProcessedPcmSampleBytes | `wk-routes` |
| ProcessedSoundEffect | `wk-routes` |
| ProcessedVideoType | `wk-routes` |
| ProcessorParameter | `wk-processor` |
| ProcessorParameterBindings | `wk-processor` |
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
| Quaternion | `wk-xml` |
| Quaternion.Add | `math-checks` |
| Quaternion.Concatenate | `quat-axis`, `math-checks` |
| Quaternion.Conjugate | `quat-axis`, `math-checks` |
| Quaternion.CreateFromAxisAngle | `quat-axis`, `math-checks` |
| Quaternion.CreateFromRotationMatrix | `math-checks` |
| Quaternion.CreateFromYawPitchRoll | `quat-euler`, `math-checks`, `wk-xml` |
| Quaternion.Divide | `math-checks` |
| Quaternion.Dot | `quat-slerp`, `math-checks` |
| Quaternion.Inverse | `quat-axis`, `math-checks` |
| Quaternion.Lerp | `quat-slerp`, `math-checks` |
| Quaternion.Multiply | `math-checks` |
| Quaternion.Negate | `math-checks` |
| Quaternion.Normalize | `quat-slerp`, `math-checks` |
| Quaternion.Slerp | `quat-slerp`, `math-checks`, `cam-transition` |
| Quaternion.Subtract | `math-checks` |
| QuaternionConverter | `vt-converters` |
| RasterizerState | `renderstates` |
| RasterizerState.CullCounterClockwise | `rt-mirror` |
| RasterizerState.CullMode | `geo-triangle`, `rs-cull`, `rv-geometry` |
| RasterizerState.DepthBias | `rs-depthbias` |
| RasterizerState.FillMode | `geo-procedural`, `geo-terrain`, `rs-fill`, `rv-capabilities` |
| RasterizerState.MultiSampleAntiAlias | `rt-msaa` |
| RasterizerState.ScissorTestEnable | `sprite-scissor`, `rs-scissor` |
| RasterizerState.SlopeScaleDepthBias | `rs-depthbias` |
| RawPushNotificationMessage | `dev-notify` |
| RawPushNotificationMessage.RawData | `dev-notify` |
| RawPushNotificationMessage.SendAsync | `dev-notify` |
| Ray.Intersects | `col-ray-box`, `col-ray-sphere`, `col-ray-plane`, `col-pick`, `math-checks` |
| RayConverter | `vt-converters` |
| Rectangle | `vt-rectangle`, `sprite-sheet`, `sprite-ui`, `sprite-anim` |
| Rectangle.Bottom | `vt-rectangle` |
| Rectangle.Contains | `math-checks`, `vt-rectangle` |
| Rectangle.Inflate | `vt-rectangle` |
| Rectangle.Intersect | `math-checks`, `vt-rectangle`, `sprite-hit` |
| Rectangle.Intersects | `math-checks`, `vt-rectangle`, `sprite-hit`, `sprite-tilemap` |
| Rectangle.IsEmpty | `vt-rectangle` |
| Rectangle.Location | `vt-rectangle` |
| Rectangle.Union | `math-checks`, `vt-rectangle` |
| RectangleConverter | `vt-converters` |
| ReflectiveSharedTypeReader | `wk-typewriter` |
| ReflectiveTypeReader | `wk-typewriter` |
| ReflectiveTypeReaderBuilder | `wk-typewriter` |
| ReflectiveTypeReaderBuilder.Base | `wk-typewriter` |
| ReflectiveTypeReaderBuilder.Custom | `wk-typewriter` |
| ReflectiveTypeReaderBuilder.EnumField | `wk-typewriter` |
| ReflectiveTypeReaderBuilder.Field | `wk-typewriter` |
| ReflectiveTypeReaderBuilder.Register | `wk-typewriter` |
| ReflectiveTypeReaderBuilder.RegisterShared | `wk-typewriter` |
| ReflectiveTypeReaderBuilder.SharedResourceField | `wk-typewriter` |
| RefreshContentBuildDirectFingerprint | `wk-incremental` |
| RefreshContentBuildEffectiveFingerprint | `wk-incremental` |
| RegisterBuiltInContentPipeline | `wk-contentproject` |
| RegisterCnjContentPipeline | `wk-routes` |
| RegisterCompiledEffectContentPipeline | `wk-routes` |
| RegisterModelContentPipeline | `wk-routes` |
| RegisterSongContentPipeline | `wk-routes` |
| RegisterSoundEffectContentPipeline | `wk-routes` |
| RegisterTexture2DContentPipeline | `wk-material`, `wk-routes` |
| RegisterVideoContentPipeline | `wk-routes` |
| RegisterXnaImporter | `wk-processor` |
| RegisterXnaModelSourceContentPipeline | `wk-material` |
| RegisterXnaProcessor | `wk-processor` |
| RegisterXnaXmlSourceContentPipeline | `wk-routes` |
| RegisterXnaXnbOutput | `wk-processor` |
| RegisterXnbContentPipeline | `wk-routes` |
| RegisterXnbOutputContentPipeline | `wk-material`, `wk-routes` |
| RenderTarget2D | `rendertarget`, `cam-orbit`, `cam-fov`, `cam-split`, `cam-mirror`, `sprite-basics`, `sprite-lights`, `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-swirl`, `fx-chromatic`, `fx-kernel`, `fx-halftone`, `fx-vertexwave`, `fx-colormatrix`, `rs-scissor`, `rt-basic`, `rt-lowres`, `rt-control`, `rt-recursive`, `rt-mirror`, `rt-bloom`, `rt-msaa`, `pf-targets`, `rv-targets`, `rf-telescope` |
| RenderTarget2D.ContentLost | `dev-window` |
| RenderTarget2D.IsContentLost | `dev-window` |
| RenderTarget2D.MultiSampleCount | `rt-msaa`, `rv-capabilities` |
| RenderTarget2D.NotifyContentLostEXT | `dev-window` |
| RenderTargetBinding | `rt-mrt`, `rt-cube`, `rv-capabilities` |
| RenderTargetBinding.CubeMapFace | `rt-cube` |
| RenderTargetBinding.RenderTarget | `rt-mrt`, `rt-cube` |
| RenderTargetCube | `rt-cube` |
| RenderTargetUsage.PreserveContents | `rt-recursive` |
| RendererCapabilityProfile | `app-device` |
| RendererCapabilityProfile.GetFeature | `rv-capabilities` |
| RendererDetail.FriendlyName | `au-xact` |
| RendererFeature | `pl-renderer` |
| RendererFeature.BaseInstanceDrawing | `geo-instancing` |
| RendererFeatureInfo | `rv-capabilities` |
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
| RunContentCompiler | `wk-contentproject` |
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
| SharpXmlEnumEntries | `wk-xml` |
| SharpXmlMembers | `wk-xml` |
| SharpXmlRootName | `wk-xml` |
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
| SkinnedMaterialContent | `wk-material` |
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
| Song.Duration | `au-music`, `dev-medialib`, `wk-routes` |
| Song.Genre | `dev-medialib` |
| Song.IsProtected | `au-music` |
| Song.IsRated | `au-music` |
| Song.Name | `dev-medialib` |
| Song.PlayCount | `au-music` |
| Song.Rating | `au-music` |
| Song.TrackNumber | `au-music`, `dev-medialib` |
| SongCollection | `au-music`, `dev-medialib` |
| SongContent | `ct-audiopipeline` |
| SongContentWriter | `wk-routes` |
| SongImporter | `wk-routes` |
| SongNameParameter | `wk-routes` |
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
| SoundEffectContentWriter | `wk-routes` |
| SoundEffectInstance | `grounds-fountain`, `audio`, `au-layers`, `rf-garden` |
| SoundEffectInstance.Apply3D | `au-spatial` |
| SoundEffectInstance.IsLooped | `au-layers` |
| SoundEffectInstance.Pitch | `au-layers` |
| SoundEffectInstance.Volume | `au-layers` |
| SoundEffectLoopPolicy | `wk-incremental` |
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
| StencilOperation | `rs-portal`, `rs-stencilcount`, `rv-depth` |
| StencilOperation.Increment | `rs-shadow` |
| StorageContainer | `ct-storage`, `pl-exceptions` |
| StorageContainer.CreateDirectory | `ct-storage` |
| StorageContainer.CreateFile | `ct-storage` |
| StorageContainer.DeleteDirectory | `ct-storage` |
| StorageContainer.DeleteFile | `ct-storage` |
| StorageContainer.DirectoryExists | `ct-storage` |
| StorageContainer.DisplayName | `ct-storage` |
| StorageContainer.Dispose | `ct-storage` |
| StorageContainer.Disposing | `ct-storage` |
| StorageContainer.FileExists | `ct-storage` |
| StorageContainer.GetDirectoryNames | `ct-storage` |
| StorageContainer.GetFileNames | `ct-storage` |
| StorageContainer.IsDisposed | `ct-storage` |
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
| StorageDeviceNotConnectedException | `ct-storage` |
| SurfaceFormat | `tex-formats`, `rt-formats`, `rv-formats` |
| SurfaceFormat.Dxt1 | `ct-formats` |
| SurfaceFormat.Single | `tex-volume` |
| SurfaceFormat.Vector4 | `tex-volume` |
| System.ArgumentNullException | `wk-material` |
| System.ArgumentOutOfRangeException | `pl-exceptions` |
| System.IO.FileNotFoundException | `pl-exceptions`, `wk-importers` |
| System.IO.MemoryStream | `tex-io` |
| System.InvalidOperationException | `pl-exceptions` |
| System.ObjectDisposedException | `pl-exceptions` |
| System.Resources.ResourceManager | `ct-resource` |
| System.Text.StringBuilder | `text-hud` |
| System::ArgumentException | `rv-vertextype` |
| System::Xml::Serialization::XmlSerializer | `wk-xml` |
| SystemTray.AddEntry | `dev-tray` |
| SystemTray.GetEntryChecked | `dev-tray` |
| SystemTray.GetEntryEnabled | `dev-tray` |
| SystemTray.IsSupported | `dev-tray` |
| SystemTray.SetEntryChecked | `dev-tray` |
| SystemTray.SetEntryEnabled | `dev-tray` |
| SystemTray.SetEntryLabel | `dev-tray` |
| SystemTray.Tooltip | `dev-tray` |
| TaskItem | `wk-tasks` |
| TaskItem.GetMetadata | `wk-tasks` |
| TaskItem.HasMetadata | `wk-tasks` |
| TaskItem.ItemSpec | `wk-tasks` |
| TaskItem.MetadataNames | `wk-tasks` |
| TaskItem.SetMetadata | `wk-tasks` |
| Tasks::ContentProject | `wk-contentproject` |
| Tasks::ContentTask | `wk-tasks` |
| Tasks::ContentTask.ErrorsEXT | `wk-tasks` |
| Tasks::ContentTask.Execute | `wk-tasks` |
| Tasks::ContentTask.LogError | `wk-tasks` |
| Tasks::ContentTask.LogMessage | `wk-tasks` |
| Tasks::ContentTask.LogWarning | `wk-tasks` |
| Tasks::ContentTask.MessagesEXT | `wk-tasks` |
| Tasks::ContentTask.SetLoggerEXT | `wk-tasks` |
| Tasks::ContentTask.WarningsEXT | `wk-tasks` |
| TextDropEventArgsEXT | `dev-window` |
| TextDropEventArgsEXT.Text | `dev-window` |
| TextInputEXT.StartTextInputWithTypeEXT | `in-text` |
| TextInputEXT.TextInput | `in-text` |
| TextInputTypeEXT | `in-text` |
| Texture.Format | `rt-formats` |
| Texture.LevelCount | `wk-material` |
| Texture2D | `materials`, `grounds-fountain`, `app-content`, `tex-mip`, `rf-garden` |
| Texture2D.DDSFromStreamEXT | `ct-formats` |
| Texture2D.FromStream | `tex-io`, `dev-medialib` |
| Texture2D.GetData | `sprite-atlas`, `sprite-hit`, `sprite-tilemap`, `geo-terrain`, `tex-dynamic`, `ct-formats`, `wk-material`, `rv-targets`, `rv-formats` |
| Texture2D.SaveAsJpeg | `tex-io` |
| Texture2D.SaveAsPng | `tex-io` |
| Texture2D.SetData | `sprite-ui`, `sprite-atlas`, `sprite-anim`, `tex-dynamic`, `rv-formats` |
| Texture2DContent | `ct-processor`, `ct-compiler` |
| Texture3D | `tex-volume`, `tex-volshader`, `rv-capabilities` |
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
| TextureProcessorOutputFormat | `ct-processor`, `wk-material` |
| TextureReferenceDictionary | `wk-material` |
| TextureTransformEXT | `model-material`, `wk-gltf` |
| TitleContainer.OpenStream | `ct-storage`, `pl-exceptions` |
| TitleLocation.Path | `wk-cnbloader` |
| ToCnbSoundEffect | `wk-routes` |
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
| Vector2 | `sprite-origin`, `wk-xml` |
| Vector2.Add | `math-checks` |
| Vector2.Barycentric | `math-checks` |
| Vector2.CatmullRom | `math-checks` |
| Vector2.Clamp | `math-checks` |
| Vector2.Distance | `math-checks` |
| Vector2.DistanceSquared | `math-checks` |
| Vector2.Divide | `math-checks` |
| Vector2.Dot | `math-checks` |
| Vector2.Hermite | `math-checks` |
| Vector2.Lerp | `math-checks` |
| Vector2.Max | `math-checks` |
| Vector2.Min | `math-checks` |
| Vector2.Multiply | `math-checks` |
| Vector2.Negate | `math-checks` |
| Vector2.Normalize | `math-checks` |
| Vector2.Reflect | `math-checks` |
| Vector2.SmoothStep | `math-checks` |
| Vector2.Subtract | `math-checks` |
| Vector2.Transform | `math-checks` |
| Vector2.TransformNormal | `math-checks` |
| Vector2Converter | `vt-converters` |
| Vector3 | `an-pendulum`, `sim-flock`, `sim-integrators`, `wk-xml`, `rf-deck` |
| Vector3.Add | `vec-add`, `math-checks` |
| Vector3.Barycentric | `curve-barycentric`, `math-checks` |
| Vector3.CatmullRom | `vec-lerp`, `curve-spline`, `math-checks` |
| Vector3.Clamp | `math-checks` |
| Vector3.Cross | `vec-cross`, `math-checks`, `sim-ik` |
| Vector3.Distance | `vec-normalize`, `math-checks`, `an-doors` |
| Vector3.DistanceSquared | `math-checks` |
| Vector3.Divide | `math-checks` |
| Vector3.Dot | `vec-dot`, `vec-project`, `math-checks`, `sim-balls`, `sim-ik` |
| Vector3.Hermite | `math-checks` |
| Vector3.Length | `vec-cross`, `vec-normalize` |
| Vector3.LengthSquared | `sim-flock` |
| Vector3.Lerp | `vec-lerp`, `math-checks`, `cam-chase` |
| Vector3.Max | `math-checks` |
| Vector3.Min | `math-checks` |
| Vector3.Multiply | `vec-project`, `math-checks` |
| Vector3.Negate | `math-checks` |
| Vector3.Normalize | `vec-reflect`, `vec-normalize`, `math-checks`, `sim-flock` |
| Vector3.Reflect | `vec-reflect`, `math-checks` |
| Vector3.SmoothStep | `vec-lerp`, `math-checks`, `cam-transition` |
| Vector3.Subtract | `vec-add`, `math-checks` |
| Vector3.Transform | `math-checks` |
| Vector3.TransformNormal | `mat-normals`, `math-checks` |
| Vector3Converter | `vt-converters` |
| Vector3Converter.ConvertFrom | `vt-converters` |
| Vector3Converter.ConvertTo | `vt-converters` |
| Vector4 | `wk-xml` |
| Vector4.Add | `math-checks` |
| Vector4.Barycentric | `math-checks` |
| Vector4.CatmullRom | `math-checks` |
| Vector4.Clamp | `math-checks` |
| Vector4.Distance | `math-checks` |
| Vector4.DistanceSquared | `math-checks` |
| Vector4.Divide | `math-checks` |
| Vector4.Dot | `math-checks` |
| Vector4.Hermite | `math-checks` |
| Vector4.Lerp | `math-checks` |
| Vector4.Max | `math-checks` |
| Vector4.Min | `math-checks` |
| Vector4.Multiply | `math-checks` |
| Vector4.Negate | `math-checks` |
| Vector4.Normalize | `math-checks` |
| Vector4.SmoothStep | `math-checks` |
| Vector4.Subtract | `math-checks` |
| Vector4.Transform | `mat-screen`, `math-checks` |
| Vector4Converter | `vt-converters` |
| VertexBuffer | `geo-vertextypes`, `geo-indexed`, `geo-procedural`, `geo-manybuffers`, `geo-terrain`, `model-skinfx`, `pf-buffers` |
| VertexBuffer(GraphicsDevice, Type, int, BufferUsage) | `rv-vertextype` |
| VertexBuffer.GetData | `wk-gltf` |
| VertexBuffer.VertexCount | `wk-gltf` |
| VertexBuffer.VertexDeclaration | `wk-gltf` |
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
| VertexDeclaration.GetVertexElements | `geo-vertextypes`, `wk-gltf`, `rv-vertextype` |
| VertexDeclaration.VertexStride | `geo-vertextypes`, `wk-gltf`, `rv-vertextype` |
| VertexDeclarationContent | `ct-meshbuilder` |
| VertexElement | `geo-vertextypes`, `rv-vertextype` |
| VertexElement.Offset | `wk-gltf` |
| VertexElement.UsageIndex | `wk-gltf` |
| VertexElement.VertexElementUsage | `wk-gltf` |
| VertexElementFormat | `geo-vertextypes` |
| VertexElementUsage | `geo-vertextypes`, `wk-gltf` |
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
| Video.Width | `wk-routes` |
| VideoContent | `au-video` |
| VideoContentWriter | `wk-routes` |
| VideoFramesPerSecondParameter | `wk-routes` |
| VideoHeightParameter | `wk-routes` |
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
| VideoWidthParameter | `wk-routes` |
| Viewport | `app-viewport`, `cam-split` |
| Viewport.MaxDepth | `app-viewport` |
| Viewport.MinDepth | `app-viewport` |
| Viewport.Project | `app-viewport`, `mat-screen`, `text-labels` |
| Viewport.Unproject | `mat-screen` |
| VisualizationData | `au-music` |
| WaveBank | `au-xact` |
| WaveBank.IsInUse | `au-xact` |
| WmaImporter | `wk-importers` |
| WmaImporter.Attribute | `wk-importers` |
| WmaImporter.Import | `wk-importers` |
| WmvImporter | `au-video` |
| WmvImporter.Import | `au-video` |
| WriteLeaderboardsEventArgs.Gamer | `gs-leaderboards` |
| WriteLeaderboardsEventArgs.IsLeaving | `gs-leaderboards` |
| XImporter | `ct-modelimport` |
| XImporter.Import | `wk-animation` |
| XmaEncodeQuality | `wk-importers` |
| XmaEncodeQualityName | `wk-importers` |
| XmaEncodeRequest | `wk-importers` |
| XmaEncodeResult | `wk-importers` |
| XmaEncoderIdentity.ToString | `wk-importers` |
| XmaEncoderService | `wk-importers` |
| XmaEncoderService.Available | `wk-importers` |
| XmaEncoderService.Encode | `wk-importers` |
| XmaEncoderService.Identity | `wk-importers` |
| XmaEncoderService.UnavailableReason | `wk-importers` |
| XmaEncoderUnavailableSentence | `wk-importers` |
| XmlImporter | `ct-intermediate` |
| XmlImporter.Attribute | `ct-intermediate` |
| XmlImporter.Import | `ct-intermediate` |
| XnaBridgeProcessorContext | `wk-material`, `wk-processor` |
| XnaComponentMapping | `wk-tasks` |
| XnbImporter | `wk-routes` |
| XnbVideoProcessor | `wk-routes` |
| XnbVideoProcessor.InputType | `wk-routes` |
| cna_accelerometer_create | `cx-capi` |
| cna_accelerometer_destroy | `cx-capi` |
| cna_accelerometer_get_current_value | `cx-capi` |
| cna_accelerometer_get_is_data_valid | `cx-capi` |
| cna_accelerometer_get_is_supported | `cx-capi` |
| cna_accelerometer_inject_synthetic_update_ext | `cx-capi` |
| cna_accelerometer_reading_copy_string | `cx-capi` |
| cna_accelerometer_reading_copy_type_name | `cx-capi` |
| cna_accelerometer_reading_equals | `cx-capi` |
| cna_accelerometer_reading_get_hash_code | `cx-capi` |
| cna_accelerometer_reading_get_string_size | `cx-capi` |
| cna_accelerometer_reading_get_type_name_size | `cx-capi` |
| cna_accelerometer_reading_init | `cx-capi` |
| cna_accelerometer_reading_init_from_values | `cx-capi` |
| cna_accelerometer_set_started_for_tests_ext | `cx-capi` |
| cna_accelerometer_set_supported_for_tests_ext | `cx-capi` |
| cna_accelerometer_start | `cx-capi` |
| cna_achievement_collection_add | `cx-capi` |
| cna_achievement_collection_create_ext | `cx-capi` |
| cna_achievement_collection_destroy | `cx-capi` |
| cna_achievement_collection_get_by_key | `cx-capi` |
| cna_achievement_collection_get_count | `cx-capi` |
| cna_achievement_copy_name | `cx-capi` |
| cna_achievement_create_ext | `cx-capi` |
| cna_achievement_destroy | `cx-capi` |
| cna_achievement_get_info | `cx-capi` |
| cna_album_collection_destroy | `cx-capi` |
| cna_album_collection_get_count | `cx-capi` |
| cna_alpha_test_effect_create | `cx-capi` |
| cna_alpha_test_effect_get_alpha_function | `cx-capi` |
| cna_alpha_test_effect_get_reference_alpha | `cx-capi` |
| cna_artist_collection_destroy | `cx-capi` |
| cna_artist_collection_get_count | `cx-capi` |
| cna_assembly_copy_title_ext | `cx-capi` |
| cna_assembly_set_title_ext | `cx-capi` |
| cna_audio_category_copy_name | `cx-capi` |
| cna_audio_category_destroy | `cx-capi` |
| cna_audio_emitter_init | `cx-capi` |
| cna_audio_engine_create | `cx-capi` |
| cna_audio_engine_destroy | `cx-capi` |
| cna_audio_engine_get_category | `cx-capi` |
| cna_audio_engine_get_global_variable | `cx-capi` |
| cna_audio_engine_set_global_variable | `cx-capi` |
| cna_audio_get_capabilities | `cx-capi` |
| cna_audio_listener_init | `cx-capi` |
| cna_available_network_session_copy_host_gamertag | `cx-capi` |
| cna_available_network_session_create_ext | `cx-capi` |
| cna_available_network_session_destroy | `cx-capi` |
| cna_available_network_session_get_open_public_gamer_slots | `cx-capi` |
| cna_avatar_description_create_random | `cx-capi` |
| cna_avatar_description_destroy | `cx-capi` |
| cna_avatar_description_get_info | `cx-capi` |
| cna_basic_effect_create | `cx-capi` |
| cna_basic_effect_get_alpha | `cx-capi` |
| cna_basic_effect_get_diffuse_color | `cx-capi` |
| cna_basic_effect_get_emissive_color | `cx-capi` |
| cna_basic_effect_get_specular_color | `cx-capi` |
| cna_basic_effect_get_specular_power | `cx-capi` |
| cna_basic_effect_set_diffuse_color | `cx-capi` |
| cna_basic_effect_set_vertex_color_enabled | `cx-capi` |
| cna_blend_state_init | `cx-capi` |
| cna_bounding_box_contains_box | `cx-capi` |
| cna_bounding_box_contains_point | `cx-capi` |
| cna_bounding_box_copy_corners | `cx-capi` |
| cna_bounding_box_copy_string | `cx-capi` |
| cna_bounding_box_create_from_points | `cx-capi` |
| cna_bounding_box_create_merged | `cx-capi` |
| cna_bounding_box_get_string_size | `cx-capi` |
| cna_bounding_box_init_min_max | `cx-capi` |
| cna_bounding_frustum_contains_box | `cx-capi` |
| cna_bounding_frustum_contains_point | `cx-capi` |
| cna_bounding_frustum_copy_corners | `cx-capi` |
| cna_bounding_frustum_get_far | `cx-capi` |
| cna_bounding_frustum_get_near | `cx-capi` |
| cna_bounding_frustum_init_matrix | `cx-capi` |
| cna_bounding_sphere_contains_sphere | `cx-capi` |
| cna_bounding_sphere_create_from_box | `cx-capi` |
| cna_bounding_sphere_init_center_radius | `cx-capi` |
| cna_bounding_sphere_transform | `cx-capi` |
| cna_camera_create_with_test_backend_ext | `cx-capi` |
| cna_camera_destroy | `cx-capi` |
| cna_camera_device_info_init | `cx-capi` |
| cna_camera_get_frame_height_ext | `cx-capi` |
| cna_camera_get_frame_width_ext | `cx-capi` |
| cna_camera_get_state_ext | `cx-capi` |
| cna_camera_set_test_frame_ext | `cx-capi` |
| cna_camera_try_acquire_frame_ext | `cx-capi` |
| cna_clipboard_copy_text | `cx-capi` |
| cna_clipboard_get_has_text | `cx-capi` |
| cna_clipboard_get_text_size | `cx-capi` |
| cna_clipboard_set_text | `cx-capi` |
| cna_cnb_asset_type_id_from_name | `cx-capi` |
| cna_cnb_byte_writer_copy_bytes | `cx-capi` |
| cna_cnb_byte_writer_create | `cx-capi` |
| cna_cnb_byte_writer_destroy | `cx-capi` |
| cna_cnb_byte_writer_get_size | `cx-capi` |
| cna_cnb_byte_writer_write_string | `cx-capi` |
| cna_cnb_byte_writer_write_u32 | `cx-capi` |
| cna_cnb_checked_add | `cx-capi` |
| cna_cnb_checked_multiply | `cx-capi` |
| cna_cnb_copy_chunk_id_string | `cx-capi` |
| cna_cnb_copy_compressed | `cx-capi` |
| cna_cnb_copy_compression_name | `cx-capi` |
| cna_cnb_copy_decompressed | `cx-capi` |
| cna_cnb_copy_format_magic | `cx-capi` |
| cna_cnb_crc32c | `cx-capi` |
| cna_cnb_crc32c_continue | `cx-capi` |
| cna_cnb_crc32c_portable | `cx-capi` |
| cna_cnb_crc32c_uses_hardware | `cx-capi` |
| cna_cnb_decode_curve | `cx-capi` |
| cna_cnb_document_copy_chunk_data | `cx-capi` |
| cna_cnb_document_copy_metadata_content_name | `cx-capi` |
| cna_cnb_document_destroy | `cx-capi` |
| cna_cnb_document_find_single | `cx-capi` |
| cna_cnb_document_get_asset_type_id | `cx-capi` |
| cna_cnb_document_get_chunk | `cx-capi` |
| cna_cnb_document_get_chunk_count | `cx-capi` |
| cna_cnb_document_get_container_major | `cx-capi` |
| cna_cnb_document_parse | `cx-capi` |
| cna_cnb_encode_curve | `cx-capi` |
| cna_cnb_get_compressed_byte_count | `cx-capi` |
| cna_cnb_get_texture_level_byte_size | `cx-capi` |
| cna_cnb_has_magic | `cx-capi` |
| cna_cnb_is_block_compressed_texture_format | `cx-capi` |
| cna_cnb_is_compression_supported | `cx-capi` |
| cna_cnb_is_custom_asset_type_id | `cx-capi` |
| cna_cnb_is_well_formed_chunk_id | `cx-capi` |
| cna_cnb_make_chunk_id | `cx-capi` |
| cna_cnb_reader_copy_string | `cx-capi` |
| cna_cnb_reader_create | `cx-capi` |
| cna_cnb_reader_destroy | `cx-capi` |
| cna_cnb_reader_read_string | `cx-capi` |
| cna_cnb_reader_read_u32 | `cx-capi` |
| cna_cnb_reader_require_exhausted | `cx-capi` |
| cna_cnb_writer_add_chunk | `cx-capi` |
| cna_cnb_writer_build | `cx-capi` |
| cna_cnb_writer_create | `cx-capi` |
| cna_cnb_writer_destroy | `cx-capi` |
| cna_cnb_writer_set_metadata | `cx-capi` |
| cna_color_copy_string | `cx-capi` |
| cna_color_equals | `cx-capi` |
| cna_color_from_non_premultiplied_int | `cx-capi` |
| cna_color_get_hash_code | `cx-capi` |
| cna_color_get_packed_value | `cx-capi` |
| cna_color_get_string_byte_count | `cx-capi` |
| cna_color_init_float_rgba | `cx-capi` |
| cna_color_init_int_rgb | `cx-capi` |
| cna_color_init_vector4 | `cx-capi` |
| cna_color_lerp | `cx-capi` |
| cna_color_multiply | `cx-capi` |
| cna_color_not_equals | `cx-capi` |
| cna_color_set_packed_value | `cx-capi` |
| cna_color_to_vector4 | `cx-capi` |
| cna_compass_create | `cx-capi` |
| cna_compass_destroy | `cx-capi` |
| cna_compass_get_current_value | `cx-capi` |
| cna_compass_inject_synthetic_update_ext | `cx-capi` |
| cna_compass_reading_copy_type_name | `cx-capi` |
| cna_compass_reading_equals | `cx-capi` |
| cna_compass_reading_init_from_values | `cx-capi` |
| cna_compass_set_test_backend_ext | `cx-capi` |
| cna_compass_start | `cx-capi` |
| cna_compass_stop | `cx-capi` |
| cna_content_manager_copy_normalized_key | `cx-capi` |
| cna_content_manager_copy_root_directory | `cx-capi` |
| cna_content_manager_create | `cx-capi` |
| cna_content_manager_destroy | `cx-capi` |
| cna_content_manager_get_has_service_provider | `cx-capi` |
| cna_content_manager_get_manifest_entry_count | `cx-capi` |
| cna_content_manager_load_model | `cx-capi` |
| cna_content_manager_load_sound_effect | `cx-capi` |
| cna_content_manager_load_texture2d | `cx-capi` |
| cna_content_reader_check_collection_element_count | `cx-capi` |
| cna_content_reader_copy_asset_name | `cx-capi` |
| cna_content_reader_create | `cx-capi` |
| cna_content_reader_destroy | `cx-capi` |
| cna_content_reader_get_content_manager | `cx-capi` |
| cna_content_reader_get_platform | `cx-capi` |
| cna_content_reader_get_version | `cx-capi` |
| cna_content_reader_initialize_type_readers | `cx-capi` |
| cna_content_reader_read_color | `cx-capi` |
| cna_content_reader_read_object_tag | `cx-capi` |
| cna_content_reader_read_shared_resources | `cx-capi` |
| cna_content_reader_read_vector2 | `cx-capi` |
| cna_content_reader_read_vector3 | `cx-capi` |
| cna_content_type_reader_copy_target_type_name | `cx-capi` |
| cna_content_type_reader_destroy | `cx-capi` |
| cna_content_type_reader_get_can_deserialize_into_existing_object | `cx-capi` |
| cna_content_type_reader_get_target_type_name_size | `cx-capi` |
| cna_content_type_reader_get_type_version | `cx-capi` |
| cna_content_type_reader_manager_create_reader | `cx-capi` |
| cna_content_type_reader_manager_get_is_registered | `cx-capi` |
| cna_content_type_reader_manager_register | `cx-capi` |
| cna_content_type_reader_manager_unregister | `cx-capi` |
| cna_content_type_reader_supports_version | `cx-capi` |
| cna_crt_effect_create | `cx-capi` |
| cna_crt_effect_get_scanline_intensity | `cx-capi` |
| cna_crt_effect_set_scanline_intensity | `cx-capi` |
| cna_cue_copy_name | `cx-capi` |
| cna_cue_destroy | `cx-capi` |
| cna_cue_get_info | `cx-capi` |
| cna_cue_get_variable | `cx-capi` |
| cna_cue_play | `cx-capi` |
| cna_cue_set_variable | `cx-capi` |
| cna_cue_stop | `cx-capi` |
| cna_curve_create | `cx-capi` |
| cna_curve_destroy | `cx-capi` |
| cna_curve_evaluate | `cx-capi` |
| cna_curve_get_keys | `cx-capi` |
| cna_curve_key_collection_add | `cx-capi` |
| cna_curve_key_init_position_value | `cx-capi` |
| cna_debug_draw_add_box | `cx-capi` |
| cna_debug_draw_add_line | `cx-capi` |
| cna_debug_draw_begin | `cx-capi` |
| cna_debug_draw_clear | `cx-capi` |
| cna_debug_draw_copy_vertices | `cx-capi` |
| cna_debug_draw_create | `cx-capi` |
| cna_debug_draw_destroy | `cx-capi` |
| cna_debug_draw_get_line_count | `cx-capi` |
| cna_debug_draw_set_depth_tested | `cx-capi` |
| cna_depth_stencil_state_init | `cx-capi` |
| cna_desktop_os_get_current | `cx-capi` |
| cna_devices_ext_is_available | `cx-capi` |
| cna_directional_light_destroy | `cx-capi` |
| cna_directional_light_get_diffuse_color | `cx-capi` |
| cna_directional_light_get_direction | `cx-capi` |
| cna_display_info_get_content_scale_ext | `cx-capi` |
| cna_display_mode_equals | `cx-capi` |
| cna_display_mode_init | `cx-capi` |
| cna_dual_texture_effect_create | `cx-capi` |
| cna_dual_texture_effect_get_diffuse_color | `cx-capi` |
| cna_effect_apply | `cx-capi` |
| cna_effect_clone | `cx-capi` |
| cna_effect_copy_type_name | `cx-capi` |
| cna_effect_destroy | `cx-capi` |
| cna_effect_fog_get_end | `cx-capi` |
| cna_effect_get_current_technique | `cx-capi` |
| cna_effect_get_parameters | `cx-capi` |
| cna_effect_get_techniques | `cx-capi` |
| cna_effect_lights_enable_default | `cx-capi` |
| cna_effect_lights_get_ambient_color | `cx-capi` |
| cna_effect_lights_get_directional_light | `cx-capi` |
| cna_effect_lights_get_enabled | `cx-capi` |
| cna_effect_matrices_set_projection | `cx-capi` |
| cna_effect_matrices_set_view | `cx-capi` |
| cna_effect_matrices_set_world | `cx-capi` |
| cna_effect_parameter_collection_destroy | `cx-capi` |
| cna_effect_parameter_collection_find_name | `cx-capi` |
| cna_effect_parameter_collection_get_count | `cx-capi` |
| cna_effect_parameter_destroy | `cx-capi` |
| cna_effect_parameter_get_info | `cx-capi` |
| cna_effect_technique_collection_destroy | `cx-capi` |
| cna_effect_technique_collection_get_count | `cx-capi` |
| cna_effect_technique_copy_name | `cx-capi` |
| cna_effect_technique_destroy | `cx-capi` |
| cna_enum_type_reader_copy_canonical_name | `cx-capi` |
| cna_enum_type_reader_get_canonical_name_size | `cx-capi` |
| cna_environment_get_device_type | `cx-capi` |
| cna_environment_map_effect_create | `cx-capi` |
| cna_environment_map_effect_get_amount | `cx-capi` |
| cna_environment_map_effect_get_fresnel_factor | `cx-capi` |
| cna_error_copy_last_exception_param_name_ext | `cx-capi` |
| cna_error_copy_last_exception_type_ext | `cx-capi` |
| cna_error_copy_last_message | `cx-capi` |
| cna_error_get_last_exception_param_name_size_ext | `cx-capi` |
| cna_error_get_last_exception_type_size_ext | `cx-capi` |
| cna_error_get_last_info | `cx-capi` |
| cna_error_get_last_message_size | `cx-capi` |
| cna_file_dialog_set_test_backend_ext | `cx-capi` |
| cna_file_dialog_show_open_file_ext | `cx-capi` |
| cna_framework_dispatcher_update | `cx-capi` |
| cna_game_clear | `cx-capi` |
| cna_game_component_callbacks_init | `cx-capi` |
| cna_game_component_copy_type_name | `cx-capi` |
| cna_game_component_create | `cx-capi` |
| cna_game_component_destroy | `cx-capi` |
| cna_game_component_get_enabled | `cx-capi` |
| cna_game_component_get_is_drawable | `cx-capi` |
| cna_game_component_get_update_order | `cx-capi` |
| cna_game_component_set_update_order | `cx-capi` |
| cna_game_components_add | `cx-capi` |
| cna_game_components_contains | `cx-capi` |
| cna_game_components_get_count | `cx-capi` |
| cna_game_components_index_of | `cx-capi` |
| cna_game_components_remove | `cx-capi` |
| cna_game_copy_type_name | `cx-capi` |
| cna_game_create | `cx-capi` |
| cna_game_destroy | `cx-capi` |
| cna_game_fps_to_milliseconds_per_frame_ext | `cx-capi` |
| cna_game_get_content_manager_ext | `cx-capi` |
| cna_game_get_graphics_device | `cx-capi` |
| cna_game_get_is_active | `cx-capi` |
| cna_game_get_is_fixed_time_step | `cx-capi` |
| cna_game_get_target_elapsed_time_ticks | `cx-capi` |
| cna_game_get_target_fps_ext | `cx-capi` |
| cna_game_get_target_ms_frame_time_ext | `cx-capi` |
| cna_game_launch_parameters_add | `cx-capi` |
| cna_game_launch_parameters_contains_key | `cx-capi` |
| cna_game_launch_parameters_copy_key | `cx-capi` |
| cna_game_launch_parameters_copy_value | `cx-capi` |
| cna_game_launch_parameters_get_count | `cx-capi` |
| cna_game_request_exit | `cx-capi` |
| cna_game_run | `cx-capi` |
| cna_game_run_one_frame | `cx-capi` |
| cna_game_services_contains_ext | `cx-capi` |
| cna_game_set_target_elapsed_time_ticks | `cx-capi` |
| cna_game_set_window_title | `cx-capi` |
| cna_game_started_event_info_init | `cx-capi` |
| cna_game_subscribe | `cx-capi` |
| cna_game_unsubscribe | `cx-capi` |
| cna_game_window_copy_title | `cx-capi` |
| cna_game_window_copy_type_name | `cx-capi` |
| cna_game_window_get_client_bounds | `cx-capi` |
| cna_game_window_get_title_size | `cx-capi` |
| cna_gamepad_apply_dead_zone | `cx-capi` |
| cna_gamepad_buttons_init_from_mask | `cx-capi` |
| cna_gamepad_buttons_is_pressed | `cx-capi` |
| cna_gamepad_get_capabilities | `cx-capi` |
| cna_gamepad_get_state | `cx-capi` |
| cna_gamepad_set_vibration | `cx-capi` |
| cna_gamepad_state_copy_string | `cx-capi` |
| cna_gamepad_state_get_buttons | `cx-capi` |
| cna_gamepad_state_init_from_components | `cx-capi` |
| cna_gamepad_state_is_button_down | `cx-capi` |
| cna_gamepad_state_is_button_up | `cx-capi` |
| cna_gamepad_thumb_sticks_init_from_positions | `cx-capi` |
| cna_gamepad_triggers_init_from_positions | `cx-capi` |
| cna_gamer_copy_gamertag | `cx-capi` |
| cna_gamer_get_signed_in_gamer_at_player_index | `cx-capi` |
| cna_gamer_get_signed_in_gamer_count | `cx-capi` |
| cna_gamer_presence_init | `cx-capi` |
| cna_gamer_services_dispatcher_get_is_initialized | `cx-capi` |
| cna_gamer_services_dispatcher_initialize | `cx-capi` |
| cna_gamer_services_dispatcher_update | `cx-capi` |
| cna_gamer_set_signed_in_gamers_ext | `cx-capi` |
| cna_genre_collection_destroy | `cx-capi` |
| cna_genre_collection_get_count | `cx-capi` |
| cna_get_abi_version | `cx-capi` |
| cna_graphics_adapter_copy_description | `cx-capi` |
| cna_graphics_adapter_get_count | `cx-capi` |
| cna_graphics_adapter_get_current_display_mode | `cx-capi` |
| cna_graphics_adapter_get_info | `cx-capi` |
| cna_graphics_adapter_is_profile_supported | `cx-capi` |
| cna_graphics_device_clear_options | `cx-capi` |
| cna_graphics_device_copy_capability_report_ext | `cx-capi` |
| cna_graphics_device_copy_render_targets | `cx-capi` |
| cna_graphics_device_copy_renderer_name | `cx-capi` |
| cna_graphics_device_copy_type_name | `cx-capi` |
| cna_graphics_device_draw_indexed_primitives | `cx-capi` |
| cna_graphics_device_draw_user_primitives | `cx-capi` |
| cna_graphics_device_get_backbuffer_info | `cx-capi` |
| cna_graphics_device_get_blend_factor | `cx-capi` |
| cna_graphics_device_get_blend_state | `cx-capi` |
| cna_graphics_device_get_capability_report_size_ext | `cx-capi` |
| cna_graphics_device_get_depth_stencil_state | `cx-capi` |
| cna_graphics_device_get_graphics_profile | `cx-capi` |
| cna_graphics_device_get_index_buffer | `cx-capi` |
| cna_graphics_device_get_multi_sample_mask | `cx-capi` |
| cna_graphics_device_get_presentation_parameters | `cx-capi` |
| cna_graphics_device_get_rasterizer_state | `cx-capi` |
| cna_graphics_device_get_reference_stencil | `cx-capi` |
| cna_graphics_device_get_render_target_count | `cx-capi` |
| cna_graphics_device_get_renderer_info | `cx-capi` |
| cna_graphics_device_get_renderer_name_size | `cx-capi` |
| cna_graphics_device_get_sampler_state | `cx-capi` |
| cna_graphics_device_get_scissor_rectangle | `cx-capi` |
| cna_graphics_device_get_shader_dialect_ext | `cx-capi` |
| cna_graphics_device_get_viewport | `cx-capi` |
| cna_graphics_device_information_clone | `cx-capi` |
| cna_graphics_device_information_copy_type_name | `cx-capi` |
| cna_graphics_device_information_init | `cx-capi` |
| cna_graphics_device_manager_create | `cx-capi` |
| cna_graphics_device_manager_destroy | `cx-capi` |
| cna_graphics_device_manager_get_graphics_profile | `cx-capi` |
| cna_graphics_device_manager_get_preferred_back_buffer_width | `cx-capi` |
| cna_graphics_device_manager_set_preferred_back_buffer_width | `cx-capi` |
| cna_graphics_device_set_blend_factor | `cx-capi` |
| cna_graphics_device_set_blend_state | `cx-capi` |
| cna_graphics_device_set_depth_stencil_state | `cx-capi` |
| cna_graphics_device_set_index_buffer | `cx-capi` |
| cna_graphics_device_set_multi_sample_mask | `cx-capi` |
| cna_graphics_device_set_rasterizer_state | `cx-capi` |
| cna_graphics_device_set_reference_stencil | `cx-capi` |
| cna_graphics_device_set_render_target2d | `cx-capi` |
| cna_graphics_device_set_sampler_state | `cx-capi` |
| cna_graphics_device_set_scissor_rectangle | `cx-capi` |
| cna_graphics_device_set_vertex_buffer | `cx-capi` |
| cna_graphics_device_set_viewport | `cx-capi` |
| cna_graphics_device_supports_capability | `cx-capi` |
| cna_graphics_ext_is_available | `cx-capi` |
| cna_graphics_renderer_category_copy_name | `cx-capi` |
| cna_graphics_renderer_category_get_name_size | `cx-capi` |
| cna_graphics_renderer_copy_available_ext | `cx-capi` |
| cna_graphics_renderer_copy_current_name | `cx-capi` |
| cna_graphics_renderer_get_active_ext | `cx-capi` |
| cna_graphics_renderer_get_automatic_fallback_ext | `cx-capi` |
| cna_graphics_renderer_get_available_count_ext | `cx-capi` |
| cna_graphics_renderer_get_category | `cx-capi` |
| cna_graphics_renderer_get_current_name_size | `cx-capi` |
| cna_graphics_renderer_get_current_type | `cx-capi` |
| cna_graphics_renderer_get_fallback_count_ext | `cx-capi` |
| cna_graphics_renderer_get_is_available_ext | `cx-capi` |
| cna_graphics_renderer_get_is_latched_ext | `cx-capi` |
| cna_graphics_renderer_get_maturity | `cx-capi` |
| cna_graphics_renderer_get_selected_ext | `cx-capi` |
| cna_graphics_renderer_maturity_copy_name | `cx-capi` |
| cna_graphics_renderer_set_preferred_ext | `cx-capi` |
| cna_graphics_renderer_try_parse_name_ext | `cx-capi` |
| cna_graphics_resource_copy_name | `cx-capi` |
| cna_graphics_resource_copy_string | `cx-capi` |
| cna_graphics_resource_dispose | `cx-capi` |
| cna_graphics_resource_get_graphics_device | `cx-capi` |
| cna_graphics_resource_get_is_disposed | `cx-capi` |
| cna_graphics_resource_get_tag | `cx-capi` |
| cna_graphics_resource_set_name | `cx-capi` |
| cna_graphics_resource_set_tag | `cx-capi` |
| cna_graphics_resource_subscribe_disposing | `cx-capi` |
| cna_graphics_resource_unsubscribe_disposing | `cx-capi` |
| cna_guide_begin_show_keyboard_input | `cx-capi` |
| cna_guide_get_has_pending_keyboard_input_ext | `cx-capi` |
| cna_guide_get_is_trial_mode | `cx-capi` |
| cna_guide_get_is_visible | `cx-capi` |
| cna_guide_get_simulate_trial_mode | `cx-capi` |
| cna_guide_reset_pending_keyboard_input_ext | `cx-capi` |
| cna_guide_set_simulate_trial_mode | `cx-capi` |
| cna_guide_simulate_keyboard_input_cancel_ext | `cx-capi` |
| cna_guide_was_keyboard_input_canceled_ext | `cx-capi` |
| cna_half_from_single | `cx-capi` |
| cna_haptic_device_destroy | `cx-capi` |
| cna_haptic_device_get_is_open | `cx-capi` |
| cna_haptic_device_play_rumble | `cx-capi` |
| cna_haptic_direction_init | `cx-capi` |
| cna_haptic_effect_equals | `cx-capi` |
| cna_haptic_effect_init | `cx-capi` |
| cna_haptics_get_count | `cx-capi` |
| cna_haptics_open_from_mouse | `cx-capi` |
| cna_image_based_light_ext_init | `cx-capi` |
| cna_image_based_light_ext_is_valid | `cx-capi` |
| cna_index_buffer_create | `cx-capi` |
| cna_index_buffer_destroy | `cx-capi` |
| cna_index_buffer_get_data | `cx-capi` |
| cna_index_buffer_set_data | `cx-capi` |
| cna_input_device_info_equals | `cx-capi` |
| cna_input_device_info_init | `cx-capi` |
| cna_input_devices_get_keyboard_count | `cx-capi` |
| cna_input_devices_get_mouse_count | `cx-capi` |
| cna_input_devices_raise_mouse_connected_ext | `cx-capi` |
| cna_input_devices_subscribe_mouse_connected_ext | `cx-capi` |
| cna_input_devices_unsubscribe_ext | `cx-capi` |
| cna_joystick_info_equals | `cx-capi` |
| cna_joystick_info_init | `cx-capi` |
| cna_joysticks_get_count | `cx-capi` |
| cna_joysticks_raise_connected_ext | `cx-capi` |
| cna_joysticks_subscribe_connected_ext | `cx-capi` |
| cna_joysticks_unsubscribe_ext | `cx-capi` |
| cna_keyboard_copy_key_name_ext | `cx-capi` |
| cna_keyboard_get_key_from_name_ext | `cx-capi` |
| cna_keyboard_get_state | `cx-capi` |
| cna_keyboard_state_copy_pressed_keys | `cx-capi` |
| cna_keyboard_state_copy_string | `cx-capi` |
| cna_keyboard_state_equals | `cx-capi` |
| cna_keyboard_state_get_hash_code | `cx-capi` |
| cna_keyboard_state_get_key_state | `cx-capi` |
| cna_keyboard_state_get_pressed_key_count | `cx-capi` |
| cna_keyboard_state_init_from_keys | `cx-capi` |
| cna_keyboard_state_is_key_down | `cx-capi` |
| cna_keyboard_state_is_key_up | `cx-capi` |
| cna_keyboard_state_not_equals | `cx-capi` |
| cna_known_unsupported_content_type_reader_create | `cx-capi` |
| cna_leaderboard_identity_init | `cx-capi` |
| cna_locale_get_preferred_count_ext | `cx-capi` |
| cna_logger_get_minimum_level | `cx-capi` |
| cna_logger_info | `cx-capi` |
| cna_logger_log | `cx-capi` |
| cna_logger_reset_sink_ext | `cx-capi` |
| cna_logger_set_minimum_level | `cx-capi` |
| cna_logger_set_sink_ext | `cx-capi` |
| cna_logger_warn | `cx-capi` |
| cna_logger_warn_if | `cx-capi` |
| cna_math_barycentric | `cx-capi` |
| cna_math_catmull_rom | `cx-capi` |
| cna_math_clamp_float | `cx-capi` |
| cna_math_clamp_int32 | `cx-capi` |
| cna_math_closest_msaa_power | `cx-capi` |
| cna_math_distance | `cx-capi` |
| cna_math_hermite | `cx-capi` |
| cna_math_lerp | `cx-capi` |
| cna_math_max | `cx-capi` |
| cna_math_min | `cx-capi` |
| cna_math_smooth_step | `cx-capi` |
| cna_math_to_degrees | `cx-capi` |
| cna_math_to_radians | `cx-capi` |
| cna_math_wrap_angle | `cx-capi` |
| cna_matrix_create_look_at | `cx-capi` |
| cna_matrix_create_perspective_field_of_view | `cx-capi` |
| cna_matrix_create_rotation_y | `cx-capi` |
| cna_matrix_create_scale_scalar | `cx-capi` |
| cna_matrix_create_translation_xyz | `cx-capi` |
| cna_matrix_invert | `cx-capi` |
| cna_matrix_multiply | `cx-capi` |
| cna_media_library_copy_type_name | `cx-capi` |
| cna_media_library_create | `cx-capi` |
| cna_media_library_destroy | `cx-capi` |
| cna_media_library_dispose | `cx-capi` |
| cna_media_library_get_albums | `cx-capi` |
| cna_media_library_get_artists | `cx-capi` |
| cna_media_library_get_genres | `cx-capi` |
| cna_media_library_get_is_disposed | `cx-capi` |
| cna_media_library_get_pictures | `cx-capi` |
| cna_media_library_get_playlists | `cx-capi` |
| cna_media_library_get_songs | `cx-capi` |
| cna_media_player_get_game_has_control | `cx-capi` |
| cna_media_player_get_is_repeating | `cx-capi` |
| cna_media_player_get_queue | `cx-capi` |
| cna_media_player_get_state | `cx-capi` |
| cna_media_player_get_volume | `cx-capi` |
| cna_media_player_play_song | `cx-capi` |
| cna_media_player_set_is_repeating | `cx-capi` |
| cna_media_player_set_volume | `cx-capi` |
| cna_media_player_stop | `cx-capi` |
| cna_media_queue_destroy | `cx-capi` |
| cna_media_queue_get_count | `cx-capi` |
| cna_message_box_get_test_log_ext | `cx-capi` |
| cna_message_box_set_test_backend_ext | `cx-capi` |
| cna_message_box_show_ext | `cx-capi` |
| cna_model_bone_add_child | `cx-capi` |
| cna_model_bone_collection_destroy | `cx-capi` |
| cna_model_bone_collection_find | `cx-capi` |
| cna_model_bone_collection_get_count | `cx-capi` |
| cna_model_bone_copy_name | `cx-capi` |
| cna_model_bone_create | `cx-capi` |
| cna_model_bone_destroy | `cx-capi` |
| cna_model_bone_get_index | `cx-capi` |
| cna_model_bone_get_parent | `cx-capi` |
| cna_model_bone_set_transform | `cx-capi` |
| cna_model_copy_absolute_bone_transforms | `cx-capi` |
| cna_model_create | `cx-capi` |
| cna_model_destroy | `cx-capi` |
| cna_model_draw | `cx-capi` |
| cna_model_get_bones | `cx-capi` |
| cna_model_get_meshes | `cx-capi` |
| cna_model_get_root | `cx-capi` |
| cna_model_mesh_collection_destroy | `cx-capi` |
| cna_model_mesh_collection_get_at | `cx-capi` |
| cna_model_mesh_collection_get_count | `cx-capi` |
| cna_model_mesh_destroy | `cx-capi` |
| cna_model_mesh_get_bounding_sphere | `cx-capi` |
| cna_mouse_cursor_create_from_texture2d | `cx-capi` |
| cna_mouse_cursor_destroy | `cx-capi` |
| cna_mouse_cursor_dispose | `cx-capi` |
| cna_mouse_cursor_get_stock_ext | `cx-capi` |
| cna_mouse_get_state | `cx-capi` |
| cna_mouse_raise_clicked_ext | `cx-capi` |
| cna_mouse_set_cursor_ext | `cx-capi` |
| cna_mouse_state_copy_string | `cx-capi` |
| cna_mouse_state_equals | `cx-capi` |
| cna_mouse_state_get_hash_code | `cx-capi` |
| cna_mouse_state_get_string_size | `cx-capi` |
| cna_mouse_state_init_from_values | `cx-capi` |
| cna_mouse_state_not_equals | `cx-capi` |
| cna_mouse_subscribe_clicked_ext | `cx-capi` |
| cna_mouse_unsubscribe_clicked_ext | `cx-capi` |
| cna_network_gamer_create | `cx-capi` |
| cna_network_gamer_destroy | `cx-capi` |
| cna_network_gamer_get_is_host | `cx-capi` |
| cna_network_gamer_get_is_ready | `cx-capi` |
| cna_network_gamer_set_is_ready | `cx-capi` |
| cna_network_session_create | `cx-capi` |
| cna_network_session_create_with_local_gamers | `cx-capi` |
| cna_network_session_destroy | `cx-capi` |
| cna_network_session_get_gamer_count | `cx-capi` |
| cna_network_session_get_is_host | `cx-capi` |
| cna_network_session_get_max_gamers | `cx-capi` |
| cna_network_session_get_session_state | `cx-capi` |
| cna_network_session_properties_contains | `cx-capi` |
| cna_network_session_properties_create | `cx-capi` |
| cna_network_session_properties_destroy | `cx-capi` |
| cna_network_session_properties_get_count | `cx-capi` |
| cna_network_session_properties_get_is_read_only | `cx-capi` |
| cna_network_session_properties_get_item | `cx-capi` |
| cna_network_session_properties_set_item | `cx-capi` |
| cna_network_session_start_game | `cx-capi` |
| cna_network_session_update | `cx-capi` |
| cna_packet_reader_create | `cx-capi` |
| cna_packet_reader_destroy | `cx-capi` |
| cna_packet_reader_get_position | `cx-capi` |
| cna_packet_reader_read_color | `cx-capi` |
| cna_packet_reader_read_double | `cx-capi` |
| cna_packet_reader_read_single | `cx-capi` |
| cna_packet_reader_read_vector3 | `cx-capi` |
| cna_packet_reader_set_data_ext | `cx-capi` |
| cna_packet_writer_copy_data_ext | `cx-capi` |
| cna_packet_writer_create | `cx-capi` |
| cna_packet_writer_destroy | `cx-capi` |
| cna_packet_writer_get_length | `cx-capi` |
| cna_packet_writer_write_color | `cx-capi` |
| cna_packet_writer_write_double | `cx-capi` |
| cna_packet_writer_write_single | `cx-capi` |
| cna_packet_writer_write_vector3 | `cx-capi` |
| cna_picture_collection_destroy | `cx-capi` |
| cna_picture_collection_get_count | `cx-capi` |
| cna_plane_copy_string | `cx-capi` |
| cna_plane_dot_coordinate | `cx-capi` |
| cna_plane_equals | `cx-capi` |
| cna_plane_get_hash_code | `cx-capi` |
| cna_plane_init_abcd | `cx-capi` |
| cna_plane_init_normal_d | `cx-capi` |
| cna_plane_init_points | `cx-capi` |
| cna_plane_intersects_box | `cx-capi` |
| cna_plane_intersects_sphere | `cx-capi` |
| cna_plane_normalize | `cx-capi` |
| cna_plane_not_equals | `cx-capi` |
| cna_platform_copy_current_name_ext | `cx-capi` |
| cna_platform_get_current | `cx-capi` |
| cna_platform_get_current_name_size_ext | `cx-capi` |
| cna_platform_get_is_apple_ext | `cx-capi` |
| cna_platform_get_is_mobile_ext | `cx-capi` |
| cna_playlist_collection_destroy | `cx-capi` |
| cna_playlist_collection_get_count | `cx-capi` |
| cna_point_add | `cx-capi` |
| cna_point_copy_string | `cx-capi` |
| cna_point_get_hash_code | `cx-capi` |
| cna_point_init_xy | `cx-capi` |
| cna_power_get_info | `cx-capi` |
| cna_presentation_parameters_clone | `cx-capi` |
| cna_presentation_parameters_get_bounds | `cx-capi` |
| cna_presentation_parameters_init | `cx-capi` |
| cna_primitive_type_get_vertex_count | `cx-capi` |
| cna_property_dictionary_contains_key | `cx-capi` |
| cna_property_dictionary_copy_string | `cx-capi` |
| cna_property_dictionary_create_ext | `cx-capi` |
| cna_property_dictionary_destroy | `cx-capi` |
| cna_property_dictionary_get_count | `cx-capi` |
| cna_property_dictionary_get_date_time_ticks | `cx-capi` |
| cna_property_dictionary_get_int32 | `cx-capi` |
| cna_property_dictionary_set_int32 | `cx-capi` |
| cna_property_dictionary_set_string | `cx-capi` |
| cna_property_dictionary_try_get_value_kind_ext | `cx-capi` |
| cna_quality_of_service_init_measured | `cx-capi` |
| cna_quaternion_create_from_yaw_pitch_roll | `cx-capi` |
| cna_quaternion_length | `cx-capi` |
| cna_rasterizer_state_init | `cx-capi` |
| cna_ray_copy_string | `cx-capi` |
| cna_ray_init_position_direction | `cx-capi` |
| cna_ray_intersects_box | `cx-capi` |
| cna_ray_intersects_plane | `cx-capi` |
| cna_ray_intersects_sphere | `cx-capi` |
| cna_rectangle_contains_xy | `cx-capi` |
| cna_rectangle_copy_string | `cx-capi` |
| cna_rectangle_get_bottom | `cx-capi` |
| cna_rectangle_get_center | `cx-capi` |
| cna_rectangle_get_hash_code | `cx-capi` |
| cna_rectangle_get_right | `cx-capi` |
| cna_rectangle_inflate | `cx-capi` |
| cna_rectangle_init_xywh | `cx-capi` |
| cna_rectangle_intersect | `cx-capi` |
| cna_rectangle_intersects | `cx-capi` |
| cna_rectangle_union | `cx-capi` |
| cna_reflective_type_reader_copy_canonical_name | `cx-capi` |
| cna_reflective_type_reader_get_canonical_name_size | `cx-capi` |
| cna_render_target2d_create | `cx-capi` |
| cna_render_target_destroy | `cx-capi` |
| cna_render_target_get_info | `cx-capi` |
| cna_render_target_usage_preserves_contents | `cx-capi` |
| cna_sampler_state_init | `cx-capi` |
| cna_sensor_info_equals | `cx-capi` |
| cna_sensor_info_init | `cx-capi` |
| cna_sensors_get_last_error_id_ext | `cx-capi` |
| cna_signed_in_gamer_create_ext | `cx-capi` |
| cna_signed_in_gamer_destroy | `cx-capi` |
| cna_skinned_effect_create | `cx-capi` |
| cna_skinned_effect_get_specular_power | `cx-capi` |
| cna_skinned_effect_get_weights_per_vertex | `cx-capi` |
| cna_skinned_effect_set_weights_per_vertex | `cx-capi` |
| cna_song_collection_destroy | `cx-capi` |
| cna_song_collection_get_count | `cx-capi` |
| cna_song_copy_name | `cx-capi` |
| cna_song_create | `cx-capi` |
| cna_song_create_with_duration | `cx-capi` |
| cna_song_destroy | `cx-capi` |
| cna_song_equals | `cx-capi` |
| cna_song_get_duration | `cx-capi` |
| cna_sound_bank_create | `cx-capi` |
| cna_sound_bank_destroy | `cx-capi` |
| cna_sound_bank_get_cue | `cx-capi` |
| cna_sound_effect_copy_type_name | `cx-capi` |
| cna_sound_effect_create_instance | `cx-capi` |
| cna_sound_effect_create_pcm16 | `cx-capi` |
| cna_sound_effect_destroy | `cx-capi` |
| cna_sound_effect_get_duration_ticks | `cx-capi` |
| cna_sound_effect_get_master_volume | `cx-capi` |
| cna_sound_effect_get_sample_duration_ticks | `cx-capi` |
| cna_sound_effect_get_sample_size_in_bytes | `cx-capi` |
| cna_sound_effect_instance_destroy | `cx-capi` |
| cna_sound_effect_instance_get_info | `cx-capi` |
| cna_sound_effect_instance_play | `cx-capi` |
| cna_sound_effect_instance_set_is_looped | `cx-capi` |
| cna_sound_effect_instance_set_pan | `cx-capi` |
| cna_sound_effect_instance_set_volume | `cx-capi` |
| cna_sound_effect_instance_stop | `cx-capi` |
| cna_sprite_batch_begin | `cx-capi` |
| cna_sprite_batch_copy_type_name | `cx-capi` |
| cna_sprite_batch_create | `cx-capi` |
| cna_sprite_batch_destroy | `cx-capi` |
| cna_sprite_batch_end | `cx-capi` |
| cna_sprite_batch_submit_many | `cx-capi` |
| cna_sprite_font_copy_characters | `cx-capi` |
| cna_sprite_font_copy_glyphs | `cx-capi` |
| cna_sprite_font_create | `cx-capi` |
| cna_sprite_font_destroy | `cx-capi` |
| cna_sprite_font_get_info | `cx-capi` |
| cna_sprite_font_measure_utf8 | `cx-capi` |
| cna_sprite_font_set_default_character | `cx-capi` |
| cna_sprite_font_set_line_spacing | `cx-capi` |
| cna_sprite_font_set_spacing | `cx-capi` |
| cna_storage_container_copy_display_name | `cx-capi` |
| cna_storage_container_copy_file_name | `cx-capi` |
| cna_storage_container_copy_type_name | `cx-capi` |
| cna_storage_container_create_directory | `cx-capi` |
| cna_storage_container_create_file | `cx-capi` |
| cna_storage_container_delete_directory | `cx-capi` |
| cna_storage_container_delete_file | `cx-capi` |
| cna_storage_container_destroy | `cx-capi` |
| cna_storage_container_directory_exists | `cx-capi` |
| cna_storage_container_file_exists | `cx-capi` |
| cna_storage_container_get_file_name_count | `cx-capi` |
| cna_storage_container_open | `cx-capi` |
| cna_storage_container_open_file_access | `cx-capi` |
| cna_storage_copy_root_ext | `cx-capi` |
| cna_storage_device_delete_container | `cx-capi` |
| cna_storage_device_destroy | `cx-capi` |
| cna_storage_device_get_free_space | `cx-capi` |
| cna_storage_device_get_is_connected | `cx-capi` |
| cna_storage_device_get_total_space | `cx-capi` |
| cna_storage_device_show_selector | `cx-capi` |
| cna_storage_stream_close | `cx-capi` |
| cna_storage_stream_flush | `cx-capi` |
| cna_storage_stream_get_can_write | `cx-capi` |
| cna_storage_stream_get_length | `cx-capi` |
| cna_storage_stream_read | `cx-capi` |
| cna_storage_stream_seek | `cx-capi` |
| cna_storage_stream_write | `cx-capi` |
| cna_system_info_get_logical_cpu_core_count_ext | `cx-capi` |
| cna_system_info_get_system_ram_megabytes_ext | `cx-capi` |
| cna_system_tray_add_entry | `cx-capi` |
| cna_system_tray_click_entry_for_tests_ext | `cx-capi` |
| cna_system_tray_create_with_test_backend_ext | `cx-capi` |
| cna_system_tray_destroy | `cx-capi` |
| cna_system_tray_get_entry_checked | `cx-capi` |
| cna_system_tray_get_entry_enabled | `cx-capi` |
| cna_system_tray_set_entry_checked | `cx-capi` |
| cna_system_tray_set_entry_enabled | `cx-capi` |
| cna_text_input_raise_text_input_ext | `cx-capi` |
| cna_text_input_subscribe_text_input_ext | `cx-capi` |
| cna_text_input_unsubscribe_ext | `cx-capi` |
| cna_texture2d_copy_encoded | `cx-capi` |
| cna_texture2d_copy_type_name | `cx-capi` |
| cna_texture2d_create | `cx-capi` |
| cna_texture2d_create_from_encoded_memory | `cx-capi` |
| cna_texture2d_create_from_rgba8 | `cx-capi` |
| cna_texture2d_destroy | `cx-capi` |
| cna_texture2d_get_data_rgba8 | `cx-capi` |
| cna_texture2d_get_encoded_byte_count | `cx-capi` |
| cna_texture2d_get_info | `cx-capi` |
| cna_texture2d_set_data_rgba8 | `cx-capi` |
| cna_texture3d_copy_type_name | `cx-capi` |
| cna_texture3d_create | `cx-capi` |
| cna_texture3d_destroy | `cx-capi` |
| cna_texture3d_get_data | `cx-capi` |
| cna_texture3d_get_info | `cx-capi` |
| cna_texture3d_set_data | `cx-capi` |
| cna_texture_get_format_size | `cx-capi` |
| cna_texturecube_copy_type_name | `cx-capi` |
| cna_texturecube_create | `cx-capi` |
| cna_texturecube_destroy | `cx-capi` |
| cna_texturecube_get_data | `cx-capi` |
| cna_texturecube_set_data | `cx-capi` |
| cna_title_container_read_ext | `cx-capi` |
| cna_title_location_copy_path | `cx-capi` |
| cna_touch_get_capabilities | `cx-capi` |
| cna_touch_get_state | `cx-capi` |
| cna_touch_location_copy_string | `cx-capi` |
| cna_touch_location_get_hash_code | `cx-capi` |
| cna_touch_location_init_from_values | `cx-capi` |
| cna_touch_location_init_with_previous | `cx-capi` |
| cna_touch_location_try_get_previous | `cx-capi` |
| cna_touch_panel_enqueue_gesture_ext | `cx-capi` |
| cna_touch_panel_get_display_width | `cx-capi` |
| cna_touch_panel_get_enabled_gestures | `cx-capi` |
| cna_touch_panel_get_is_gesture_available | `cx-capi` |
| cna_touch_panel_read_gesture | `cx-capi` |
| cna_touch_panel_set_display_width | `cx-capi` |
| cna_touch_panel_set_enabled_gestures | `cx-capi` |
| cna_touch_state_find_by_id | `cx-capi` |
| cna_touch_state_init_from_locations | `cx-capi` |
| cna_vector3_cross | `cx-capi` |
| cna_vector3_dot | `cx-capi` |
| cna_vector3_init_xyz | `cx-capi` |
| cna_vector3_length | `cx-capi` |
| cna_vector3_transform_matrix | `cx-capi` |
| cna_vertex_buffer_create | `cx-capi` |
| cna_vertex_buffer_destroy | `cx-capi` |
| cna_vertex_buffer_get_data | `cx-capi` |
| cna_vertex_buffer_get_info | `cx-capi` |
| cna_vertex_buffer_set_data | `cx-capi` |
| cna_vertex_declaration_create_with_stride | `cx-capi` |
| cna_vertex_declaration_destroy | `cx-capi` |
| cna_vertex_declaration_get_stride | `cx-capi` |
| cna_vertex_element_copy_string | `cx-capi` |
| cna_vertex_element_equals | `cx-capi` |
| cna_vertex_element_not_equals | `cx-capi` |
| cna_vertex_type_copy_elements | `cx-capi` |
| cna_vertex_type_get_stride | `cx-capi` |
| cna_vertex_value_copy_string | `cx-capi` |
| cna_vertex_value_equals | `cx-capi` |
| cna_vertex_value_init_default | `cx-capi` |
| cna_vibrate_controller_copy_device_name_ext | `cx-capi` |
| cna_vibrate_controller_get_is_supported_ext | `cx-capi` |
| cna_vibrate_controller_get_test_log_ext | `cx-capi` |
| cna_vibrate_controller_set_test_backend_ext | `cx-capi` |
| cna_vibrate_controller_start | `cx-capi` |
| cna_vibrate_controller_stop | `cx-capi` |
| cna_video_copy_type_name | `cx-capi` |
| cna_video_create_with_metadata | `cx-capi` |
| cna_video_destroy | `cx-capi` |
| cna_video_get_duration | `cx-capi` |
| cna_video_get_frames_per_second | `cx-capi` |
| cna_video_get_height | `cx-capi` |
| cna_video_get_width | `cx-capi` |
| cna_video_info_init | `cx-capi` |
| cna_video_player_copy_type_name | `cx-capi` |
| cna_video_player_create | `cx-capi` |
| cna_video_player_destroy | `cx-capi` |
| cna_video_player_get_state | `cx-capi` |
| cna_video_player_get_video | `cx-capi` |
| cna_video_player_get_volume | `cx-capi` |
| cna_viewport_copy_string | `cx-capi` |
| cna_viewport_get_aspect_ratio | `cx-capi` |
| cna_viewport_get_title_safe_area | `cx-capi` |
| cna_viewport_init_bounds | `cx-capi` |
| cna_viewport_project | `cx-capi` |
| cna_viewport_unproject | `cx-capi` |
| cna_wave_bank_create | `cx-capi` |
| cna_wave_bank_destroy | `cx-capi` |
<!-- END COVERAGE -->
