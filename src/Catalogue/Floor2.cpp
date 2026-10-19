// Floor 2: two-dimensional rendering. SpriteBatch, SpriteFont and the 2D techniques built on them.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

RoomSpec Sprite(const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis)
{
    RoomSpec r;
    r.key = key;
    r.title = title;
    r.subtitle = subtitle;
    r.exhibit = "sprites";
    r.params = {{"mode", mode}};
    r.width = 13;
    r.depth = 13;
    r.height = 5.2f;
    r.theme = "pop-studio";
    r.apis = std::move(apis);
    return r;
}

WingSpec BasicsWing()
{
    WingSpec w;
    w.name = "SpriteBatch Basics";
    w.tagline = "Position, rotation, origin, scale, flips, source rectangles, tint";
    w.theme = "corridor-pop";
    w.accent = {1.0f, 0.62f, 0.30f};
    w.spineSide = Wall::East;
    w.rooms = {
        Sprite("sprite-basics", "Sprites: Draw and Its Arguments", "SpriteBatch.Draw with position, rotation, origin, scale and SpriteEffects, on a live render-target screen", "basics",
               {"SpriteBatch.Begin", "SpriteBatch.Draw", "SpriteEffects", "RenderTarget2D"}),
        Sprite("sprite-origin", "Origin and Rotation", "Why the origin matters: the same rotation around three different pivot points", "origin", {"SpriteBatch.Draw", "Vector2"}),
        Sprite("sprite-sheet", "Sprite Sheets", "Source rectangles and animation frames from one small texture, with point or linear sampling", "sheet", {"SpriteBatch.Draw", "Rectangle", "SamplerState.PointClamp"}),
        Sprite("sprite-tint", "Tint and Alpha", "The Color argument as a multiplier: tint, fade and Color.Lerp over a checkerboard", "tint", {"Color", "Color.Lerp", "BlendState.NonPremultiplied"}),
    };
    return w;
}

WingSpec StateWing()
{
    WingSpec w;
    w.name = "Blending, Sorting and Sampling";
    w.tagline = "BlendState, SpriteSortMode and SamplerState compared visually";
    w.theme = "corridor-pop";
    w.accent = {0.95f, 0.45f, 0.75f};
    w.spineSide = Wall::West;
    w.rooms = {
        Sprite("sprite-blend", "BlendState Gallery", "Eight blend configurations of the same two sprites: presets, custom factors, Min and Max", "blend",
               {"BlendState", "BlendState.Additive", "BlendState.AlphaBlend", "Blend", "BlendFunction"}),
        Sprite("sprite-sort", "SpriteSortMode and Layer Depth", "Deferred, Immediate, Texture, BackToFront and FrontToBack, with a movable layer", "sort", {"SpriteSortMode", "SpriteBatch.Draw"}),
        Sprite("sprite-sampler", "Filtering and Addressing", "Point, Linear and Anisotropic; Wrap, Clamp and Mirror; source rectangles beyond the texture", "sampler",
               {"SamplerState", "TextureAddressMode"}),
    };
    return w;
}

