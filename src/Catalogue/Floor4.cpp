// Floor 4: effects and lighting. BasicEffect and the other stock effects, then custom shaders.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec ShaderWing()
{
    WingSpec w;
    w.name = "Shader Laboratory";
    w.tagline = "CNA extension: GLSL fragment and vertex shaders through ShaderEffect and SpriteBatch";
    w.theme = "corridor-dark";
    w.accent = {0.70f, 0.45f, 1.0f};
    w.spineSide = Wall::East;
    const auto sh = [](const char* key, const char* title, const char* subtitle, const char* shader) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "shaders";
        r.params = {{"shader", shader}};
        r.width = 14;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "neon-lab";
        r.apis = {"ShaderEffect", "ShaderEffect.SetUniformFloat", "ShaderEffect.DeclareUniformBlockEXT", "GraphicsDevice.GetShaderDialectEXT", "GraphicsDevice.SupportsShaderLanguageEXT", "SpriteBatch.Begin(effect)", "RenderTarget2D"};
        r.origin = ApiOrigin::CnaExtension;
        return r;
    };
    w.rooms = {
        sh("fx-grade", "Colour Grading", "A fragment shader that grays, tints, inverts or two-tones the whole scene", "grade"),
        sh("fx-blur", "Blur", "A 5 x 5 box blur driven by one uniform, applied to a rendered 3D scene", "blur"),
        sh("fx-pixelate", "Pixelate and Posterise", "Snap texture coordinates and quantise colours for a retro look", "pixelate"),
        sh("fx-ripple", "Water Ripple", "UV distortion animated by a time uniform", "ripple"),
        sh("fx-edge", "Edge Detection", "A Sobel filter built from eight extra texture reads per pixel", "edge"),
        sh("fx-crt", "CRT Look", "Scanlines, vignette and barrel distortion in one pass", "crt"),
        sh("fx-plasma", "Procedural Shader", "A colour computed from math alone, mixed over the scene", "plasma"),
        sh("fx-swirl", "Swirl", "Rotating the texture coordinates by a radius-dependent angle, animated by a time uniform", "swirl"),
        sh("fx-chromatic", "Chromatic Aberration", "Sampling red, green and blue at slightly different offsets like a cheap lens", "chromatic"),
        sh("fx-kernel", "Convolution Kernels", "Sharpen, emboss and Laplacian outline from nine texture reads", "kernel"),
        sh("fx-halftone", "Halftone", "Dots whose size follows brightness, one scene sample per cell", "halftone"),
        sh("fx-vertexwave", "Vertex Shader", "The vertex stage moving a sprite's corners with a sine wave", "vertexwave"),
    };
    {
        RoomSpec cm = sh("fx-colormatrix", "Colour Matrix", "A 4 x 4 matrix and an offset turn each pixel's (r, g, b, a): grayscale, sepia, invert, hue and saturation are all one matrix", "colormatrix");
        cm.apis.insert(cm.apis.end(), {"ColorMatrixEffect", "ColorMatrixEffect.SetColorMatrix", "ColorMatrixEffect.SetColorOffset", "Matrix"});
        w.rooms.push_back(cm);
    }
    {
        RoomSpec fb = sh("fx-compiled", "Compiled XNA Effects", "Pixel and vertex-texture effects, including live VertexSamplerStates", "compiled");
        fb.exhibit = "fxb-lab";
        fb.params = {};
        fb.apis = {"Effect", "Effect(GraphicsDevice, byte[])", "Effect.Parameters", "Effect.Techniques", "EffectParameter", "EffectParameter.SetValue", "EffectParameter.GetValueSingle", "EffectParameter.GetValueVector2", "EffectTechnique", "EffectPass", "EffectParameterClass", "EffectParameterType",
                   "GraphicsCapability.CompiledEffects", "GraphicsDevice.VertexTextures", "GraphicsDevice.VertexSamplerStates", "SpriteBatch.Begin",
                   "SamplerStateCollection.operator[]",
                   "EffectContent", "EffectContent.EffectCode", "EffectImporter", "EffectImporter.Import", "EffectProcessor", "EffectProcessor.DebugMode", "EffectProcessor.Defines", "EffectProcessor.Process",
                   "CompiledEffectContent", "CompiledEffectContent.GetEffectCode", "ContentCompiler.Compile(CompiledEffectContent)", "ContentManager.Load<Effect>"};
        fb.origin = ApiOrigin::Xna;
        w.rooms.push_back(fb);
    }
    {
        RoomSpec er = sh("fx-errors", "Shader Compile Errors", "Six deliberate mistakes in a shader: what IsEffectValid, GetCompileErrorEXT and GetShaderDiagnosticsEXT hand back, and the log parser on fixed logs", "errors");
        er.exhibit = "shader-errors";
        er.params = {};
        er.apis = {"ShaderEffect", "ShaderEffect.IsEffectValid", "ShaderEffect.GetCompileErrorEXT", "ShaderEffect.GetShaderDiagnosticsEXT", "CNA::ShaderDiagnosticEXT", "ShaderDiagnosticEXT.parseCompilerLog", "CNA::ShaderCompilationExceptionEXT",
                   "GraphicsDevice.ExecutesShaderEffectSourceEXT", "GraphicsDevice.GetShaderDialectEXT"};
        er.origin = ApiOrigin::CnaExtension;
        w.rooms.push_back(er);
    }
    return w;
}

