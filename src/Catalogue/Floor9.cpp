// Floor 9: performance, validation and the rooftop.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec PerformanceWing()
{
    WingSpec w;
    w.name = "Performance Lab";
    w.tagline = "Stress levels for draw calls, sprites, state changes, uploads, render targets, overdraw and particles";
    w.theme = "industrial";
    w.accent = {0.95f, 0.30f, 0.30f};
    w.spineSide = Wall::East;
    const auto pf = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin = ApiOrigin::Xna) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "perf-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "industrial";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        pf("pf-dashboard", "Frame-Time Dashboard", "Update-time history, 1% lows and the engine's own counters", "dashboard", {"GameTime.ElapsedGameTime", "GameTime.IsRunningSlowly", "CNA::Diagnostics::GetProvider", "CNA::Diagnostics::IDiagnosticsProvider.CaptureSnapshot", "CNA::Diagnostics::Snapshot.metrics", "CNA::Diagnostics::MetricSample.name", "CNA::Diagnostics::MetricSample.value"}, ApiOrigin::CnaExtension),
        pf("pf-objects", "Draw Calls and Instancing", "10 to 20,000 cubes: a call each, or DrawInstancedPrimitives", "objects", {"GraphicsDevice.DrawIndexedPrimitives", "GraphicsDevice.DrawInstancedPrimitives", "VertexBufferBinding", "GraphicsCapability.Instancing"}),
        pf("pf-sprites", "SpriteBatch Volume", "100 to 60,000 sprites with sort modes and texture counts", "sprites", {"SpriteBatch.Draw", "SpriteSortMode", "SpriteBatch.Begin"}),
        pf("pf-state", "State Changes and Batching", "The same quads drawn with and without sorting by texture", "state", {"BasicEffect.Texture", "EffectPass.Apply", "CNA::Diagnostics::GetProvider", "CNA::Diagnostics::IDiagnosticsProvider.CaptureSnapshot", "CNA::Diagnostics::Snapshot.metrics"}, ApiOrigin::CnaExtension),
        pf("pf-buffers", "Buffer Uploads", "DynamicVertexBuffer, DrawUserPrimitives and static buffers compared", "buffers", {"DynamicVertexBuffer.SetData", "SetDataOptions.Discard", "GraphicsDevice.DrawUserPrimitives", "VertexBuffer"}),
        pf("pf-targets", "Render-Target Switches", "1 to 32 small render targets drawn every frame", "targets", {"GraphicsDevice.SetRenderTarget", "RenderTarget2D", "CNA::Diagnostics::GetProvider", "CNA::Diagnostics::IDiagnosticsProvider.CaptureSnapshot", "CNA::Diagnostics::Snapshot.metrics"}, ApiOrigin::CnaExtension),
        pf("pf-overdraw", "Overdraw", "1 to 32 full-screen translucent layers", "overdraw", {"SpriteBatch.Draw", "BlendState.NonPremultiplied"}),
        pf("pf-particles", "CPU Particles", "1,000 to 250,000 particles updated on the CPU and drawn as points", "particles", {"PrimitiveType.PointListEXT", "GraphicsDevice.DrawUserPrimitives"}, ApiOrigin::CnaExtension),
    };
    return w;
}


