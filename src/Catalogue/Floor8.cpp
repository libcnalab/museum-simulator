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
        ct("ct-storage", "Title Files and Saved Data", "TitleContainer.OpenStream, StorageDevice and StorageContainer", "storage", {"TitleContainer.OpenStream", "StorageDevice", "StorageDevice.BeginShowSelector", "StorageDevice.EndShowSelector", "StorageDevice.BeginOpenContainer", "StorageDevice.EndOpenContainer", "StorageDevice.IsConnected", "StorageDevice.DeviceChanged", "StorageDevice.SetAppNameEXT", "StorageContainer", "StorageContainer.DisplayName", "StorageContainer.CreateFile", "StorageContainer.OpenFile", "StorageContainer.FileExists", "StorageContainer.DeleteFile", "StorageContainer.CreateDirectory", "StorageContainer.DeleteDirectory", "StorageContainer.DirectoryExists", "StorageContainer.GetFileNames", "StorageContainer.GetDirectoryNames", "StorageDevice.GetStorageRootEXT", "StorageDevice.DeleteContainer", "StorageDevice.TotalSpace", "StorageDevice.FreeSpace"}),
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
            RoomSpec r = pl("pl-log", "CNA's Log", "CNA::Logger: seven levels, nine categories, a minimum level, conditional lines and a sink that receives what the engine itself writes", "", {"CNA::Logger", "Logger.Log", "Logger.InfoIf", "Logger.WarnIf", "Logger.SetMinimumLevel", "Logger.GetMinimumLevel", "Logger.SetSink", "Logger.ResetSink", "CNA::LogLevel", "CNA::LogCategory"}, ApiOrigin::CnaExtension);
            r.exhibit = "log-lab";
            r.params = {};
            return r;
        }(),
        [&] {
            RoomSpec r = pl("pl-diagnostics", "Measuring Your Own Code", "CNA::Diagnostics for the program's own counters, gauges, zones, markers, a custom resource and a per-frame source, with a recorded trace written as Chrome Trace JSON", "",
                            {"CNA::Diagnostics::CounterHandle", "CNA::Diagnostics::GaugeHandle", "CNA::Diagnostics::FrameCounterHandle", "CNA::Diagnostics::NameHandle", "CNA::Diagnostics::ZoneScope", "CNA::Diagnostics::MarkEvent",
                             "CNA::Diagnostics::ResourceHandle", "CNA::Diagnostics::ResourceDescriptor", "CNA::Diagnostics::IDiagnosticsSource", "CNA::Diagnostics::RegisterSource", "CNA::Diagnostics::SourceRegistration", "CNA::Diagnostics::FrameStatisticsSink",
                             "CNA::Diagnostics::GetProvider", "IDiagnosticsProvider.ReadEvents", "IDiagnosticsProvider.ResolveName", "CNA::Diagnostics::SetRuntimeMode", "CNA::Diagnostics::StartRecording", "CNA::Diagnostics::StopRecording",
                             "CNA::Diagnostics::Trace", "Trace.WriteChromeTrace", "Trace.WriteBinary", "Trace.ReadBinary", "CNA::Diagnostics::EventRecord", "CNA::Diagnostics::EventBatch", "CNA::Diagnostics::MetricKind", "CNA::Diagnostics::ResourceKind"},
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
              "LocalNetworkGamer.SendData", "LocalNetworkGamer.ReceiveData", "SendDataOptions", "NetworkGamer.IsReady", "NetworkSession.GamerJoined", "NetworkSession.GameStarted", "NetworkSession.BeginFind", "NetworkSession.EndFind", "AvailableNetworkSession", "AvailableNetworkSession.CurrentGamerCount", "AvailableNetworkSession.HostGamertag", "AvailableNetworkSession.OpenPrivateGamerSlots", "AvailableNetworkSession.OpenPublicGamerSlots", "AvailableNetworkSession.QualityOfService", "AvailableNetworkSession.SessionProperties", "AvailableNetworkSessionCollection", "AvailableNetworkSessionCollection.IsDisposed", "QualityOfService", "NetworkSessionProperties", "NetworkSession.SessionProperties", "NetworkSession.AllowJoinInProgress", "NetworkSession.BeginJoin", "NetworkSession.EndJoin", "NetworkGamer.IsGuest", "NetworkGamer.IsLocal", "NetworkGamer.IsHost", "NetworkGamer.Machine", "NetworkGamer.RoundtripTime", "NetworkGamer.Session", "NetworkMachine", "NetworkMachine.Gamers", "NetworkMachine.RemoveFromSession", "NetworkSession.AllGamers", "NetworkSessionJoinException", "NetworkSessionJoinError", "NetworkSessionEndReason", "NetworkSessionEndedEventArgs", "NetworkSessionEndedEventArgs.EndReason", "HostChangedEventArgs", "HostChangedEventArgs.NewHost", "HostChangedEventArgs.OldHost", "NetworkSession.HostChanged", "NetworkSession.SessionEnded", "NetworkSession.InviteAccepted", "NetworkSession.BeginJoinInvited", "NetworkSession.EndJoinInvited", "InviteAcceptedEventArgs", "InviteAcceptedEventArgs.Gamer", "InviteAcceptedEventArgs.IsCurrentSession", "NetworkSessionType.PlayerMatch"}),
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
            "ContentManager.Load", "ContentManager.OpenStream", "ContentLoadException"}),
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
            "ContentSerializerAttribute.ElementName", "InvalidContentException"},
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
            "ContentBuildEnvironment", "ContentProcessorParameters", "ContentDependency", "ContentDeploymentFile", "ContentLogMessage", "ContentPipelineRegistry"},
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
            r.subtitle = "The native C ABI used from a C17 file: version, result codes, thread-local errors, value types and a handle's lifetime";
            r.exhibit = "capi-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "workshop";
            r.apis = {"cna_get_abi_version", "cna_vector3_init_xyz", "cna_vector3_cross", "cna_vector3_dot", "cna_matrix_create_rotation_y", "cna_matrix_create_translation_xyz", "cna_matrix_multiply",
                      "cna_matrix_invert", "cna_vector3_transform_matrix", "cna_quaternion_create_from_yaw_pitch_roll", "cna_quaternion_length", "cna_half_from_single", "cna_curve_create", "cna_curve_get_keys",
                      "cna_curve_key_init_position_value", "cna_curve_key_collection_add", "cna_curve_evaluate", "cna_curve_destroy", "cna_error_get_last_info", "cna_error_copy_last_message", "CNA_ErrorInfo",
                      "CNA_Vector3", "CNA_Matrix", "CNA_Quaternion", "CNA_CurveKey"};
            r.origin = ApiOrigin::CnaExtension;
            return r;
        }(),
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
