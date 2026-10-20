// Floor 8: content, platform and the CNA extensions.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec ContentWing()
{
    WingSpec w;
    w.name = "Content Pipeline";
    w.tagline = "ContentManager, loose files, compiled .cnb and .xnb, load failures, storage and resource lifetime";
    w.theme = "workshop";
    w.accent = {0.35f, 0.80f, 0.55f};
    w.spineSide = Wall::West;
    const auto ct = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin = ApiOrigin::Xna) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "content-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "workshop";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        ct("ct-load", "Loading Assets", "ContentManager.Load for textures, sounds and models, timed", "load", {"ContentManager.Load<Texture2D>", "ContentManager.Load<SoundEffect>", "ContentManager.Load<Model>", "ContentManager.RootDirectory", "ContentManager.ServiceProvider"}),
        ct("ct-cache", "The ContentManager Cache", "Cached loads, Unload and separate managers", "cache", {"ContentManager.Load", "ContentManager.Unload", "GraphicsResource.IsDisposed"}),
        ct("ct-formats", "Loose Image Formats", "PNG, JPEG, BMP, TGA, GIF and QOI decoded without a build step, a DXT1 DDS read from a stream (and TIFF, which is not)", "formats", {"ContentManager.Load<Texture2D>", "Texture2D.GetData", "Texture2D.DDSFromStreamEXT", "SurfaceFormat.Dxt1"}, ApiOrigin::CnaExtension),
        ct("ct-failure", "When Loading Fails", "ContentLoadException for missing, mistyped and corrupt assets", "failure", {"ContentLoadException", "ContentManager.Load"}),
        ct("ct-pipeline", "The Content Compiler", "cna-content builds .cnb and .xnb; loading and sizes compared with loose files", "pipeline", {"ContentManager.Load", "ContentManager.GetContentManifest"}, ApiOrigin::CnaExtension),
        []{
            RoomSpec r;
            r.key = "ct-intermediate";
            r.title = "The Pipeline in the Game";
            r.subtitle = "A content type written as XNA intermediate XML and read back, and what the pipeline says about a damaged document";
            r.exhibit = "pipeline-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "workshop";
            r.apis = {"IntermediateSerializer", "IntermediateSerializer.Serialize", "IntermediateSerializer.Deserialize", "IntermediateSerializer.GetTypeSerializer", "XmlImporter", "XmlImporter.Import", "XmlImporter.Attribute", "ContentImporterAttribute", "ContentItem", "ContentTypeDescriptor", "InvalidContentException", "ContentSerializerAttribute", "ContentSerializerCollectionItemNameAttribute", "ContentSerializerCollectionItemNameAttribute.CollectionItemName"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
        []{
            RoomSpec r;
            r.key = "ct-processor";
            r.title = "Processing a Texture";
            r.subtitle = "TextureImporter and TextureProcessor: colour key, premultiplied alpha, mipmaps, power-of-two resize and DXT compression, measured";
            r.exhibit = "processor-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "workshop";
            r.apis = {"TextureImporter", "TextureImporter.Import", "Processors::TextureProcessor", "Processors::TextureProcessor.Process", "Processors::TextureProcessor.ColorKeyEnabled", "Processors::TextureProcessor.ColorKeyColor", "Processors::TextureProcessor.PremultiplyAlpha", "Processors::TextureProcessor.GenerateMipmaps", "Processors::TextureProcessor.ResizeToPowerOfTwo",
                      "Processors::TextureProcessor.TextureFormat", "TextureProcessorOutputFormat", "Texture2DContent", "MipmapChain", "BitmapContent", "PixelBitmapContent"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
        []{
            RoomSpec r;
            r.key = "ct-fontbuild";
            r.title = "Building a Font";
            r.subtitle = "FontDescriptionProcessor rasterises a TrueType file into a glyph atlas at run time; the text on the screen is drawn with the font it made";
            r.exhibit = "fontbuild-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "workshop";
            r.apis = {"Graphics::FontDescription", "Microsoft::Xna::Framework::Content::Pipeline::FontDescriptionImporter", "Microsoft::Xna::Framework::Content::Pipeline::FontDescriptionImporter.Import", "Graphics::FontDescription.FontName", "Graphics::FontDescription.Size", "Graphics::FontDescription.Spacing", "Graphics::FontDescription.Style", "Graphics::FontDescription.UseKerning", "Processors::FontDescriptionProcessor", "Graphics::FontDescriptionStyle", "SpriteFontContent", "Graphics::FontDescription.Characters", "Graphics::FontDescription.DefaultCharacter", "ContentIdentity", "SpriteFont", "SpriteFont.MeasureString", "SpriteBatch.DrawString"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
        []{
            RoomSpec r;
            r.key = "ct-meshbuilder";
            r.title = "Building a Model";
            r.subtitle = "MeshBuilder collects positions, channels and triangles; ModelProcessor turns the MeshContent into a vertex buffer, indices and a bounding sphere that is drawn here";
            r.exhibit = "meshbuild-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "workshop";
            r.apis = {"MeshBuilder", "MeshBuilder.StartMesh", "MeshBuilder.CreatePosition", "MeshBuilder.CreateVertexChannel", "MeshBuilder.SetVertexChannelData", "MeshBuilder.AddTriangleVertex", "MeshBuilder.FinishMesh",
                      "MeshBuilder.MergeDuplicatePositions", "MeshBuilder.SwapWindingOrder", "MeshContent", "VertexChannelNames", "Processors::ModelProcessor", "Processors::ModelProcessor.Scale", "Processors::ModelProcessor.RotationY",
                      "Processors::ModelProcessor.GenerateTangentFrames", "Processors::ModelProcessor.SwapWindingOrder", "ModelContent", "ModelMeshContent", "ModelMeshPartContent", "VertexBufferContent", "VertexDeclarationContent"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
        []{
            RoomSpec r;
            r.key = "ct-modelimport";
            r.title = "Importing a Model File";
            r.subtitle = "XImporter and FbxImporter read DirectX .x, ASCII FBX 6.1, and modern binary FBX 7.5; ModelProcessor builds and draws the processed parts";
            r.exhibit = "modelimport-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "workshop";
            r.apis = {"XImporter", "FbxImporter", "Microsoft::Xna::Framework::Content::Pipeline::ContentImporter", "ContentImporterAttribute", "Microsoft::Xna::Framework::Content::Pipeline::ContentImporterContext", "NodeContent", "NodeContent.Children", "MeshContent", "GeometryContent", "MaterialContent", "BasicMaterialContent",
                      "BasicMaterialContent.DiffuseColor", "Processors::ModelProcessor", "ModelContent", "ModelMeshContent", "ModelMeshPartContent", "ModelBoneContent", "ContentProcessorContext.Convert"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
        []{
            RoomSpec r;
            r.key = "ct-compiler";
            r.title = "Writing an .xnb";
            r.subtitle = "ContentCompiler writes a processed texture as an .xnb with a chosen platform, profile and compression; the bytes load back through a ContentManager";
            r.exhibit = "compiler-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "workshop";
            r.apis = {"ContentCompiler", "ContentCompiler.Compile", "ContentCompiler.GetTypeWriter", "CompileOptions", "CompileOptions.targetPlatform", "CompileOptions.targetProfile", "CompileOptions.compressContent", "Microsoft::Xna::Framework::Content::Pipeline::TargetPlatform", "Texture2DContent", "Processors::TextureProcessor",
                      "TextureImporter", "ContentManager.OpenStream", "ContentManager.ReadAsset", "ContentManager.RouteLoadsThroughOpenStreamEXT"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
        []{
            RoomSpec r;
            r.key = "ct-audiopipeline";
            r.title = "Importing and Processing Audio";
            r.subtitle = "WavImporter or Mp3Importer decodes a sound, SoundEffectProcessor converts it for a quality, SongProcessor keeps a name and a duration; both versions are drawn and can be heard";
            r.exhibit = "audiopipeline-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "workshop";
            r.apis = {"Microsoft::Xna::Framework::Content::Pipeline::WavImporter", "Microsoft::Xna::Framework::Content::Pipeline::WavImporter.Import", "Mp3Importer", "Mp3Importer.Import", "Mp3Importer.Attribute", "ContentImporterAttribute", "AudioContent", "AudioContent.Format", "AudioContent.Data", "Audio::AudioFormat", "Audio::AudioFormat.SampleRate", "Audio::AudioFormat.ChannelCount", "Audio::AudioFormat.BitsPerSample",
                      "Audio::AudioFormat.NativeWaveFormat", "ConversionQuality", "Processors::SoundEffectProcessor", "Processors::SoundEffectProcessor.Quality", "SoundEffectContent", "Processors::SongProcessor", "SongContent", "SoundEffect.FromStream", "ContentCompiler.Compile(SoundEffectContent)", "ContentCompiler.Compile(SongContent)"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
        ct("ct-storage", "Title Files and Saved Data", "TitleContainer.OpenStream, StorageDevice and StorageContainer", "storage", {"TitleContainer.OpenStream", "StorageDevice", "StorageDevice.BeginShowSelector", "StorageDevice.EndShowSelector", "StorageDevice.BeginOpenContainer", "StorageDevice.EndOpenContainer", "StorageDevice.IsConnected", "StorageDevice.DeviceChanged", "StorageDevice.SetAppNameEXT", "StorageContainer", "StorageContainer.DisplayName", "StorageContainer.CreateFile", "StorageContainer.OpenFile", "StorageContainer.FileExists", "StorageContainer.DeleteFile", "StorageContainer.CreateDirectory", "StorageContainer.DeleteDirectory", "StorageContainer.DirectoryExists", "StorageContainer.GetFileNames", "StorageContainer.GetDirectoryNames", "StorageDevice.GetStorageRootEXT", "StorageDevice.DeleteContainer", "StorageDevice.TotalSpace", "StorageDevice.FreeSpace", "StorageContainer.Dispose", "StorageContainer.IsDisposed", "StorageContainer.Disposing", "StorageDeviceNotConnectedException"}),
        ct("ct-custom", "Teaching ContentManager a New Type", "Your own types loaded through ContentManager: a loose-file reader and a named .cnj loader", "custom", {"ContentManager.RegisterTypeReader", "LooseFileContentTypeReader", "ContentManager.RegisterCnjLoader", "ContentLoadException"}, ApiOrigin::CnaExtension),
        ct("ct-reader", "Content Readers", "An .xnb file with your own type: a ContentTypeReader registered under the name the file records", "reader",
           {"ContentTypeReader", "ContentTypeReaderManager.AddTypeCreator", "ContentTypeReaderManager.GetTypeReader", "ContentReader", "ContentReader.ContentManager", "ContentReader.AssetName", "ContentReader.Platform", "ContentReader.Version", "ContentReader.ReadBoundingSphere", "ContentReader.ReadColor", "ContentReader.ReadMatrix", "ContentReader.ReadQuaternion", "ContentReader.ReadVector2", "ContentReader.ReadVector3", "ContentReader.ReadVector4", "ContentManager.Load"}),
        ct("ct-lifetime", "Resource Lifetime", "Dispose, IsDisposed, the Disposing event and ContentManager.Unload", "lifetime", {"GraphicsResource.Dispose", "GraphicsResource.IsDisposed", "GraphicsResource.Disposing", "GraphicsDevice.ResourceCreated", "GraphicsDevice.ResourceDestroyed", "ResourceCreatedEventArgs", "ResourceCreatedEventArgs.Resource", "ResourceDestroyedEventArgs", "ResourceDestroyedEventArgs.Name", "ResourceDestroyedEventArgs.Tag", "ContentManager.Unload"}),
        ct("ct-resource", "Content from Memory", "ResourceContentManager serving compiled .xnb assets from a System.Resources.ResourceManager instead of the content folder", "resource",
           {"ResourceContentManager", "ResourceContentManager.OpenStream", "System.Resources.ResourceManager", "ContentManager.Load", "ContentLoadException"}),
    };
    return w;
}



RoomSpec ShadowRoom()
{
    RoomSpec r;
    r.key = "cx-shadow";
    r.title = "Shadow Mapping";
    r.subtitle = "A caster pass into a float render target (optionally cascaded), then BasicEffect receives the shadows";
    r.exhibit = "shadow-lab";
    r.width = 15;
    r.depth = 15;
    r.height = 5.5f;
    r.theme = "futuristic";
    r.apis = {"IShadowReceiverEXT", "BasicEffect.ShadowMapEXT", "BasicEffect.LightViewProjectionEXT", "BasicEffect.ShadowDepthBiasEXT", "BasicEffect.ShadowFilterRadiusEXT", "ShadowCascadeStateEXT", "BasicEffect.ShadowCascadesEXT", "DynamicVertexBuffer", "VertexPositionColor", "BasicEffect.VertexColorEnabled", "Matrix.CreateOrthographic"};
    r.origin = ApiOrigin::CnaExtension;
    return r;
}

WingSpec CnaWing()
{
    WingSpec w;
    w.name = "CNA Extensions";
    w.tagline = "What CNA adds to XNA 4.0: DebugDraw, post-process effects, engine statistics, point lists and device queries";
    w.theme = "futuristic";
    w.accent = {0.35f, 0.80f, 0.55f};
    w.spineSide = Wall::East;
    const auto cx = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "cnaext-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "futuristic";
        r.apis = std::move(apis);
        r.origin = ApiOrigin::CnaExtension;
        return r;
    };
    w.rooms = {
        cx("cx-debugdraw", "DebugDraw", "Wire boxes, spheres, frusta and lines batched into one call", "debugdraw", {"CNA::Graphics::DebugDraw", "DebugDraw.addBox", "DebugDraw.addSphere", "DebugDraw.addFrustum", "DebugDraw.setDepthTested"}),
        cx("cx-crt", "CRT Effect", "Scanlines, curvature, vignette and a phosphor mask as a SpriteBatch effect", "crt", {"CNA::Graphics::CRTEffect", "CRTMaskType", "SpriteBatch.Begin(effect)"}),
        cx("cx-depth", "Colour Depth Effect", "16-bit, 8-bit, grayscale and palette looks with ordered dithering", "depth", {"CNA::Graphics::DepthEffect", "DepthEffectMode", "DitherMode"}),
        cx("cx-ascii", "ASCII Post-Process", "A texture redrawn as text, in black and white or colour", "ascii", {"CNA::Graphics::AsciiPostProcessEffect", "AsciiQuantizeMode"}),
        cx("cx-package", "Shader Packages", "One shader in several languages: the package picks the variant this renderer can run", "package", {"CNA::Graphics::ShaderPackageEXT", "ShaderPackageEXT.selectFor", "ShaderCodeEXT", "ShaderEffect(device, package)", "ShaderLanguageEXT", "ShaderEffect.DeclareUniformBlockEXT"}),
        ShadowRoom(),
        cx("cx-stats", "Engine Statistics", "Draw calls, primitives, state changes and every live graphics resource", "stats", {"CNA::Diagnostics::GetProvider", "Snapshot.metrics", "Snapshot.resources", "SetRuntimeMode"}),
        cx("cx-points", "Point Lists", "PrimitiveType::PointListEXT and debug string markers", "points", {"PrimitiveType.PointListEXT", "GraphicsDevice.DrawUserPrimitives", "GraphicsDevice.SetStringMarkerEXT"}),
        cx("cx-devices", "Device Extensions", "Power, locale, CPU and memory, display scale and safe area", "devices", {"CNA::Devices::SystemInfo", "CNA::Devices::Locale", "CNA::Devices::PowerInfo", "CNA::Devices::DisplayInfo"}),
    };
    return w;
}


WingSpec PlatformWing()
{
    WingSpec w;
    w.name = "Platform and Renderer";
    w.tagline = "The window, the display adapter, the active renderer and the platform, as CNA reports them";
    w.theme = "control-room";
    w.accent = {0.35f, 0.80f, 0.55f};
    w.spineSide = Wall::West;
    const auto pl = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "platform-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "control-room";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        [&] {
            RoomSpec r = pl("pl-exceptions", "Exceptions CNA Throws", "Deliberate mistakes and the XNA exception each one raises, with CNA's own message", "", {"System.ObjectDisposedException", "System.ArgumentOutOfRangeException", "System.InvalidOperationException", "ContentLoadException", "System.IO.FileNotFoundException",
                                                                                                            "GamerServicesNotAvailableException", "GamerPrivilegeException", "Guide.ShowMarketplace", "SignedInGamer.IsFriend", "GuideAlreadyVisibleException", "StorageContainer", "TitleContainer.OpenStream", "SpriteBatch.Begin", "SpriteBatch.End"}, ApiOrigin::Xna);
            r.exhibit = "exception-lab";
            r.params = {};
            return r;
        }(),
        [&] {
            RoomSpec r = pl("pl-log", "CNA's Log", "CNA::Logger: seven levels, nine categories, a minimum level, conditional lines and a sink that receives what the engine itself writes", "", {"CNA::Logger", "Logger.Log", "Logger.InfoIf", "Logger.WarnIf", "Logger.SetMinimumLevel", "Logger.GetMinimumLevel", "Logger.SetSink", "Logger.ResetSink", "CNA::LogLevel", "CNA::LogCategory", "Logger.ErrorIf", "Logger.Warn", "Logger.Info"}, ApiOrigin::CnaExtension);
            r.exhibit = "log-lab";
            r.params = {};
            return r;
        }(),
        [&] {
            RoomSpec r = pl("pl-diagnostics", "Measuring Your Own Code", "CNA::Diagnostics for the program's own counters, gauges, zones, markers, a custom resource and a per-frame source, with a recorded trace written as Chrome Trace JSON", "",
                            {"CNA::Diagnostics::CounterHandle", "CNA::Diagnostics::GaugeHandle", "CNA::Diagnostics::FrameCounterHandle", "CNA::Diagnostics::NameHandle", "CNA::Diagnostics::ZoneScope", "CNA::Diagnostics::MarkEvent",
                             "CNA::Diagnostics::ResourceHandle", "CNA::Diagnostics::ResourceDescriptor", "CNA::Diagnostics::IDiagnosticsSource", "CNA::Diagnostics::RegisterSource", "CNA::Diagnostics::SourceRegistration", "CNA::Diagnostics::FrameStatisticsSink",
                             "CNA::Diagnostics::GetProvider", "IDiagnosticsProvider.ReadEvents", "IDiagnosticsProvider.ResolveName", "CNA::Diagnostics::SetRuntimeMode", "CNA::Diagnostics::StartRecording", "CNA::Diagnostics::StopRecording",
                             "CNA::Diagnostics::Trace", "Trace.WriteChromeTrace", "Trace.WriteBinary", "Trace.ReadBinary", "CNA::Diagnostics::EventRecord", "CNA::Diagnostics::EventBatch", "CNA::Diagnostics::MetricKind", "CNA::Diagnostics::ResourceKind",
                             "CNA_DIAGNOSTICS_COUNTER_ADD", "CNA_DIAGNOSTICS_GAUGE_SET", "CNA_DIAGNOSTICS_GAUGE_ADD", "CNA_DIAGNOSTICS_FRAME_COUNTER_ADD", "CNA_PROFILE_SCOPE", "CNA_DIAGNOSTICS_EVENT"},
                            ApiOrigin::CnaExtension);
            r.exhibit = "diagnostics-lab";
            r.params = {};
            return r;
        }(),
        [&] {
            RoomSpec r = pl("pl-inspector", "The Inspector", "CNA's optional Inspector: an agent serves diagnostics over an authenticated loopback socket and a client reads snapshots and events", "",
                            {"CNA::Inspector::Agent", "Agent.Start", "Agent.Stop", "Agent.GetPort", "Agent.GetAuthenticationToken", "AgentConfiguration", "CNA::Inspector::Client", "Client.Connect", "Client.Ping", "Client.CaptureSnapshot", "Client.ReadEvents",
                             "Client.RequestPreview", "HelloResponse", "SnapshotResponse", "EventsResponse", "PreviewResponse", "Capability", "SnapshotPart"},
                            ApiOrigin::CnaExtension);
            r.exhibit = "inspector-lab";
            r.params = {};
            return r;
        }(),
        pl("pl-window", "The Game Window", "GameWindow title, client bounds, resizing, activation and file drops", "window", {"GameWindow.GetNativeWindowHandleEXT", "GameWindow.CurrentOrientation", "GameWindow.OrientationChanged", "GraphicsDeviceManager.SupportedOrientations", "CNA::Platform::NativeWindowHandle", "GameWindow.Title", "GameWindow.ClientBounds", "GameWindow.AllowUserResizing", "GameWindow.ClientSizeChanged", "Game.Activated", "GameWindow.FileDropEXT", "PhoneApplicationService", "PhoneApplicationService.AttachEXT", "PhoneApplicationService.Launching", "PhoneApplicationService.Activated", "PhoneApplicationService.Deactivated", "PhoneApplicationService.Closing", "PhoneApplicationService.State", "LaunchingEventArgs", "ActivatedEventArgs", "DeactivatedEventArgs", "ClosingEventArgs"}, ApiOrigin::Xna),
        pl("pl-adapter", "Adapter and Back Buffer", "GraphicsAdapter, display modes, GraphicsProfile and PresentationParameters", "adapter", {"GraphicsAdapter", "GraphicsAdapter.UseNullDevice", "GraphicsAdapter.UseReferenceDevice", "GraphicsAdapter.GetCurrentDeviceTypeEXT", "GraphicsAdapter.GetRequiredRendererEXT", "Graphics::DisplayMode", "GraphicsDevice.GraphicsProfile", "PresentationParameters", "GraphicsAdapter.IsProfileSupported", "GraphicsAdapter.QueryRenderTargetFormat", "GraphicsAdapter.QueryBackBufferFormat", "GraphicsAdapter.Adapters", "GraphicsAdapter.IsWideScreenAspectRatioEXT"}, ApiOrigin::Xna),
        pl("pl-renderer", "The Active Renderer", "Renderer identity, maturity, features, limits, surface formats and the device's own colour-space and support queries", "renderer", {"GraphicsDevice.GetRendererCapabilityProfileEXT", "GraphicsDevice.GetRendererCapabilityReportEXT", "RendererFeature", "RendererLimit", "RendererFormatUsage", "GraphicsDevice.GetGraphicsRendererName", "GraphicsDevice.GetDisplayColorSpaceEXT", "GraphicsDevice.SupportsDisplayColorSpaceEXT", "GraphicsDevice.SetDisplayColorSpaceEXT", "GraphicsDevice.SupportsImageBasedLightingEXT", "GraphicsDevice.GetTrackedResourceCount", "GraphicsDevice.GetLogicalSizeEXT"}, ApiOrigin::CnaExtension),
        pl("pl-platform", "Platform Capabilities", "What the host platform can do: clipboard, IME, gamepads, haptics, camera, dialogs", "platform", {"Game.GetPlatformCapabilitiesEXT", "PlatformCapabilities", "Game.GetPlatformEXT", "CNA::Platform::IPlatform", "IPlatform.GetName", "IPlatform.IsSubsystemInitialized", "IPlatform.GetPerformanceFrequency", "IPlatform.GetTicksMilliseconds", "IPlatform.GetKeyboard"}, ApiOrigin::CnaExtension),
    };
    return w;
}


WingSpec HistoryWing()
{
    WingSpec w;
    w.name = "History and Versions";
    w.tagline = "Where XNA came from, and what this very build of CNA is made of";
    w.theme = "wood-gallery";
    w.accent = {0.35f, 0.80f, 0.55f};
    w.spineSide = Wall::East;
    const auto hi = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "history-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "wood-gallery";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        hi("hist-xna", "XNA, 2004 to Today", "The releases of XNA Game Studio and what happened to it", "xna", {"SpriteBatch.DrawString"}, ApiOrigin::Xna),
        hi("hist-cna", "This Build of CNA", "The revision, options, renderers and maturity of the CNA running right now", "cna", {"CNA::GraphicsRendererSelection.GetAvailable", "CNA::GraphicsRendererSelection.GetSelected", "CNA::GraphicsRendererSelection.GetActive", "CNA::GraphicsRendererSelection.IsFallbackEnabled", "CNA::GraphicsRendererSelection.GetFallbackHistory", "CNA::GraphicsRendererFallbackReason", "CNA::getGraphicsRendererMaturity", "CNA::getGraphicsRendererName"}, ApiOrigin::CnaExtension),
        hi("hist-compare", "XNA 4.0 and What CNA Adds", "How many rooms use XNA features, CNA extensions or application-level behavior, counted live", "compare", {"SpriteBatch.Draw", "SpriteBatch.DrawString", "BlendState.NonPremultiplied"}, ApiOrigin::Application),
    };
    return w;
}

}  // namespace

WingSpec PlayersWing()
{
    WingSpec w;
    w.name = "Players and Networking";
    w.tagline = "GamerServices and Net: signed-in players, achievements, the Guide, packets and a session between two local players";
    w.theme = "control-room";
    w.accent = {0.35f, 0.80f, 0.55f};
    w.spineSide = Wall::East;
    const auto room = [](const char* key, const char* title, const char* subtitle, const char* exhibit, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = exhibit;
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "control-room";
        r.apis = std::move(apis);
        r.origin = ApiOrigin::Xna;
        return r;
    };
    w.rooms = {
        room("gs-players", "Signed-in Players", "Gamer.SignedInGamers, presence, privileges, game defaults and the profile: two local offline players", "players-lab", "players",
             {"Gamer.SignedInGamers", "SignedInGamer.Presence", "SignedInGamer.Privileges", "SignedInGamer.GameDefaults", "Gamer.GetProfile", "GamerProfile", "GamerProfile.Motto", "GamerProfile.Region", "GamerProfile.GetGamerPicture", "GameDefaults.ControllerSensitivity", "GameDefaults.RacingCameraAngle", "GameDefaults.AutoCenter", "GameDefaults.ManualTransmission", "GameDefaults.AccelerateWithButtons", "GameDefaults.PrimaryColor", "SignedInGamer.SignedIn", "GamerPresenceMode"}),
        room("gs-achievements", "Achievements", "A catalog the title ships, awards kept by the framework: the museum awards them as you explore", "players-lab", "achievements",
             {"SignedInGamer.AwardAchievement", "SignedInGamer.GetAchievements", "Achievement", "AchievementCollection"}),
        room("gs-leaderboards", "Leaderboards", "LeaderboardWriter sets a gamer's rating, LeaderboardReader pages through the ranking or reads around one gamer", "players-lab", "leaderboards",
             {"LeaderboardWriter", "LeaderboardWriter.GetLeaderboard", "LeaderboardEntry", "LeaderboardEntry.Gamer", "LeaderboardEntry.Rating", "LeaderboardEntry.Columns", "LeaderboardReader", "LeaderboardIdentity", "LeaderboardKey", "LeaderboardOutcome", "PropertyDictionary", "PropertyDictionary.Count", "PropertyDictionary.Keys", "PropertyDictionary.Values", "PropertyDictionary.ContainsKey", "PropertyDictionary.SetValue", "PropertyDictionary.GetValueInt32", "PropertyDictionary.GetValueOutcome", "Gamer.LeaderboardWriter", "NetworkSession.WriteArbitratedLeaderboard", "NetworkSession.WriteUnarbitratedLeaderboard", "NetworkSession.WriteTrueSkill", "WriteLeaderboardsEventArgs.Gamer", "WriteLeaderboardsEventArgs.IsLeaving"}),
        room("gs-guide", "The Guide", "Message boxes and text input drawn by the platform overlay, trial mode and notification position", "players-lab", "guide",
             {"Guide.BeginShowMessageBox", "Guide.EndShowMessageBox", "Guide.BeginShowKeyboardInput", "Guide.IsTrialMode", "Guide.SimulateTrialMode", "Guide.IsVisible", "Guide.IsScreenSaverEnabled", "Guide.DelayNotifications", "GamerServicesDispatcher.IsInitialized", "GamerServicesDispatcher.WindowHandle", "Guide.NotificationPosition", "MessageBoxIcon"}),
        room("gs-avatar", "Avatars", "AvatarDescription, AvatarAnimation and AvatarRenderer: CNA's original avatars standing in the museum's own scene", "avatar-lab", "avatar",
             {"AvatarDescription", "AvatarDescription.CreateRandom", "AvatarDescription.BeginGetFromGamer", "AvatarRenderer", "AvatarAnimation", "AvatarAnimationPreset", "AvatarExpression", "AvatarBodyType", "AvatarBone", "AvatarEye", "AvatarMouth", "AvatarEyebrow", "AvatarRendererState", "IAvatarAnimation"}),
        room("net-packets", "Packets", "PacketWriter and PacketReader: XNA's types as bytes, in order, with their cost", "net-lab", "packets",
             {"PacketWriter", "PacketWriter.Length", "PacketWriter.Position", "PacketWriter.Write", "PacketReader", "PacketReader.Length", "PacketReader.Position", "PacketReader.ReadVector3", "PacketReader.ReadQuaternion", "PacketReader.ReadMatrix"}),
        room("net-session", "A Network Session", "A Local session between two players: send, receive, simulated latency and loss, start and end", "net-lab", "session",
             {"NetworkSession.AllowHostMigration", "NetworkSession.SessionType", "NetworkSession.MaxGamers", "NetworkSession.PrivateGamerSlots", "NetworkSession.Host", "NetworkSession.RemoteGamers", "NetworkSession.PreviousGamers", "QualityOfService.MinimumRoundtripTime", "QualityOfService.BytesPerSecondDownstream", "NetworkSession.ResetReady", "NetworkSession.AddLocalGamer", "NetworkSession.FindGamerById", "NetworkGamer.Id", "NetworkGamer.IsPrivateSlot", "NetworkGamer.HasVoice", "NetworkGamer.IsTalking", "NetworkGamer.IsMutedByLocalUser", "NetworkGamer.HasLeftSession", "NetworkSession.Create", "NetworkSession.Update", "NetworkSession.StartGame", "NetworkSession.EndGame", "NetworkSession.SimulatedLatency", "NetworkSession.SimulatedPacketLoss", "NetworkSession.SessionState",
              "LocalNetworkGamer.SendData", "LocalNetworkGamer.ReceiveData", "SendDataOptions", "NetworkGamer.IsReady", "NetworkSession.GamerJoined", "NetworkSession.GameStarted", "NetworkSession.GameEnded", "NetworkSession.GamerLeft", "GamerJoinedEventArgs", "GamerJoinedEventArgs.Gamer", "NetworkSession.BeginFind", "NetworkSession.EndFind", "AvailableNetworkSession", "AvailableNetworkSession.CurrentGamerCount", "AvailableNetworkSession.HostGamertag", "AvailableNetworkSession.OpenPrivateGamerSlots", "AvailableNetworkSession.OpenPublicGamerSlots", "AvailableNetworkSession.QualityOfService", "AvailableNetworkSession.SessionProperties", "AvailableNetworkSessionCollection", "AvailableNetworkSessionCollection.IsDisposed", "QualityOfService", "NetworkSessionProperties", "NetworkSession.SessionProperties", "NetworkSession.AllowJoinInProgress", "NetworkSession.BeginJoin", "NetworkSession.EndJoin", "NetworkGamer.IsGuest", "NetworkGamer.IsLocal", "NetworkGamer.IsHost", "NetworkGamer.Machine", "NetworkGamer.RoundtripTime", "NetworkGamer.Session", "NetworkMachine", "NetworkMachine.Gamers", "NetworkMachine.RemoveFromSession", "NetworkSession.AllGamers", "NetworkSessionJoinException", "NetworkSessionJoinError", "NetworkSessionEndReason", "NetworkSessionEndedEventArgs", "NetworkSessionEndedEventArgs.EndReason", "HostChangedEventArgs", "HostChangedEventArgs.NewHost", "HostChangedEventArgs.OldHost", "NetworkSession.HostChanged", "NetworkSession.SessionEnded", "NetworkSession.InviteAccepted", "NetworkSession.BeginJoinInvited", "NetworkSession.EndJoinInvited", "InviteAcceptedEventArgs", "InviteAcceptedEventArgs.Gamer", "InviteAcceptedEventArgs.IsCurrentSession", "NetworkSessionType.PlayerMatch", "LocalNetworkGamer.SendPartyInvites"}),
    };
    return w;
}

WingSpec OnlineWing()
{
    WingSpec w;
    w.name = "Online Service";
    w.tagline = "CNA's account service: accounts, friends, messages and parties through the Guide (offline unless tools/online_stack.sh runs one)";
    w.theme = "control-room";
    w.accent = {0.45f, 0.75f, 0.95f};
    w.spineSide = Wall::East;
    const auto room = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "online-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "control-room";
        r.apis = std::move(apis);
        r.origin = ApiOrigin::Xna;
        return r;
    };
    w.rooms = {
        room("on-account", "Accounts and the Guide", "Guide.ShowSignIn for real accounts, the profile of a signed-in gamer, achievements and the content page", "account",
             {"Guide.ShowSignIn", "Guide.ShowAchievementsEXT", "Guide.ShowMarketplace", "SignedInGamer.IsSignedInToLive", "GamerProfile.GamerScore", "GamerProfile.Reputation", "GamerProfile.TotalAchievements", "CNA::GamerServices::resolveConfiguration"}),
        room("on-friends", "Friends and Presence", "SignedInGamer.GetFriends, FriendGamer presence and flags, the Guide's friends, players and gamer-card pages", "friends",
             {"SignedInGamer.GetFriends", "FriendCollection", "FriendCollection.IsDisposed", "FriendGamer", "FriendGamer.IsOnline", "FriendGamer.IsAway", "FriendGamer.IsBusy", "FriendGamer.IsPlaying", "FriendGamer.IsJoinable", "FriendGamer.Presence", "Guide.ShowFriends",
              "Guide.ShowPlayers", "Guide.ShowGamerCard", "Guide.ShowPlayerReview", "Guide.ShowFriendRequest"}),
        room("on-messages", "Messages", "Guide.ShowMessages and ShowComposeMessage: the player's inbox and a message to friends", "messages", {"Guide.ShowMessages", "Guide.ShowComposeMessage"}),
        room("on-party", "Parties", "Guide.ShowParty and ShowPartySessions, and SignedInGamer.PartySize", "party", {"Guide.ShowParty", "Guide.ShowPartySessions", "SignedInGamer.PartySize"}),
    };
    return w;
}

// Added after the other floor-8 wings so that no existing room moves: pipeline operations checked exactly.
WingSpec WorkshopWing()
{
    WingSpec w;
    w.name = "Pipeline Workshop";
    w.tagline = "Content-pipeline operations on data built in the room, each result checked exactly";
    w.theme = "workshop";
    w.accent = {0.85f, 0.65f, 0.35f};
    w.spineSide = Wall::West;
    const auto wk = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin = ApiOrigin::Xna) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "pipeline-checks";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "workshop";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        wk("wk-meshhelper", "MeshHelper", "Merging positions and vertices, normals, tangent frames, cache order, winding, TransformScene and skeletons", "meshhelper",
           {"MeshHelper", "MeshHelper.MergeDuplicatePositions", "MeshHelper.MergeDuplicateVertices(MeshContent)", "MeshHelper.CalculateNormals", "MeshHelper.CalculateTangentFrames", "MeshHelper.OptimizeForCache",
            "MeshHelper.SwapWindingOrder", "MeshHelper.TransformScene", "MeshHelper.FindSkeleton", "MeshHelper.FlattenSkeleton", "MeshBuilder", "MeshBuilder.StartMesh", "MeshBuilder.CreatePosition",
            "MeshBuilder.AddTriangleVertex", "MeshBuilder.CreateVertexChannel", "MeshBuilder.SetVertexChannelData", "MeshBuilder.FinishMesh", "MeshContent", "MeshContent.Positions", "MeshContent.Geometry",
            "GeometryContent", "GeometryContent.Indices", "GeometryContent.Vertices", "VertexContent.PositionIndices", "VertexContent.VertexCount", "VertexContent.Channels", "VertexChannelCollection.Contains",
            "VertexChannelCollection.Get", "VertexChannelNames.Normal", "VertexChannelNames.Tangent", "VertexChannelNames.Binormal", "VertexChannelNames.TextureCoordinate", "BoneContent", "NodeContent.Children",
            "NodeContent.AbsoluteTransform"}),
        wk("wk-typewriter", "A Content Type of Your Own", "ContentTypeWriter and ContentTypeReader for a game type: compiled, loaded back, compared, and broken files refused", "typewriter",
           {"Compiler.ContentTypeWriter", "ContentTypeWriterBase.GetRuntimeReader", "ContentTypeWriterBase.TypeVersion", "ContentTypeWriterBase.TargetType", "ContentTypeWriterAttribute", "ContentCompiler", "ContentCompiler.AddTypeWriter",
            "ContentCompiler.GetTypeWriter", "ContentCompiler.Compile", "CompileOptions", "ContentWriter", "ContentWriter.Write", "ContentWriter.Write7BitEncodedInt", "ContentWriter.WriteObject",
            "ContentWriter.WriteRawObject", "ContentWriter.WriteSharedResource", "ContentItem", "ContentTypeReader", "ContentTypeReaderBase.TypeVersion", "ContentTypeReaderManager.AddTypeCreator",
            "ContentTypeReaderManager.RemoveTypeCreatorEXT", "ContentTypeReaderManager.IsRegistered", "ContentReader.ReadObject", "ContentReader.ReadRawObject", "ContentReader.ReadSharedResource",
            "ContentReader.ReadColor", "ContentReader.ReadMatrix", "ContentReader.ReadQuaternion", "ContentReader.ReadVector2", "ContentReader.ReadVector3", "ContentReader.ReadVector4", "ContentReader.AssetName",
            "ContentManager.Load", "ContentManager.OpenStream", "ContentLoadException", "ReflectiveTypeReader", "ReflectiveTypeReaderBuilder", "ReflectiveTypeReaderBuilder.Field",
            "ReflectiveTypeReaderBuilder.Base", "ReflectiveTypeReaderBuilder.EnumField", "ReflectiveTypeReaderBuilder.Custom", "ReflectiveTypeReaderBuilder.Register", "ReflectiveTypeReaderBuilder.RegisterShared",
            "ReflectiveTypeReaderBuilder.SharedResourceField", "ReflectiveSharedTypeReader", "EnumTypeReader", "CanonicalReflectiveReaderNameEXT"}),
        wk("wk-xml", "XmlSerializer and XNA Values", "A settings record with vectors, a quaternion, a matrix and Keys, saved as XML and read back bit for bit", "xml",
           {"Vector2", "Vector3", "Vector4", "Quaternion", "Matrix", "Keys", "SharpXmlRootName", "SharpXmlMembers", "SharpXmlEnumEntries", "Quaternion.CreateFromYawPitchRoll", "Matrix.CreatePerspectiveFieldOfView",
            "Matrix.CreateLookAt", "System::Xml::Serialization::XmlSerializer"},
           ApiOrigin::CnaExtension),
        wk("wk-cnbloader", "A Custom .cnb Asset", "A game-defined type in CNA's own container: CnbWriter, CnbLoaderRegistry, ContentManager.Load; and a ContentTypeSerializer of its own", "cnbloader",
           {"CnbLoaderRegistry", "CnbLoaderRegistry.Register", "CnbLoaderRegistry.Remove", "CnbLoaderRegistry.IsRegistered", "CnbLoaderRegistry.RegisteredTypeName", "CnbAssetTypeIdFromName", "IsCustomAssetTypeId",
            "CnbWriter", "CnbWriter.SetMetadata", "CnbWriter.AddChunk", "CnbWriter.Build", "CnbByteWriter", "CnbByteWriter.WriteString", "CnbByteWriter.WriteI32", "CnbByteWriter.WriteF32", "CnbByteWriter.WriteF64",
            "CnbByteWriter.WriteU64", "CnbByteWriter.WriteU32", "CnbByteWriter.Take", "CnbDocument", "CnbDocument.Parse", "CnbDocument.HasMagic", "CnbDocument.AssetTypeId", "CnbDocument.AssetSchemaVersion",
            "CnbDocument.Metadata", "CnbDocument.FindSingle", "CnbDocument.RequireSingle", "CnbDocument.RequireAsset", "CnbDocument.OpenChunk", "CnbDocument.ChunkAt", "CnbDocument.ChunkData", "CnbChunkEntry.IsMandatory",
            "ChunkIdToString", "MakeChunkId", "CnbByteReader", "CnbByteReader.ReadString", "CnbByteReader.ReadI32", "CnbByteReader.ReadF32", "CnbByteReader.ReadF64", "CnbByteReader.ReadU64",
            "CnbByteReader.ReadCount", "CnbByteReader.RequireExhausted", "Crc32c", "ContentManager.Load", "ContentLoadException", "TitleLocation.Path", "ContentTypeSerializer", "IntermediateSerializer.AddTypeSerializer",
            "IntermediateSerializer.FindTypeSerializer", "IntermediateSerializer.Serialize", "IntermediateSerializer.Deserialize", "IntermediateWriter.WriteObject", "IntermediateReader.ReadObject",
            "ContentSerializerAttribute.ElementName", "InvalidContentException", "ContentTypeSerializerAttribute"},
           ApiOrigin::CnaExtension),
        wk("wk-importers", "Importers and Codecs", "WmaImporter, PassThroughProcessor, build-time MP3/WMA/WMV decoding and the XMA encoder service, with their availability reported honestly", "importers",
           {"WmaImporter", "WmaImporter.Import", "WmaImporter.Attribute", "ContentImporterAttribute.FileExtensions", "ContentImporterAttribute.DefaultProcessor", "AudioContent", "AudioContent.FileType", "AudioContent.Format",
            "AudioContent.Duration", "AudioContent.Data", "AudioFormat.ChannelCount", "AudioFormat.BitsPerSample", "AudioFormat.SampleRate", "AudioFormat.Format", "AudioFileType", "PassThroughProcessor", "PassThroughProcessor.Process", "ContentProcessor.InputTypeName",
            "ContentProcessor.OutputTypeName", "BuildTimeMedia::IsAvailable", "BuildTimeMedia::UnavailableReason", "BuildTimeMedia::Identity", "BuildTimeMedia::DecodeAudio", "BuildTimeMedia::DecodedAudio",
            "BuildTimeMedia::AudioSourceFormat", "BuildTimeMedia::ProbeVideo", "BuildTimeMedia::ProbedVideo", "BuildTimeMedia::ProbeAudioDurationTicks", "BuildTimeMedia::EncodeWindowsMediaAudio",
            "XmaEncodeQualityName", "XmaEncodeQuality", "XmaEncodeRequest", "XmaEncodeResult", "XmaEncoderService", "XmaEncoderService.Available", "XmaEncoderService.UnavailableReason",
            "XmaEncoderService.Encode", "XmaEncoderService.Identity", "XmaEncoderIdentity.ToString", "XmaEncoderUnavailableSentence", "MakeUnavailableXmaEncoder", "MakeExternalXmaEncoder", "BuildXmaEncoder",
            "System.IO.FileNotFoundException"}),
        wk("wk-material", "MaterialProcessor", "A BasicMaterialContent naming a texture: the texture built by TextureProcessor, the stock material types, and the refusals", "material",
           {"MaterialProcessor", "MaterialProcessor.Process", "MaterialProcessor.ColorKeyColor", "MaterialProcessor.ColorKeyEnabled", "MaterialProcessor.GenerateMipmaps", "MaterialProcessor.PremultiplyTextureAlpha",
            "MaterialProcessor.ResizeTexturesToPowerOfTwo", "MaterialProcessor.TextureFormat", "MaterialProcessor.DefaultEffect", "MaterialProcessorDefaultEffect", "TextureProcessorOutputFormat", "MaterialContent",
            "MaterialContent.Textures", "TextureReferenceDictionary", "BasicMaterialContent", "BasicMaterialContent.DiffuseColor", "BasicMaterialContent.EmissiveColor", "BasicMaterialContent.SpecularColor",
            "BasicMaterialContent.SpecularPower", "BasicMaterialContent.Alpha", "BasicMaterialContent.VertexColorEnabled", "BasicMaterialContent.Texture", "SkinnedMaterialContent", "EnvironmentMapMaterialContent",
            "DualTextureMaterialContent", "AlphaTestMaterialContent", "EffectMaterialContent", "EffectMaterialContent.Effect", "ExternalReference", "ExternalReference.Filename", "InvalidContentException",
            "XnaBridgeProcessorContext", "CNA::Content::Pipeline::ContentProcessorContext", "ContentProcessorContext.NestedOutputs", "ContentDependencyCollector.Dependencies", "ContentPipeline", "ContentPipelineRegistry",
            "RegisterTexture2DContentPipeline", "MakeBlockCompressionTextureEncoder", "RegisterXnaModelSourceContentPipeline", "RegisterXnbOutputContentPipeline", "Texture2D.GetData", "Texture.LevelCount",
            "System.ArgumentNullException"}),
        wk("wk-processor", "A Processor and Its Context", "A game's own importer and processors on CNA's real processor context: dependencies, output files, Convert, BuildAsset, BuildAndLoadAsset", "processor",
           {"Framework.Content.Pipeline.ContentImporter", "Framework.Content.Pipeline.ContentImporter.Import", "ContentImporterAttribute", "ContentImporterAttribute.DisplayName", "Framework.Content.Pipeline.ContentProcessor",
            "Framework.Content.Pipeline.ContentProcessor.Process", "ContentProcessorAttribute",
            "ContentProcessorAttribute.DisplayName", "ProcessorParameterBindings", "Framework.Content.Pipeline.ContentProcessorContext", "ContentProcessorContext.AddDependency", "ContentProcessorContext.AddOutputFile",
            "ContentProcessorContext.Convert", "ContentProcessorContext.BuildAsset", "ContentProcessorContext.BuildAndLoadAsset", "ContentProcessorContext.OutputFilename", "ContentProcessorContext.OutputDirectory",
            "ContentProcessorContext.IntermediateDirectory", "ContentProcessorContext.BuildConfiguration", "ContentProcessorContext.TargetPlatform", "ContentProcessorContext.TargetProfile",
            "Framework.Content.Pipeline.ContentProcessorContext.Parameters", "Framework.Content.Pipeline.ContentProcessorContext.Logger", "ContentImporterContext.Logger", "ContentBuildLogger.LogMessage", "ContentBuildLogger.LogImportantMessage", "ContentIdentity",
            "ExternalReference", "ExternalReference.Filename", "OpaqueDataDictionary", "PipelineException", "BasicMaterialContent", "MaterialContent.Textures", "Compiler.ContentTypeWriter",
            "ContentWriter.WriteExternalReference", "ContentCompiler.AddTypeWriter", "ContentTypeReader", "ContentTypeReaderManager.AddTypeCreator", "ContentManager.Load", "RegisterXnaImporter",
            "RegisterXnaProcessor", "RegisterXnaXnbOutput", "XnaBridgeProcessorContext", "CNA::Content::Pipeline::ContentProcessorContext", "ContentPipeline", "ContentPipeline.Build", "ContentBuildRequest", "ContentBuildResult",
            "ContentBuildEnvironment", "ContentProcessorParameters", "ContentDependency", "ContentDeploymentFile", "ContentLogMessage", "ContentPipelineRegistry", "PipelineComponentScanner",
            "PipelineComponentScanner.Update", "PipelineComponentScanner.ImporterNames", "PipelineComponentScanner.ProcessorNames", "PipelineComponentScanner.ImporterAttributes",
            "PipelineComponentScanner.ImporterOutputTypes", "PipelineComponentScanner.ProcessorAttributes", "PipelineComponentScanner.ProcessorInputTypes", "PipelineComponentScanner.ProcessorOutputTypes",
            "PipelineComponentScanner.ProcessorParameters", "PipelineComponentScanner.Errors", "ProcessorParameter"},
           ApiOrigin::Xna),
        wk("wk-gltf", "glTF Import, Checked", "Two UV sets, texture transforms, specular and IOR, sparse accessors, morph targets, and extensions required, used and unknown", "gltf",
           {"ContentManager.Load", "Model", "Model.Meshes", "ModelMesh.MeshParts", "ModelMeshPart.VertexBuffer", "ModelMeshPart.VertexOffset", "ModelMeshPart.NumVertices", "ModelMeshPart.Effect", "ModelMeshPart.Tag",
            "VertexBuffer.GetData", "VertexBuffer.VertexDeclaration", "VertexBuffer.VertexCount", "VertexDeclaration.VertexStride", "VertexDeclaration.GetVertexElements", "VertexElement.Offset",
            "VertexElement.VertexElementUsage", "VertexElement.UsageIndex", "VertexElementUsage", "PbrEffect", "PbrEffect.TextureCoordinateSetsEXT", "PbrEffect.TextureTransformsEXT", "PbrEffect.MetallicFactor",
            "PbrEffect.RoughnessFactor", "PbrEffect.IorEXT", "PbrEffect.SpecularFactorEXT", "PbrEffect.SpecularColorFactorEXT", "TextureTransformEXT", "MorphTargetDataEXT", "MorphTargetDataEXT.PositionDeltas",
            "MorphTargetDataEXT.Weights", "MorphTargetDataEXT.BaseVertexBytes", "MorphTargetDataEXT.Stride", "Model.GltfImportReportEXT", "GltfImportReportEXT", "GltfImportReportEXT.Diagnostics",
            "GltfImportReportEXT.MeshInstanceCount", "GltfImportReportEXT.NodeCount", "GltfImportDiagnosticEXT", "GltfImportDiagnosticKindEXT", "ContentLoadException"},
           ApiOrigin::CnaExtension),
        [] {
            // CNA's exported C ABI from a C17 translation unit (src/CApi/NativeCProbe.c).
            RoomSpec r;
            r.key = "cx-capi";
            r.title = "CNA from C";
            r.subtitle = "The native C ABI from C17: every header family, checked in the museum and in a C game of its own started as a separate process";
            r.exhibit = "capi-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "workshop";
            r.apis = {"cna_get_abi_version", "cna_vector3_init_xyz", "cna_vector3_cross", "cna_vector3_dot", "cna_matrix_create_rotation_y", "cna_matrix_create_translation_xyz", "cna_matrix_multiply",
                      "cna_matrix_invert", "cna_vector3_transform_matrix", "cna_quaternion_create_from_yaw_pitch_roll", "cna_quaternion_length", "cna_half_from_single", "cna_curve_create", "cna_curve_get_keys",
                      "cna_curve_key_init_position_value", "cna_curve_key_collection_add", "cna_curve_evaluate", "cna_curve_destroy", "cna_error_get_last_info", "cna_error_copy_last_message", "CNA_ErrorInfo",
                      "CNA_Vector3", "CNA_Matrix", "CNA_Quaternion", "CNA_CurveKey",
                      "cna_color_init_int_rgb", "cna_color_get_packed_value", "cna_color_get_string_byte_count", "cna_color_copy_string", "cna_color_set_packed_value", "cna_color_equals",
                      "cna_color_not_equals", "cna_color_get_hash_code", "cna_color_lerp", "cna_color_from_non_premultiplied_int", "cna_color_multiply", "cna_color_init_float_rgba",
                      "cna_color_to_vector4", "cna_color_init_vector4", "cna_plane_init_points", "cna_plane_dot_coordinate", "cna_plane_copy_string", "cna_plane_get_hash_code",
                      "cna_plane_init_normal_d", "cna_plane_equals", "cna_plane_not_equals", "cna_plane_init_abcd", "cna_plane_normalize", "cna_bounding_box_init_min_max", "cna_bounding_sphere_init_center_radius",
                      "cna_plane_intersects_box", "cna_plane_intersects_sphere", "cna_ray_init_position_direction", "cna_ray_intersects_box", "cna_ray_intersects_sphere", "cna_ray_intersects_plane",
                      "cna_ray_copy_string", "cna_bounding_box_create_from_points", "cna_bounding_box_contains_point", "cna_bounding_box_contains_box", "cna_bounding_box_copy_corners",
                      "cna_bounding_box_create_merged", "cna_bounding_box_copy_string", "cna_bounding_box_get_string_size", "cna_bounding_sphere_create_from_box", "cna_matrix_create_scale_scalar",
                      "cna_bounding_sphere_transform", "cna_bounding_sphere_contains_sphere", "cna_matrix_create_look_at", "cna_matrix_create_perspective_field_of_view", "cna_bounding_frustum_init_matrix",
                      "cna_bounding_frustum_get_near", "cna_bounding_frustum_get_far", "cna_bounding_frustum_copy_corners", "cna_bounding_frustum_contains_point", "cna_bounding_frustum_contains_box",
                      "cna_vertex_type_get_stride", "cna_vertex_type_copy_elements", "cna_vertex_element_copy_string", "cna_vertex_element_equals", "cna_vertex_element_not_equals",
                      "cna_vertex_value_init_default", "cna_vertex_value_copy_string", "cna_vertex_value_equals", "cna_logger_get_minimum_level", "cna_logger_set_sink_ext",
                      "cna_logger_set_minimum_level", "cna_logger_info", "cna_logger_warn_if", "cna_logger_log", "cna_logger_warn", "cna_logger_reset_sink_ext", "cna_platform_get_current",
                      "cna_platform_get_is_apple_ext", "cna_platform_get_is_mobile_ext", "cna_desktop_os_get_current", "cna_platform_get_current_name_size_ext", "cna_platform_copy_current_name_ext",
                      "cna_graphics_renderer_get_category", "cna_graphics_renderer_category_get_name_size", "cna_graphics_renderer_category_copy_name", "cna_graphics_renderer_try_parse_name_ext",
                      "cna_graphics_renderer_get_current_name_size", "cna_graphics_renderer_copy_current_name", "cna_graphics_renderer_get_current_type", "cna_graphics_renderer_get_is_available_ext",
                      "cna_graphics_renderer_get_available_count_ext", "cna_graphics_renderer_copy_available_ext", "cna_graphics_renderer_get_maturity", "cna_graphics_renderer_maturity_copy_name",
                      "cna_assembly_copy_title_ext", "cna_assembly_set_title_ext", "cna_vector3_length", "cna_error_get_last_message_size", "cna_error_get_last_exception_type_size_ext",
                      "cna_error_get_last_exception_param_name_size_ext", "cna_error_copy_last_exception_type_ext", "cna_error_copy_last_exception_param_name_ext", "cna_cnb_crc32c",
                      "cna_cnb_crc32c_portable", "cna_cnb_crc32c_continue", "cna_cnb_crc32c_uses_hardware", "cna_cnb_copy_format_magic", "cna_cnb_has_magic", "cna_cnb_make_chunk_id",
                      "cna_cnb_copy_chunk_id_string", "cna_cnb_is_well_formed_chunk_id", "cna_cnb_checked_add", "cna_cnb_checked_multiply", "cna_cnb_asset_type_id_from_name",
                      "cna_cnb_is_custom_asset_type_id", "cna_cnb_is_compression_supported", "cna_cnb_copy_compression_name", "cna_cnb_get_compressed_byte_count", "cna_cnb_copy_compressed",
                      "cna_cnb_copy_decompressed", "cna_cnb_get_texture_level_byte_size", "cna_cnb_is_block_compressed_texture_format", "cna_cnb_byte_writer_create", "cna_cnb_byte_writer_write_u32",
                      "cna_cnb_byte_writer_write_string", "cna_cnb_byte_writer_get_size", "cna_cnb_byte_writer_copy_bytes", "cna_cnb_byte_writer_destroy", "cna_cnb_reader_create",
                      "cna_cnb_reader_read_u32", "cna_cnb_reader_read_string", "cna_cnb_reader_copy_string", "cna_cnb_reader_require_exhausted", "cna_cnb_reader_destroy", "cna_cnb_writer_create",
                      "cna_cnb_writer_set_metadata", "cna_cnb_writer_add_chunk", "cna_cnb_writer_build", "cna_cnb_writer_destroy", "cna_cnb_document_parse", "cna_cnb_document_get_container_major",
                      "cna_cnb_document_get_asset_type_id", "cna_cnb_document_get_chunk_count", "cna_cnb_document_find_single", "cna_cnb_document_get_chunk", "cna_cnb_document_copy_chunk_data",
                      "cna_cnb_document_copy_metadata_content_name", "cna_cnb_document_destroy", "cna_cnb_encode_curve", "cna_cnb_decode_curve", "cna_content_type_reader_manager_register",
                      "cna_content_type_reader_manager_get_is_registered", "cna_content_type_reader_manager_create_reader", "cna_content_type_reader_get_target_type_name_size",
                      "cna_content_type_reader_copy_target_type_name", "cna_content_type_reader_get_type_version", "cna_content_type_reader_get_can_deserialize_into_existing_object",
                      "cna_content_type_reader_supports_version", "cna_content_type_reader_destroy", "cna_content_type_reader_manager_unregister", "cna_reflective_type_reader_get_canonical_name_size",
                      "cna_reflective_type_reader_copy_canonical_name", "cna_enum_type_reader_get_canonical_name_size", "cna_enum_type_reader_copy_canonical_name", "cna_known_unsupported_content_type_reader_create",
                      "cna_keyboard_state_init_from_keys", "cna_keyboard_state_is_key_down", "cna_keyboard_state_is_key_up", "cna_keyboard_state_get_key_state", "cna_keyboard_state_get_pressed_key_count",
                      "cna_keyboard_state_copy_pressed_keys", "cna_keyboard_state_equals", "cna_keyboard_state_not_equals", "cna_keyboard_state_get_hash_code", "cna_keyboard_state_copy_string",
                      "cna_mouse_state_init_from_values", "cna_mouse_state_get_string_size", "cna_mouse_state_copy_string", "cna_mouse_state_get_hash_code", "cna_mouse_state_equals",
                      "cna_mouse_state_not_equals", "cna_gamepad_buttons_init_from_mask", "cna_gamepad_buttons_is_pressed", "cna_gamepad_thumb_sticks_init_from_positions", "cna_gamepad_triggers_init_from_positions",
                      "cna_gamepad_state_init_from_components", "cna_gamepad_state_get_buttons", "cna_gamepad_state_is_button_down", "cna_gamepad_state_is_button_up", "cna_gamepad_state_copy_string",
                      "cna_gamepad_apply_dead_zone", "cna_touch_location_init_from_values", "cna_touch_location_init_with_previous", "cna_touch_location_try_get_previous", "cna_touch_location_copy_string",
                      "cna_touch_location_get_hash_code", "cna_touch_state_init_from_locations", "cna_touch_state_find_by_id", "cna_joystick_info_init", "cna_joystick_info_equals",
                      "cna_sensor_info_init", "cna_sensor_info_equals", "cna_input_device_info_init", "cna_input_device_info_equals", "cna_haptic_effect_init", "cna_haptic_effect_equals",
                      "cna_haptic_direction_init", "cna_accelerometer_reading_init_from_values", "cna_accelerometer_reading_init", "cna_accelerometer_reading_equals", "cna_accelerometer_reading_get_hash_code",
                      "cna_accelerometer_reading_get_type_name_size", "cna_accelerometer_reading_copy_type_name", "cna_accelerometer_reading_get_string_size", "cna_accelerometer_reading_copy_string",
                      "cna_compass_reading_init_from_values", "cna_compass_reading_equals", "cna_compass_reading_copy_type_name", "cna_environment_get_device_type", "cna_devices_ext_is_available",
                      "cna_camera_device_info_init", "cna_property_dictionary_create_ext", "cna_property_dictionary_set_int32", "cna_property_dictionary_set_string", "cna_property_dictionary_get_count",
                      "cna_property_dictionary_get_int32", "cna_property_dictionary_copy_string", "cna_property_dictionary_try_get_value_kind_ext", "cna_property_dictionary_contains_key",
                      "cna_property_dictionary_get_date_time_ticks", "cna_property_dictionary_destroy", "cna_achievement_create_ext", "cna_achievement_collection_create_ext",
                      "cna_achievement_collection_add", "cna_achievement_collection_get_count", "cna_achievement_collection_get_by_key", "cna_achievement_copy_name", "cna_achievement_get_info",
                      "cna_achievement_destroy", "cna_achievement_collection_destroy", "cna_gamer_presence_init", "cna_leaderboard_identity_init", "cna_avatar_description_create_random",
                      "cna_avatar_description_get_info", "cna_avatar_description_destroy", "cna_packet_writer_create", "cna_packet_writer_write_vector3", "cna_packet_writer_write_color",
                      "cna_packet_writer_write_single", "cna_packet_writer_write_double", "cna_packet_writer_get_length", "cna_packet_writer_copy_data_ext", "cna_packet_writer_destroy",
                      "cna_packet_reader_create", "cna_packet_reader_set_data_ext", "cna_packet_reader_read_vector3", "cna_packet_reader_read_color", "cna_packet_reader_read_single",
                      "cna_packet_reader_read_double", "cna_packet_reader_get_position", "cna_packet_reader_destroy", "cna_network_session_properties_create", "cna_network_session_properties_get_count",
                      "cna_network_session_properties_get_item", "cna_network_session_properties_set_item", "cna_network_session_properties_contains", "cna_network_session_properties_get_is_read_only",
                      "cna_quality_of_service_init_measured", "cna_available_network_session_create_ext", "cna_available_network_session_get_open_public_gamer_slots", "cna_available_network_session_copy_host_gamertag",
                      "cna_available_network_session_destroy", "cna_network_session_properties_destroy", "cna_network_gamer_create", "cna_network_gamer_get_is_host", "cna_network_gamer_set_is_ready",
                      "cna_network_gamer_get_is_ready", "cna_network_gamer_destroy", "cna_game_started_event_info_init", "cna_game_get_graphics_device", "cna_game_run", "cna_game_request_exit",
                      "cna_game_clear", "cna_game_create", "cna_game_subscribe", "cna_graphics_device_get_renderer_info", "cna_graphics_device_supports_capability", "cna_graphics_renderer_get_is_latched_ext",
                      "cna_graphics_renderer_get_active_ext", "cna_graphics_renderer_set_preferred_ext", "cna_game_run_one_frame", "cna_game_components_remove", "cna_game_component_destroy",
                      "cna_graphics_device_manager_destroy", "cna_sprite_batch_destroy", "cna_render_target_destroy", "cna_game_destroy", "cna_game_get_is_active", "cna_game_unsubscribe",
                      "cna_graphics_renderer_get_selected_ext", "cna_graphics_renderer_get_automatic_fallback_ext", "cna_graphics_renderer_get_fallback_count_ext", "cna_game_window_copy_title",
                      "cna_game_set_window_title", "cna_game_window_get_title_size", "cna_game_window_get_client_bounds", "cna_game_copy_type_name", "cna_game_window_copy_type_name",
                      "cna_game_get_target_elapsed_time_ticks", "cna_game_get_is_fixed_time_step", "cna_game_set_target_elapsed_time_ticks", "cna_game_get_target_fps_ext", "cna_game_get_target_ms_frame_time_ext",
                      "cna_game_fps_to_milliseconds_per_frame_ext", "cna_game_launch_parameters_get_count", "cna_game_launch_parameters_add", "cna_game_launch_parameters_contains_key",
                      "cna_game_launch_parameters_copy_value", "cna_game_launch_parameters_copy_key", "cna_title_location_copy_path", "cna_title_container_read_ext", "cna_graphics_device_information_init",
                      "cna_graphics_device_information_clone", "cna_graphics_device_information_copy_type_name", "cna_game_services_contains_ext", "cna_graphics_device_manager_create",
                      "cna_graphics_device_manager_get_graphics_profile", "cna_graphics_device_manager_set_preferred_back_buffer_width", "cna_graphics_device_manager_get_preferred_back_buffer_width",
                      "cna_game_component_callbacks_init", "cna_game_component_create", "cna_game_components_get_count", "cna_game_components_add", "cna_game_components_contains",
                      "cna_game_components_index_of", "cna_game_component_get_is_drawable", "cna_game_component_get_enabled", "cna_game_component_set_update_order", "cna_game_component_get_update_order",
                      "cna_game_component_copy_type_name", "cna_graphics_device_set_render_target2d", "cna_texture2d_get_data_rgba8", "cna_graphics_device_get_renderer_name_size",
                      "cna_graphics_device_copy_renderer_name", "cna_graphics_device_get_shader_dialect_ext", "cna_graphics_device_get_capability_report_size_ext", "cna_graphics_device_copy_capability_report_ext",
                      "cna_graphics_device_get_backbuffer_info", "cna_graphics_device_get_graphics_profile", "cna_graphics_device_copy_type_name", "cna_viewport_init_bounds",
                      "cna_viewport_get_aspect_ratio", "cna_viewport_project", "cna_viewport_unproject", "cna_viewport_copy_string", "cna_viewport_get_title_safe_area", "cna_blend_state_init",
                      "cna_graphics_device_set_blend_state", "cna_graphics_device_get_blend_state", "cna_sampler_state_init", "cna_graphics_device_set_sampler_state", "cna_graphics_device_get_sampler_state",
                      "cna_rasterizer_state_init", "cna_graphics_device_set_rasterizer_state", "cna_graphics_device_get_rasterizer_state", "cna_depth_stencil_state_init", "cna_graphics_device_set_depth_stencil_state",
                      "cna_graphics_device_get_depth_stencil_state", "cna_graphics_device_set_scissor_rectangle", "cna_graphics_device_get_scissor_rectangle", "cna_graphics_device_set_blend_factor",
                      "cna_graphics_device_get_blend_factor", "cna_graphics_device_set_reference_stencil", "cna_graphics_device_get_reference_stencil", "cna_graphics_device_set_multi_sample_mask",
                      "cna_graphics_device_get_multi_sample_mask", "cna_render_target2d_create", "cna_render_target_get_info", "cna_render_target_usage_preserves_contents",
                      "cna_graphics_device_get_render_target_count", "cna_graphics_device_copy_render_targets", "cna_graphics_device_set_viewport", "cna_graphics_device_get_viewport",
                      "cna_graphics_device_clear_options", "cna_basic_effect_create", "cna_basic_effect_set_vertex_color_enabled", "cna_effect_matrices_set_world", "cna_effect_matrices_set_view",
                      "cna_effect_matrices_set_projection", "cna_effect_apply", "cna_graphics_device_draw_user_primitives", "cna_primitive_type_get_vertex_count", "cna_vertex_declaration_create_with_stride",
                      "cna_vertex_declaration_get_stride", "cna_vertex_buffer_create", "cna_vertex_buffer_set_data", "cna_vertex_buffer_get_data", "cna_vertex_buffer_get_info",
                      "cna_index_buffer_create", "cna_index_buffer_set_data", "cna_index_buffer_get_data", "cna_graphics_device_set_vertex_buffer", "cna_graphics_device_set_index_buffer",
                      "cna_graphics_device_get_index_buffer", "cna_graphics_device_draw_indexed_primitives", "cna_vertex_buffer_destroy", "cna_index_buffer_destroy", "cna_vertex_declaration_destroy",
                      "cna_effect_destroy", "cna_texture2d_create_from_rgba8", "cna_sprite_batch_create", "cna_sprite_batch_begin", "cna_sprite_batch_submit_many", "cna_sprite_batch_end",
                      "cna_sprite_batch_copy_type_name", "cna_texture2d_destroy", "cna_texture2d_create", "cna_texture2d_set_data_rgba8", "cna_texture2d_get_info", "cna_texture_get_format_size",
                      "cna_texture2d_copy_type_name", "cna_texture2d_get_encoded_byte_count", "cna_texture2d_copy_encoded", "cna_texture2d_create_from_encoded_memory", "cna_graphics_resource_set_name",
                      "cna_graphics_resource_copy_name", "cna_graphics_resource_copy_string", "cna_graphics_resource_set_tag", "cna_graphics_resource_get_tag", "cna_graphics_resource_get_graphics_device",
                      "cna_graphics_resource_subscribe_disposing", "cna_graphics_resource_get_is_disposed", "cna_graphics_resource_dispose", "cna_graphics_resource_unsubscribe_disposing",
                      "cna_texture3d_create", "cna_texture3d_set_data", "cna_texture3d_get_data", "cna_texture3d_get_info", "cna_texture3d_copy_type_name", "cna_texture3d_destroy",
                      "cna_texturecube_create", "cna_texturecube_set_data", "cna_texturecube_get_data", "cna_texturecube_copy_type_name", "cna_texturecube_destroy", "cna_display_mode_init",
                      "cna_display_mode_equals", "cna_graphics_adapter_get_count", "cna_graphics_adapter_get_info", "cna_graphics_adapter_copy_description", "cna_graphics_adapter_get_current_display_mode",
                      "cna_graphics_adapter_is_profile_supported", "cna_presentation_parameters_init", "cna_graphics_device_get_presentation_parameters", "cna_presentation_parameters_get_bounds",
                      "cna_presentation_parameters_clone", "cna_graphics_ext_is_available", "cna_debug_draw_create", "cna_debug_draw_begin", "cna_debug_draw_add_line", "cna_debug_draw_set_depth_tested",
                      "cna_debug_draw_add_box", "cna_debug_draw_get_line_count", "cna_debug_draw_copy_vertices", "cna_debug_draw_clear", "cna_debug_draw_destroy", "cna_crt_effect_create",
                      "cna_crt_effect_set_scanline_intensity", "cna_crt_effect_get_scanline_intensity", "cna_image_based_light_ext_init", "cna_image_based_light_ext_is_valid",
                      "cna_basic_effect_get_diffuse_color", "cna_basic_effect_get_specular_color", "cna_basic_effect_get_emissive_color", "cna_basic_effect_get_specular_power",
                      "cna_basic_effect_get_alpha", "cna_effect_fog_get_end", "cna_effect_lights_get_enabled", "cna_effect_lights_enable_default", "cna_effect_lights_get_ambient_color",
                      "cna_effect_lights_get_directional_light", "cna_directional_light_get_direction", "cna_directional_light_get_diffuse_color", "cna_directional_light_destroy",
                      "cna_effect_get_parameters", "cna_effect_parameter_collection_get_count", "cna_effect_parameter_collection_find_name", "cna_effect_parameter_get_info",
                      "cna_effect_parameter_destroy", "cna_effect_parameter_collection_destroy", "cna_effect_get_techniques", "cna_effect_technique_collection_get_count", "cna_effect_technique_collection_destroy",
                      "cna_effect_get_current_technique", "cna_effect_technique_copy_name", "cna_effect_technique_destroy", "cna_effect_copy_type_name", "cna_basic_effect_set_diffuse_color",
                      "cna_effect_clone", "cna_alpha_test_effect_create", "cna_environment_map_effect_create", "cna_skinned_effect_create", "cna_dual_texture_effect_create",
                      "cna_alpha_test_effect_get_alpha_function", "cna_alpha_test_effect_get_reference_alpha", "cna_environment_map_effect_get_fresnel_factor", "cna_environment_map_effect_get_amount",
                      "cna_skinned_effect_get_weights_per_vertex", "cna_skinned_effect_get_specular_power", "cna_dual_texture_effect_get_diffuse_color", "cna_skinned_effect_set_weights_per_vertex",
                      "cna_model_bone_create", "cna_model_bone_set_transform", "cna_model_bone_add_child", "cna_model_create", "cna_model_copy_absolute_bone_transforms", "cna_model_get_bones",
                      "cna_model_bone_collection_get_count", "cna_model_bone_collection_find", "cna_model_bone_get_parent", "cna_model_bone_copy_name", "cna_model_bone_get_index",
                      "cna_model_bone_destroy", "cna_model_bone_collection_destroy", "cna_model_destroy", "cna_content_manager_load_model", "cna_model_get_meshes", "cna_model_mesh_collection_get_count",
                      "cna_model_mesh_collection_get_at", "cna_model_mesh_get_bounding_sphere", "cna_model_get_root", "cna_model_mesh_destroy", "cna_model_mesh_collection_destroy",
                      "cna_model_draw", "cna_sprite_font_create", "cna_sprite_font_measure_utf8", "cna_sprite_font_set_default_character", "cna_sprite_font_set_line_spacing",
                      "cna_sprite_font_set_spacing", "cna_sprite_font_get_info", "cna_sprite_font_copy_characters", "cna_sprite_font_copy_glyphs", "cna_sprite_font_destroy",
                      "cna_content_manager_create", "cna_content_manager_copy_root_directory", "cna_content_manager_load_texture2d", "cna_content_manager_copy_normalized_key",
                      "cna_content_manager_get_manifest_entry_count", "cna_game_get_content_manager_ext", "cna_content_manager_destroy", "cna_content_manager_get_has_service_provider",
                      "cna_sound_effect_get_sample_duration_ticks", "cna_sound_effect_get_sample_size_in_bytes", "cna_audio_emitter_init", "cna_audio_listener_init", "cna_audio_get_capabilities",
                      "cna_sound_effect_create_pcm16", "cna_sound_effect_get_duration_ticks", "cna_sound_effect_create_instance", "cna_sound_effect_instance_get_info", "cna_sound_effect_instance_set_volume",
                      "cna_sound_effect_instance_set_pan", "cna_sound_effect_instance_set_is_looped", "cna_sound_effect_instance_play", "cna_sound_effect_instance_stop", "cna_sound_effect_destroy",
                      "cna_sound_effect_instance_destroy", "cna_sound_effect_get_master_volume", "cna_content_manager_load_sound_effect", "cna_sound_effect_copy_type_name",
                      "cna_audio_engine_create", "cna_audio_engine_get_global_variable", "cna_audio_engine_set_global_variable", "cna_audio_engine_get_category", "cna_audio_category_copy_name",
                      "cna_audio_category_destroy", "cna_wave_bank_create", "cna_sound_bank_create", "cna_sound_bank_get_cue", "cna_cue_copy_name", "cna_cue_get_variable", "cna_cue_set_variable",
                      "cna_cue_get_info", "cna_cue_play", "cna_cue_stop", "cna_cue_destroy", "cna_sound_bank_destroy", "cna_wave_bank_destroy", "cna_audio_engine_destroy", "cna_song_create_with_duration",
                      "cna_song_create", "cna_song_get_duration", "cna_song_copy_name", "cna_song_equals", "cna_media_player_get_game_has_control", "cna_media_player_set_volume",
                      "cna_media_player_get_volume", "cna_media_player_get_state", "cna_media_player_set_is_repeating", "cna_media_player_get_is_repeating", "cna_media_player_play_song",
                      "cna_media_player_get_queue", "cna_media_queue_get_count", "cna_media_queue_destroy", "cna_media_player_stop", "cna_song_destroy", "cna_video_create_with_metadata",
                      "cna_video_get_width", "cna_video_get_height", "cna_video_get_frames_per_second", "cna_video_get_duration", "cna_video_copy_type_name", "cna_video_player_create",
                      "cna_video_player_get_state", "cna_video_player_get_video", "cna_video_player_get_volume", "cna_video_player_copy_type_name", "cna_video_player_destroy",
                      "cna_video_destroy", "cna_video_info_init", "cna_media_library_create", "cna_media_library_get_songs", "cna_media_library_get_albums", "cna_media_library_get_artists",
                      "cna_media_library_get_genres", "cna_media_library_get_playlists", "cna_media_library_get_pictures", "cna_song_collection_get_count", "cna_album_collection_get_count",
                      "cna_artist_collection_get_count", "cna_genre_collection_get_count", "cna_playlist_collection_get_count", "cna_picture_collection_get_count", "cna_media_library_copy_type_name",
                      "cna_media_library_get_is_disposed", "cna_media_library_dispose", "cna_picture_collection_destroy", "cna_playlist_collection_destroy", "cna_genre_collection_destroy",
                      "cna_artist_collection_destroy", "cna_album_collection_destroy", "cna_song_collection_destroy", "cna_media_library_destroy", "cna_keyboard_get_state",
                      "cna_mouse_get_state", "cna_gamepad_get_state", "cna_gamepad_get_capabilities", "cna_gamepad_set_vibration", "cna_touch_get_state", "cna_touch_get_capabilities",
                      "cna_keyboard_copy_key_name_ext", "cna_keyboard_get_key_from_name_ext", "cna_touch_panel_set_enabled_gestures", "cna_touch_panel_get_enabled_gestures",
                      "cna_touch_panel_set_display_width", "cna_touch_panel_get_display_width", "cna_touch_panel_get_is_gesture_available", "cna_touch_panel_enqueue_gesture_ext",
                      "cna_touch_panel_read_gesture", "cna_mouse_subscribe_clicked_ext", "cna_joysticks_subscribe_connected_ext", "cna_input_devices_subscribe_mouse_connected_ext",
                      "cna_text_input_subscribe_text_input_ext", "cna_mouse_raise_clicked_ext", "cna_joysticks_raise_connected_ext", "cna_input_devices_raise_mouse_connected_ext",
                      "cna_text_input_raise_text_input_ext", "cna_mouse_unsubscribe_clicked_ext", "cna_joysticks_unsubscribe_ext", "cna_input_devices_unsubscribe_ext", "cna_text_input_unsubscribe_ext",
                      "cna_mouse_cursor_get_stock_ext", "cna_mouse_cursor_create_from_texture2d", "cna_mouse_set_cursor_ext", "cna_mouse_cursor_dispose", "cna_mouse_cursor_destroy",
                      "cna_haptics_open_from_mouse", "cna_haptic_device_get_is_open", "cna_haptic_device_play_rumble", "cna_haptic_device_destroy", "cna_haptics_get_count",
                      "cna_joysticks_get_count", "cna_input_devices_get_mouse_count", "cna_input_devices_get_keyboard_count", "cna_clipboard_get_has_text", "cna_clipboard_copy_text",
                      "cna_clipboard_set_text", "cna_clipboard_get_text_size", "cna_vibrate_controller_set_test_backend_ext", "cna_vibrate_controller_get_is_supported_ext",
                      "cna_vibrate_controller_copy_device_name_ext", "cna_vibrate_controller_start", "cna_vibrate_controller_stop", "cna_vibrate_controller_get_test_log_ext",
                      "cna_message_box_set_test_backend_ext", "cna_message_box_show_ext", "cna_message_box_get_test_log_ext", "cna_file_dialog_set_test_backend_ext", "cna_file_dialog_show_open_file_ext",
                      "cna_framework_dispatcher_update", "cna_system_tray_create_with_test_backend_ext", "cna_system_tray_add_entry", "cna_system_tray_set_entry_checked", "cna_system_tray_get_entry_checked",
                      "cna_system_tray_set_entry_enabled", "cna_system_tray_get_entry_enabled", "cna_system_tray_click_entry_for_tests_ext", "cna_system_tray_destroy", "cna_camera_create_with_test_backend_ext",
                      "cna_camera_get_state_ext", "cna_camera_set_test_frame_ext", "cna_camera_get_frame_width_ext", "cna_camera_get_frame_height_ext", "cna_camera_try_acquire_frame_ext",
                      "cna_camera_destroy", "cna_system_info_get_logical_cpu_core_count_ext", "cna_system_info_get_system_ram_megabytes_ext", "cna_display_info_get_content_scale_ext",
                      "cna_power_get_info", "cna_locale_get_preferred_count_ext", "cna_accelerometer_get_is_supported", "cna_accelerometer_create", "cna_accelerometer_get_current_value",
                      "cna_accelerometer_set_supported_for_tests_ext", "cna_accelerometer_start", "cna_sensors_get_last_error_id_ext", "cna_accelerometer_set_started_for_tests_ext",
                      "cna_accelerometer_inject_synthetic_update_ext", "cna_accelerometer_get_is_data_valid", "cna_accelerometer_destroy", "cna_compass_create", "cna_compass_set_test_backend_ext",
                      "cna_compass_start", "cna_compass_inject_synthetic_update_ext", "cna_compass_get_current_value", "cna_compass_stop", "cna_compass_destroy", "cna_storage_device_show_selector",
                      "cna_storage_device_get_is_connected", "cna_storage_device_get_free_space", "cna_storage_device_get_total_space", "cna_storage_container_open", "cna_storage_container_copy_display_name",
                      "cna_storage_container_copy_type_name", "cna_storage_container_create_directory", "cna_storage_container_directory_exists", "cna_storage_device_destroy",
                      "cna_storage_container_create_file", "cna_storage_stream_write", "cna_storage_stream_get_length", "cna_storage_stream_get_can_write", "cna_storage_container_destroy",
                      "cna_storage_stream_flush", "cna_storage_stream_close", "cna_storage_container_open_file_access", "cna_storage_stream_read", "cna_storage_stream_seek",
                      "cna_content_reader_create", "cna_content_reader_copy_asset_name", "cna_content_reader_get_version", "cna_content_reader_get_platform", "cna_content_reader_get_content_manager",
                      "cna_content_reader_initialize_type_readers", "cna_content_reader_read_object_tag", "cna_content_reader_read_shared_resources", "cna_content_reader_read_vector3",
                      "cna_content_reader_read_color", "cna_content_reader_read_vector2", "cna_content_reader_check_collection_element_count", "cna_content_reader_destroy",
                      "cna_storage_container_get_file_name_count", "cna_storage_container_copy_file_name", "cna_storage_container_delete_file", "cna_storage_container_file_exists",
                      "cna_storage_container_delete_directory", "cna_storage_device_delete_container", "cna_storage_copy_root_ext", "cna_guide_get_is_trial_mode", "cna_gamer_services_dispatcher_get_is_initialized",
                      "cna_guide_get_is_visible", "cna_gamer_services_dispatcher_initialize", "cna_guide_set_simulate_trial_mode", "cna_gamer_services_dispatcher_update", "cna_guide_get_simulate_trial_mode",
                      "cna_guide_begin_show_keyboard_input", "cna_guide_get_has_pending_keyboard_input_ext", "cna_guide_simulate_keyboard_input_cancel_ext", "cna_guide_was_keyboard_input_canceled_ext",
                      "cna_guide_reset_pending_keyboard_input_ext", "cna_signed_in_gamer_create_ext", "cna_gamer_set_signed_in_gamers_ext", "cna_gamer_get_signed_in_gamer_count",
                      "cna_gamer_get_signed_in_gamer_at_player_index", "cna_gamer_copy_gamertag", "cna_signed_in_gamer_destroy", "cna_network_session_create_with_local_gamers",
                      "cna_network_session_get_session_state", "cna_network_session_get_is_host", "cna_network_session_get_gamer_count", "cna_network_session_get_max_gamers",
                      "cna_network_session_create", "cna_network_session_start_game", "cna_network_session_update", "cna_network_session_destroy", "CNA_COLOR_RGBA", "CNA_COLOR_TRANSPARENT",
                      "CNA_COLOR_BLACK", "CNA_COLOR_WHITE", "CNA_COLOR_RED", "CNA_COLOR_CORNFLOWER_BLUE", "CNA_Vector4", "CNA_Plane", "CNA_Ray", "CNA_BoundingBox", "CNA_BoundingSphere",
                      "CNA_BoundingFrustum", "CNA_ContainmentType", "CNA_PlaneIntersectionType", "CNA_CONTAINMENT_DISJOINT", "CNA_CONTAINMENT_CONTAINS", "CNA_CONTAINMENT_INTERSECTS",
                      "CNA_PLANE_INTERSECTION_FRONT", "CNA_PLANE_INTERSECTION_BACK", "CNA_PLANE_INTERSECTION_INTERSECTING", "CNA_VertexElement", "CNA_VERTEX_ELEMENT_FORMAT_VECTOR3",
                      "CNA_VERTEX_ELEMENT_FORMAT_COLOR", "CNA_VERTEX_ELEMENT_FORMAT_VECTOR2", "CNA_VERTEX_ELEMENT_USAGE_POSITION", "CNA_VERTEX_ELEMENT_USAGE_COLOR", "CNA_VERTEX_ELEMENT_USAGE_TEXTURE_COORDINATE",
                      "CNA_PRIMITIVE_TRIANGLE_LIST", "CNA_INDEX_ELEMENT_SIZE_SIXTEEN_BITS", "CNA_BUFFER_USAGE_NONE", "CNA_BUFFER_USAGE_WRITE_ONLY",
                      "cna_math_lerp", "cna_math_smooth_step", "cna_math_catmull_rom", "cna_math_hermite", "cna_math_barycentric", "cna_math_to_degrees", "cna_math_to_radians", "cna_math_wrap_angle", "cna_math_clamp_float", "cna_math_distance", "cna_math_clamp_int32", "cna_math_closest_msaa_power", "cna_math_min", "cna_math_max", "cna_point_init_xy", "cna_point_add", "cna_point_get_hash_code", "cna_point_copy_string", "cna_rectangle_init_xywh", "cna_rectangle_get_right", "cna_rectangle_get_bottom", "cna_rectangle_get_center", "cna_rectangle_contains_xy", "cna_rectangle_intersects", "cna_rectangle_intersect", "cna_rectangle_union", "cna_rectangle_get_hash_code", "cna_rectangle_copy_string", "cna_rectangle_inflate"};
            r.origin = ApiOrigin::CnaExtension;
            return r;
        }(),
        // Appended after cx-capi so that no existing room moves (C06, C01): CNA's build engine, XNA's build tasks, imported animation.
        wk("wk-routes", "Build Routes, Built and Loaded", "Every CNA build route on a tiny source (.cnj, .wav, .mp3, .wmv, .gltf, .fxb, .xnb, .xml): built, loaded back and compared, and the refusals", "routes",
           {"RegisterCnjContentPipeline", "CnjImporter", "CurveProcessor", "CurveContentWriter", "RegisterSoundEffectContentPipeline", "CNA::Content::Pipeline::WavImporter",
            "CNA::Content::Pipeline::SoundEffectProcessor", "SoundEffectContentWriter", "MakeProcessedSoundEffect", "ToCnbSoundEffect", "ProcessedSoundEffect", "ProcessedPcmEncoding",
            "ProcessedPcmSampleBytes", "ProcessedPcmBitsPerSample", "ProcessedPcmEncodingName", "ProcessedFrameBytes", "RegisterSongContentPipeline", "SongImporter",
            "CNA::Content::Pipeline::SongProcessor", "SongContentWriter", "SongNameParameter", "RegisterVideoContentPipeline", "CNA::Content::Pipeline::VideoImporter",
            "CNA::Content::Pipeline::VideoProcessor", "CNA::Content::Pipeline::VideoProcessor.ValidateParameters", "VideoContentWriter", "VideoWidthParameter", "VideoHeightParameter",
            "VideoFramesPerSecondParameter", "RegisterModelContentPipeline", "GltfImporter", "CNA::Content::Pipeline::ModelProcessor", "CNA::Content::Pipeline::ModelProcessor.ValidateParameters",
            "ModelContentWriter", "ModelGenerateChildAssetsParameter", "RegisterCompiledEffectContentPipeline", "IsCompiledEffectBinary", "CompiledEffectImporter", "CompiledEffectProcessor",
            "CompiledEffectHasNoCnbSchemaReason", "RegisterXnbContentPipeline", "XnbImporter", "XnbVideoProcessor", "XnbVideoProcessor.InputType", "ImportedXnbVideoType", "ProcessedVideoType",
            "RegisterXnaXmlSourceContentPipeline", "ContentPipeline.Build", "ContentPipelineRegistry.HasImporterForSource", "ContentPipelineError", "ContentPipelineError.Stage",
            "ContentPipelineStageName", "ContentOutputFormatExtension", "ContentManager.Load", "BuildTimeMedia::MakeSongDurationProbe", "BuildTimeMedia::MakeVideoMetadataProbe",
            "BuildTimeMedia::MakeCompressedSoundDecoder", "ImportWavAsImportedSound", "CnbDocument.Parse", "RegisterTexture2DContentPipeline", "RegisterXnbOutputContentPipeline",
            "MakeBlockCompressionTextureEncoder", "Curve.Evaluate", "Video.Width", "Song.Duration", "Model.Meshes"},
           ApiOrigin::CnaExtension),
        wk("wk-incremental", "Incremental Builds", "Build configuration, the manifest and its fingerprints, a changed and a missing dependency, CompileCnjToCnb, headless imports and chunk compression", "incremental",
           {"ContentBuildConfiguration", "ContentBuildConfiguration.Parse", "ContentBuildConfiguration.Find", "ContentBuildConfiguration.Entries", "ContentBuildConfiguration.OutputFormat",
            "ContentBuildConfiguration.SourceRoots", "ContentAssetBuildConfiguration", "ContentBuildConfigurationVersion", "ContentBuildConfigurationFileName", "ContentBuildManifest",
            "ContentBuildManifest.Parse", "ContentBuildManifest.Serialize", "ContentBuildManifest.Find", "ContentBuildManifest.Set", "ContentBuildManifest.Clear", "ContentBuildManifest.Entries",
            "ContentBuildManifestEntry", "ContentBuildManifestOutput", "ContentBuildFingerprintState", "ContentBuildManifestVersion", "ContentBuildManifestFileName", "ContentSha256",
            "ContentFileSha256", "ComputeContentBuildDirectFingerprint", "RefreshContentBuildDirectFingerprint", "ComputeContentBuildEffectiveFingerprint", "RefreshContentBuildEffectiveFingerprint",
            "ComputeContentBuildFingerprint", "MakeContentBuildManifestEntry", "CompileCnjToCnb", "CnjToCnbResult", "ImportImageAsCnbTexture2D", "CnbImageImportOptions", "DecodeWavAsImportedSound",
            "DecodeWavAsCnbSoundEffect", "ProcessImportedSoundEffect", "SoundEffectLoopPolicy", "ImportWavAsCnbSoundEffect", "ImportWavAsImportedSound", "IsCnbCompressionSupported",
            "CnbCompressionToString", "CompressCnbChunk", "DecompressCnbChunk", "CnbCompression", "CnbDocument.Metadata", "ContentProcessorParameters.Set", "ContentProcessorParameters.Find",
            "ContentDependency"},
           ApiOrigin::CnaExtension),
        wk("wk-tasks", "XNA's Build Tasks", "TaskItem, a game's own ContentTask, BuildXact's validation, XNA component names and the compiler's build-tool options", "tasks",
           {"TaskItem", "TaskItem.ItemSpec", "TaskItem.GetMetadata", "TaskItem.SetMetadata", "TaskItem.HasMetadata", "TaskItem.MetadataNames", "Tasks::ContentTask", "Tasks::ContentTask.Execute",
            "Tasks::ContentTask.SetLoggerEXT", "Tasks::ContentTask.MessagesEXT", "Tasks::ContentTask.ErrorsEXT", "Tasks::ContentTask.WarningsEXT", "Tasks::ContentTask.LogMessage",
            "Tasks::ContentTask.LogError", "Tasks::ContentTask.LogWarning", "BuildXact", "BuildXact.Execute", "BuildXact.XnaTypeName", "BuildXact.XactProjects", "BuildXact.TargetPlatform",
            "BuildXact.OutputXactFiles", "BuildXact.RebuiltXactFiles", "BuildXact.IntermediateFiles", "BuildXact.HasXactCompilerEXT", "BuildXact.RebuildAll", "XnaComponentMapping",
            "MapXnaImporterName", "MapXnaProcessorName", "KnownXnaImporterNames", "KnownXnaProcessorNames", "ContentCompilerOptions", "ContentCompilerOptions.buildConfiguration",
            "ContentPipelineRegistryFactory", "ContentPipelineRegistry.DocumentAbsentWriter", "ContentPipelineRegistry.AbsentWriterReason", "ContentBuildLogger.LogMessage"}),
        wk("wk-animation", "Imported Animation", "AnimationContent from a small .x file, AnimationChannel's ordering rules, bone weights, index and position collections", "animation",
           {"AnimationContent", "AnimationContent.Channels", "AnimationContent.Duration", "AnimationChannel", "AnimationChannel.Add", "AnimationChannel.Contains", "AnimationChannel.IndexOf",
            "AnimationChannel.Remove", "AnimationChannel.RemoveAt", "AnimationChannel.Clear", "AnimationChannel.Count", "AnimationKeyframe", "AnimationKeyframe.Time", "AnimationKeyframe.Transform",
            "AnimationKeyframe.CompareTo", "AnimationChannelDictionary", "AnimationContentDictionary", "NodeContent.Animations", "XImporter.Import", "BoneWeight", "BoneWeight.BoneName",
            "BoneWeight.Weight", "BoneWeightCollection", "BoneWeightCollection.NormalizeWeights", "IndexCollection", "IndexCollection.AddRange", "PositionCollection", "InvalidContentException"}),
        wk("wk-contentproject", "A Content Project, Built", "An XNA .contentproj built by BuildContent: a null rebuild, one changed source, RebuildAll, GetLastOutputs, CleanContent, and cna-content's coordinator in the game", "contentproject",
           {"Tasks::ContentProject", "ContentProject.Load", "ContentProject.Parse", "ContentProject.DirectoryEXT", "ContentProject.Property", "ContentProject.Items", "ContentProject.SourceAssets",
            "ContentProject.CopiedFiles", "ContentProject.Item.Get", "ContentProject.ToBuildContentEXT", "ContentProject.UnroutableEXT", "BuildContent", "BuildContent.Execute", "BuildContent.CancelEventNameFormat",
            "BuildContent.OutputContentFiles", "BuildContent.RebuiltContentFiles", "BuildContent.IntermediateFiles", "BuildContent.RebuildAll", "BuildContent.SourceAssets", "BuildContent.PipelineAssemblies",
            "BuildContent.RootDirectory", "BuildContent.OutputDirectory", "BuildContent.IntermediateDirectory", "BuildContent.TargetPlatform", "BuildContent.TargetProfile", "BuildContent.BuildConfiguration",
            "BuildContent.CompressContent", "BuildContent.ContentProjectGUID", "CleanContent", "CleanContent.Execute", "CleanContent.XnaTypeName", "CleanContent.OutputDirectory", "CleanContent.IntermediateDirectory",
            "CleanContent.RootDirectory", "CleanContent.TargetPlatform", "CleanContent.TargetProfile", "CleanContent.BuildConfiguration", "CleanContent.ContentProjectGUID", "GetLastOutputs", "GetLastOutputs.Execute",
            "GetLastOutputs.OutputContentFiles", "GetLastOutputs.setOutputDirectoryEXT", "GetLastOutputs.IntermediateDirectory", "RunContentCompiler", "RegisterBuiltInContentPipeline", "ContentPipelineRegistryFactory",
            "ContentBuildManifest.Parse", "ContentFileSha256", "InvalidContentException", "ContentManager.Load"}),
    };
    return w;
}

FloorSpec MakeFloor8()
{
    FloorSpec f;
    f.index = 8;
    f.name = "Content, Platform and CNA";
    f.tagline = "How assets arrive, what the platform reports, and what CNA adds to XNA";
    f.theme = "concourse";
    f.accent = {0.35f, 0.80f, 0.55f};
    f.wings.push_back(ContentWing());
    f.wings.push_back(CnaWing());
    f.wings.push_back(PlatformWing());
    f.wings.push_back(PlayersWing());
    f.wings.push_back(OnlineWing());
    f.wings.push_back(HistoryWing());
    f.wings.push_back(WorkshopWing());
    return f;
}
