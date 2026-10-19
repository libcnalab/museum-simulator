// Floor 0: the lobby and the original introductory tour.
#include "Catalogue/CatalogueBuilder.hpp"

FloorSpec MakeFloor0()
{
    FloorSpec f;
    f.index = 0;
    f.name = "Lobby and Introductory Tour";
    f.tagline = "Start here: a first look at what CNA can do";
    f.theme = "classic";
    f.accent = {0.85f, 0.70f, 0.30f};
    f.hubExhibit = "lobby";
    f.hubParams = {{"glass-wall", "south"}};
    // Keep the lobby footprint aligned with the halls above it: the west stair shafts
    // share their door plane through all ten storeys.
    f.hubHalfW = 11.0f;
    f.hubHalfD = 9.0f;
    f.hubHeight = 5.6f;

    WingSpec first;
    first.name = "First Steps";
    first.tagline = "What CNA is, models, textures, lighting and the sky";
    first.theme = "corridor-warm";
    first.accent = {0.90f, 0.72f, 0.35f};
    first.spineSide = Wall::East;
    first.rooms = {
        {.key = "info", .title = "CNA Information Hall", .subtitle = "What CNA is, what you are running on, and how to get around",
         .exhibit = "info", .width = 18, .depth = 18, .height = 7.0f, .theme = "info",
         .apis = {"GraphicsDevice"}, .origin = ApiOrigin::Xna},
        {.key = "models", .title = "Model Gallery", .subtitle = "ContentManager.Load<Model>, world/view/projection and model transforms",
         .exhibit = "models", .width = 14, .depth = 14, .height = 5.0f, .theme = "wood-gallery",
         .apis = {"Model", "ModelMesh", "ContentManager", "Matrix"}},
        {.key = "materials", .title = "Materials & Textures", .subtitle = "Texture2D, sampler filtering, addressing, mipmaps and UV mapping",
         .exhibit = "materials", .width = 14, .depth = 14, .height = 5.0f, .theme = "tiles",
         .apis = {"Texture2D", "SamplerState", "SamplerStateCollection"}},
        {.key = "lighting", .title = "Lighting Room", .subtitle = "BasicEffect directional lights, per-pixel lighting, specular and emissive",
         .exhibit = "lighting", .width = 14, .depth = 14, .height = 5.0f, .theme = "dark-room",
         .apis = {"BasicEffect", "DirectionalLight", "BasicEffect.PreferPerPixelLighting"}},
        {.key = "sky", .title = "Sky Courtyard", .subtitle = "A sky dome, day-night cycle and directional sunlight",
         .exhibit = "sky", .width = 16, .depth = 16, .height = 4.6f, .theme = "courtyard",
         .apis = {"BlendState", "DepthStencilState"}, .openRoof = true},
        {.key = "grounds-sculpture", .title = "Sculpture Garden", .subtitle = "Animated outdoor sculpture and lamps that come on at dusk",
         .exhibit = "roof-lab", .params = {{"mode", "sculpture"}}, .width = 18, .depth = 18, .height = 5.0f, .theme = "courtyard",
         .apis = {"BasicEffect.World", "BasicEffect.EmissiveColor", "Matrix.CreateRotationY", "Matrix.CreateScale"}, .openRoof = true},
        {.key = "grounds-fountain", .title = "Fountain Plaza", .subtitle = "Water jets, wind-blown leaves and exterior lamps under the sky",
         .exhibit = "roof-lab", .params = {{"mode", "garden"}}, .width = 18, .depth = 18, .height = 5.0f, .theme = "garden",
         .apis = {"DynamicVertexBuffer", "BlendState.NonPremultiplied", "Texture2D", "SoundEffectInstance"}, .openRoof = true},
    };
    f.wings.push_back(first);

    WingSpec show;
    show.name = "Showcases";
    show.tagline = "Render state, render targets, particles, animation and audio";
    show.theme = "corridor-cool";
    show.accent = {0.45f, 0.70f, 0.95f};
    show.spineSide = Wall::West;
    show.rooms = {
        {.key = "renderstates", .title = "Render State Room", .subtitle = "DepthStencilState, RasterizerState and BlendState side by side",
         .exhibit = "renderstates", .width = 14, .depth = 14, .height = 5.0f, .theme = "stone-hall",
         .apis = {"DepthStencilState", "RasterizerState", "BlendState"}},
        {.key = "rendertarget", .title = "Render Target Room", .subtitle = "RenderTarget2D: the museum filmed by a security camera",
         .exhibit = "rendertarget", .width = 14, .depth = 12, .height = 4.6f, .theme = "control-room",
         .apis = {"RenderTarget2D", "GraphicsDevice.SetRenderTarget"}},
        {.key = "particles", .title = "Particle Room", .subtitle = "Dynamic geometry: fire, smoke, sparks and a fountain",
         .exhibit = "particles", .width = 14, .depth = 14, .height = 5.5f, .theme = "cave",
         .apis = {"DynamicVertexBuffer", "IndexBuffer", "BlendState"}},
        {.key = "animation", .title = "Animation Room", .subtitle = "Matrix hierarchies: a robot arm and an orrery",
         .exhibit = "animation", .width = 14, .depth = 14, .height = 5.5f, .theme = "machine",
         .apis = {"Matrix"}},
        {.key = "audio", .title = "Audio Room", .subtitle = "SoundEffect, 3D positional audio and interactive playback",
         .exhibit = "audio", .width = 14, .depth = 14, .height = 5.0f, .theme = "audio-hall",
         .apis = {"SoundEffect", "SoundEffectInstance", "AudioEmitter", "AudioListener"}},
    };
    f.wings.push_back(show);
    return f;
}