WingSpec BasicEffectWing()
{
    WingSpec w;
    w.name = "BasicEffect Super-Wing";
    w.tagline = "Every BasicEffect property with a control, plus DualTextureEffect and the Effect model";
    w.theme = "corridor-warm";
    w.accent = {1.0f, 0.75f, 0.35f};
    w.spineSide = Wall::West;
    const auto fx = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin = ApiOrigin::Xna) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "effects";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "white-cube";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        fx("be-combos", "Texture, Vertex Colour, Diffuse", "TextureEnabled x VertexColorEnabled on the same cube, with DiffuseColor, Alpha and lighting", "combos",
           {"BasicEffect.TextureEnabled", "BasicEffect.VertexColorEnabled", "BasicEffect.DiffuseColor", "BasicEffect.Alpha"}),
        fx("be-lights", "Lights and Ambient", "Three directional lights, ambient colour and EnableDefaultLighting", "lights",
           {"BasicEffect.LightingEnabled", "BasicEffect.DirectionalLight0", "BasicEffect.AmbientLightColor", "DirectionalLight", "BasicEffect.EnableDefaultLighting"}),
        fx("be-material", "Material", "DiffuseColor, SpecularColor, SpecularPower and EmissiveColor on a row of spheres", "material",
           {"BasicEffect.SpecularColor", "BasicEffect.SpecularPower", "BasicEffect.EmissiveColor"}),
        fx("be-perpixel", "Per-Vertex and Per-Pixel Lighting", "PreferPerPixelLighting on a coarsely tessellated sphere", "perpixel", {"BasicEffect.PreferPerPixelLighting"}),
        fx("be-fog", "Fog", "FogEnabled, FogColor, FogStart and FogEnd in a corridor of columns", "fog", {"BasicEffect.FogEnabled", "BasicEffect.FogColor", "BasicEffect.FogStart", "BasicEffect.FogEnd"}),
        fx("be-alpha", "Alpha and Sorting", "Translucent spheres: draw order, depth writes and BlendState", "alpha", {"BasicEffect.Alpha", "BlendState.NonPremultiplied", "DepthStencilState.DepthRead"}),
        fx("be-punctual", "Point and Spot Lights", "PunctualLightEXT: one point or spot light with range and cone on a stock BasicEffect", "punctual", {"PunctualLightEXT", "BasicEffect.setPunctualLightEXT"}, ApiOrigin::CnaExtension),
        fx("be-dual", "DualTextureEffect", "Two textures multiplied per pixel: a tiling surface and a lightmap", "dual", {"DualTextureEffect", "DualTextureEffect.Texture2"}),
        [&] {
            RoomSpec r = fx("be-model", "The Effect Object Model", "Parameters (class, type, size, annotations), techniques and passes of six stock effects, the IEffect interfaces and Effect.Clone", "",
                            {"Effect", "EffectParameter", "EffectParameterClass", "EffectParameterType", "EffectParameterCollection", "EffectAnnotation", "EffectTechnique", "EffectPass",
                             "IEffectMatrices", "IEffectLights", "IEffectFog", "Effect.Clone", "SpriteEffect", "SkinnedEffect",
                             "EffectParameter.GetValueBoolean", "EffectParameter.GetValueInt32", "EffectParameter.GetValueString", "EffectParameter.GetValueMatrix", "EffectParameter.GetValueMatrixArray", "EffectParameter.GetValueMatrixTranspose", "EffectParameter.GetValueMatrixTransposeArray", "EffectParameter.GetValueVector3", "EffectParameter.GetValueVector4", "EffectParameter.GetValueTexture2D", "EffectParameter.GetValueTextureCube"});
            r.exhibit = "reflect-lab";
            return r;
        }(),
    };
    return w;
}

WingSpec LightingWing()
{
    WingSpec w;
    w.name = "Lighting Laboratory";
    w.tagline = "Directional, point and spot lights, attenuation, colour, many lights, flashlight, emissive";
    w.theme = "corridor-dark";
    w.accent = {1.0f, 0.88f, 0.45f};
    w.spineSide = Wall::East;
    const auto li = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin = ApiOrigin::CnaExtension) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "lighting-lab";
        r.params = {{"mode", mode}};
        r.width = 14;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "dark-room";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        li("light-types", "Directional, Point and Spot", "The three light types on the same sphere and floor", "types", {"DirectionalLight", "PunctualLightEXT", "BasicEffect"}),
        li("light-falloff", "Attenuation and Range", "How a point light fades with distance and where its range ends", "falloff", {"PunctualLightEXT.Range", "BasicEffect"}),
        li("light-coloured", "Coloured Lights", "Red, green and blue lamps added together with additive blending", "coloured", {"PunctualLightEXT.DiffuseColor", "BlendState.Additive"}),
        li("light-many", "Many Lights", "Up to 24 lights by multi-pass additive rendering, with live frame rate", "many", {"PunctualLightEXT", "BlendState.Additive", "DepthStencilState.DepthBufferFunction"}),
        li("light-flashlight", "Flashlight", "A spot light at your eye, aimed where you look", "flashlight", {"PunctualLightKindEXT.Spot", "PunctualLightEXT.OuterAngle"}),
        li("light-emissive", "Emissive Materials", "EmissiveColor glows without a lamp; an additive halo fakes bloom", "emissive", {"BasicEffect.EmissiveColor", "BlendState.Additive"}, ApiOrigin::Xna),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor4()
{
    FloorSpec f;
    f.index = 4;
    f.name = "Effects and Lighting";
    f.tagline = "BasicEffect, the stock effects, lighting and custom shaders";
    f.theme = "concourse";
    f.accent = {0.70f, 0.45f, 1.0f};
    f.wings.push_back(BasicEffectWing());
    f.wings.push_back(LightingWing());
    f.wings.push_back(ShaderWing());
    return f;
}