WingSpec TechniqueWing()
{
    WingSpec w;
    w.name = "Sprite Techniques";
    w.tagline = "Parallax, UI nine-slice, atlases, scissoring, 2D cameras and batching";
    w.theme = "corridor-pop";
    w.accent = {1.0f, 0.85f, 0.30f};
    w.spineSide = Wall::East;
    w.rooms = {
        Sprite("sprite-parallax", "Scrolling and Parallax", "Wrapping textures with SourceRectangle and SamplerState to scroll four layers at four speeds", "parallax", {"SamplerState.LinearWrap", "SpriteBatch.Draw"}),
        Sprite("sprite-ui", "UI Panels and Nine-Slice", "A run-time Texture2D skin stretched with nine source rectangles", "ui", {"Texture2D.SetData", "SpriteBatch.Draw", "Rectangle"}),
        Sprite("sprite-atlas", "Texture Atlases", "Packing textures into one with GetData and SetData, drawn by source rectangle", "atlas", {"Texture2D.GetData", "Texture2D.SetData", "SpriteBatch.Draw"}),
        Sprite("sprite-scissor", "Scissor Clipping", "RasterizerState.ScissorTestEnable and GraphicsDevice.ScissorRectangle clipping a scrolling list", "scissor",
               {"RasterizerState.ScissorTestEnable", "GraphicsDevice.ScissorRectangle"}),
        Sprite("sprite-camera", "A 2D Camera Matrix", "SpriteBatch.Begin with a transformMatrix: pan, zoom and rotate a tile world", "camera", {"SpriteBatch.Begin", "Matrix.CreateTranslation", "Matrix.CreateScale", "Matrix.CreateRotationZ"}),
        Sprite("sprite-hit", "Per-Pixel Collision", "Rectangle.Intersects as the cheap test and a GetData pixel test as the exact one, with the shared pixels highlighted", "hit",
               {"Rectangle.Intersects", "Rectangle.Intersect", "Texture2D.GetData", "SpriteBatch.Draw"}),
        Sprite("sprite-anim", "Sprite Animation", "Clips, loop modes and elapsed-time frame selection with SourceRectangle and SpriteEffects.FlipHorizontally", "anim",
               {"SpriteBatch.Draw", "Rectangle", "SpriteEffects.FlipHorizontally", "Texture2D.SetData"}),
        Sprite("sprite-tilemap", "Tile Maps and Culling", "A level read from a Texture2D with GetData, a tile atlas drawn by source rectangle and only the visible tiles submitted", "tilemap",
               {"Texture2D.GetData", "SpriteBatch.Draw", "Rectangle.Intersects", "SpriteBatch.Begin", "Matrix.CreateScale"}),
        Sprite("sprite-lights", "2D Lighting with a Light Map", "Lights added into a render target, then multiplied over the scene with Blend.DestinationColor: no shader", "lights",
               {"RenderTarget2D", "BlendState.Additive", "Blend.DestinationColor", "SpriteBatch.Draw", "SamplerState.LinearClamp"}),
        Sprite("sprite-stress", "Thousands of Sprites", "One Begin/End with up to twenty thousand animated sprites and live throughput", "stress", {"SpriteBatch", "SpriteBatch.Draw"}),
    };
    return w;
}

WingSpec TextWing()
{
    WingSpec w;
    w.name = "Text and Fonts";
    w.tagline = "SpriteFont: layout, measurement, transforms, glyphs, HUDs and world labels";
    w.theme = "corridor-pop";
    w.accent = {0.55f, 0.85f, 1.0f};
    w.spineSide = Wall::West;
    const auto text = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r = Sprite(key, title, subtitle, mode, std::move(apis));
        r.exhibit = "text";
        return r;
    };
    w.rooms = {
        text("text-align", "Alignment and Wrapping", "Left, centre, right and justified text built from SpriteFont.MeasureString", "align", {"SpriteFont.MeasureString", "SpriteBatch.DrawString"}),
        text("text-measure", "Measuring Text", "MeasureString boxes, Spacing and LineSpacing on a private font", "measure", {"SpriteFont.MeasureString", "SpriteFont.Spacing", "SpriteFont.LineSpacing"}),
        text("text-transform", "Rotating and Scaling Text", "DrawString with rotation, origin, per-axis scale, flips, per-character colour and text on a circle", "transform",
             {"SpriteBatch.DrawString", "SpriteEffects"}),
        text("text-glyphs", "Glyphs and the Default Character", "The font's character set and what happens with characters it does not have", "glyphs", {"SpriteFont.GlyphBoundsEXT", "SpriteFont.CroppingEXT", "SpriteFont.KerningEXT", "SpriteFont.TextureEXT", "SpriteFont.Characters", "SpriteFont.DefaultCharacter"}),
        text("text-hud", "HUD Text and StringBuilder", "A health bar, animated score, clock and the StringBuilder overload of DrawString", "hud", {"SpriteBatch.DrawString", "System.Text.StringBuilder"}),
        text("text-labels", "World Labels", "Labels above 3D objects with Viewport.Project and edge pointers for off-screen targets", "labels", {"Viewport.Project", "SpriteBatch.DrawString"}),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor2()
{
    FloorSpec f;
    f.index = 2;
    f.name = "Two-Dimensional Rendering";
    f.tagline = "SpriteBatch, SpriteFont and the techniques built on them";
    f.theme = "concourse";
    f.accent = {1.0f, 0.62f, 0.30f};
    f.wings.push_back(BasicsWing());
    f.wings.push_back(StateWing());
    f.wings.push_back(TechniqueWing());
    f.wings.push_back(TextWing());
    return f;
}