WingSpec ValidationWing()
{
    WingSpec w;
    w.name = "Renderer Validation";
    w.tagline = "Self-checking test images: every renderer must pass the same checks with the same stock calls";
    w.theme = "neon-lab";
    w.accent = {0.95f, 0.30f, 0.30f};
    w.spineSide = Wall::West;
    const auto rv = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "validation-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "neon-lab";
        r.apis = std::move(apis);
        r.origin = ApiOrigin::Xna;
        return r;
    };
    w.rooms = {
        rv("rv-geometry", "Geometry and Coverage", "Culling, viewport, scissor and exact pixel coverage of quads", "geometry", {"RasterizerState.CullMode", "GraphicsDevice.Viewport", "GraphicsDevice.ScissorRectangle", "GraphicsDevice.DrawUserPrimitives"}),
        rv("rv-sprites", "SpriteBatch Pixels", "Texel-exact 1:1 drawing, scaling, flips, rotation and filtering", "sprites", {"SpriteBatch.Draw", "SpriteEffects", "SamplerState.PointClamp", "SamplerState.LinearClamp"}),
        rv("rv-blend", "Blending", "Alpha, premultiplied, additive, masks, blend factor and Max", "blend", {"BlendState", "BlendFunction", "ColorWriteChannels", "Blend.BlendFactor"}),
        rv("rv-depth", "Depth Buffer", "Depth test, functions, write masks and precision", "depth", {"DepthStencilState", "CompareFunction", "DepthFormat.Depth24"}),
        rv("rv-shading", "Interpolation, Lighting and Fog", "Vertex colours, N.L diffuse lighting and linear fog", "shading", {"BasicEffect.VertexColorEnabled", "BasicEffect.DirectionalLight0", "BasicEffect.FogEnabled"}),
        rv("rv-sampling", "Texture Sampling", "Wrap, Clamp, Mirror, point and linear filtering", "sampling", {"SamplerState.AddressU", "TextureAddressMode"}),
        rv("rv-targets", "Render Targets", "Orientation, reuse as a texture and half-float targets", "targets", {"RenderTarget2D", "GraphicsDevice.SetRenderTarget", "Texture2D.GetData"}),
        rv("rv-camera", "Camera Matrices", "LookAt, perspective and orthographic projection, world translation", "camera", {"Matrix.CreateLookAt", "Matrix.CreatePerspectiveFieldOfView", "Matrix.CreateOrthographic", "BasicEffect.World"}),
        rv("rv-formats", "Surface Formats", "Every uncompressed texture format returns exactly the bits it was given", "formats", {"SurfaceFormat", "Texture2D.SetData", "Texture2D.GetData", "PackedVector.Bgr565", "PackedVector.Rgba64", "PackedVector.HalfVector4"}),
        rv("rv-effects", "Stock Effects", "BasicEffect, AlphaTestEffect and DualTextureEffect produce the expected pixels", "effects", {"BasicEffect", "AlphaTestEffect", "DualTextureEffect"}),
        rv("rv-vertextype", "A Game's Own Vertex Type", "An IVertexType registered with VertexTypeRegistryEXT, a buffer made from its Type, drawn; an unknown type refused", "vertextype",
           {"CNA::Graphics::VertexTypeRegistryEXT", "VertexTypeRegistryEXT.Register", "VertexTypeRegistryEXT.TryResolve", "VertexTypeRegistryEXT.Resolve", "IVertexType", "IVertexType.VertexDeclaration",
            "VertexBuffer(GraphicsDevice, Type, int, BufferUsage)", "DynamicVertexBuffer(GraphicsDevice, Type, int, BufferUsage)", "VertexDeclaration", "VertexElement", "VertexDeclaration.VertexStride",
            "VertexDeclaration.GetVertexElements", "System::ArgumentException", "GraphicsDevice.DrawPrimitives"}),
    };
    return w;
}


WingSpec RoofWing()
{
    WingSpec w;
    w.name = "Observation Deck";
    w.tagline = "Open-air rooms under the museum sky: the clock, a garden, a telescope and a weather station";
    w.theme = "garden";
    w.accent = {0.95f, 0.30f, 0.30f};
    w.spineSide = Wall::East;
    const auto rf = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "roof-lab";
        r.params = {{"mode", mode}};
        r.width = 15;
        r.depth = 15;
        r.height = 5.0f;
        r.theme = "garden";
        r.openRoof = true;
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        rf("rf-deck", "Sun, Moon and the Museum Clock", "Time of day drives the sun, the daylight, the ambient colour and the lamps", "deck", {"Vector3", "BasicEffect.EmissiveColor", "MathHelper"}, ApiOrigin::Xna),
        rf("rf-garden", "Terrace Garden", "A particle fountain and wind-blown leaves drawn as camera-facing quads", "garden", {"DynamicVertexBuffer", "BlendState.NonPremultiplied", "Texture2D", "SoundEffectInstance"}, ApiOrigin::Xna),
        rf("rf-telescope", "Telescope", "A narrow field of view into a render target: aim at the sun, the moon or the skyline", "telescope", {"Matrix.CreatePerspectiveFieldOfView", "RenderTarget2D", "BasicEffect.FogEnabled", "Matrix.CreateLookAt"}, ApiOrigin::Xna),
        rf("rf-weather", "Weather Station", "Rain as lines, snow and splashes as particles, wind, visibility and lightning", "weather", {"PrimitiveType.LineList", "DynamicVertexBuffer", "SetDataOptions.Discard", "SpriteBatch"}, ApiOrigin::Xna),
    };
    return w;
}


WingSpec ReferenceWing()
{
    WingSpec w;
    w.name = "Reference Scenes";
    w.tagline = "Named viewpoints that the visual regression tool renders and compares";
    w.theme = "white-cube";
    w.accent = {0.95f, 0.30f, 0.30f};
    w.spineSide = Wall::West;
    RoomSpec r;
    r.key = "vr-index";
    r.title = "Reference Scenes";
    r.subtitle = "Teleport to the exact views that tools/reference.sh captures and compares";
    r.exhibit = "views-lab";
    r.width = 13;
    r.depth = 13;
    r.height = 5.0f;
    r.theme = "white-cube";
    r.apis = {"SpriteBatch.DrawString"};   // the index is application behaviour; its screen is plain SpriteBatch text
    r.origin = ApiOrigin::Application;
    w.rooms = {r};
    return w;
}

}  // namespace

FloorSpec MakeFloor9()
{
    FloorSpec f;
    f.index = 9;
    f.name = "Performance and Validation";
    f.tagline = "Stress tests, renderer self-checks, reference scenes and the observation deck";
    f.theme = "concourse";
    f.accent = {0.95f, 0.30f, 0.30f};
    f.wings.push_back(PerformanceWing());
    f.wings.push_back(ValidationWing());
    f.wings.push_back(RoofWing());
    f.wings.push_back(ReferenceWing());
    return f;
}
