// Floor 3: three-dimensional fundamentals. Vertex data and buffers, then textures on surfaces.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

RoomSpec Geo(const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis)
{
    RoomSpec r;
    r.key = key;
    r.title = title;
    r.subtitle = subtitle;
    r.exhibit = "geometry";
    r.params = {{"mode", mode}};
    r.width = 13;
    r.depth = 13;
    r.height = 5.2f;
    r.theme = "neon-lab";
    r.apis = std::move(apis);
    return r;
}

WingSpec BuffersWing()
{
    WingSpec w;
    w.name = "Vertices and Buffers";
    w.tagline = "Triangles, indices, primitive types, dynamic and large meshes, instancing";
    w.theme = "corridor-dark";
    w.accent = {0.40f, 0.85f, 0.75f};
    w.spineSide = Wall::East;
    w.rooms = {
        Geo("geo-triangle", "A Triangle and Its Winding", "Vertex order decides which side faces you; RasterizerState.CullMode removes one", "triangle",
            {"VertexPositionColor", "GraphicsDevice.DrawUserPrimitives", "RasterizerState.CullMode"}),
        [] {
            RoomSpec r = Geo("geo-vertextypes", "Vertex Types", "VertexPositionColor, Texture, ColorTexture, NormalTexture and a vertex the museum declares, with the declaration each reports", "",
                             {"VertexDeclaration", "VertexDeclaration.GetVertexElements", "VertexDeclaration.VertexStride", "VertexElement", "VertexElementFormat", "VertexElementUsage", "VertexPositionColor", "VertexPositionTexture",
                              "VertexPositionColorTexture", "VertexPositionNormalTexture", "VertexBuffer", "BasicEffect.VertexColorEnabled", "BasicEffect.TextureEnabled"});
            r.exhibit = "vertextypes-lab";
            r.params = {};
            return r;
        }(),
        Geo("geo-indexed", "Vertex and Index Buffers", "One cube drawn without indices, with 8 shared vertices and with 24 lit vertices", "indexed",
            {"VertexBuffer", "IndexBuffer", "GraphicsDevice.DrawIndexedPrimitives", "GraphicsDevice.DrawPrimitives"}),
        Geo("geo-primitives", "Primitive Types", "TriangleList, TriangleStrip, LineList, LineStrip and the CNA point list from the same vertices", "primitives",
            {"PrimitiveType", "GraphicsDevice.DrawUserPrimitives"}),
        Geo("geo-dynamic", "Dynamic Vertex Buffers", "A DynamicVertexBuffer rewritten every frame with Discard, NoOverwrite or None", "dynamic",
            {"DynamicVertexBuffer", "SetDataOptions", "VertexPositionNormalTexture", "GraphicsDevice.DrawUserIndexedPrimitives"}),
        Geo("geo-procedural", "Procedural Geometry", "Sphere, torus, cone and height field generated in code, with wireframe", "procedural",
            {"VertexBuffer", "IndexBuffer", "RasterizerState.FillMode"}),
        Geo("geo-index32", "32-Bit Indices", "A grid of up to 160 000 vertices drawn with IndexElementSize.ThirtyTwoBits", "index32", {"IndexBuffer", "IndexElementSize"}),
        Geo("geo-manybuffers", "Many Small Buffers", "Hundreds of VertexBuffers against one merged buffer: the cost of draw calls", "manybuffers", {"VertexBuffer", "GraphicsDevice.DrawIndexedPrimitives"}),
        Geo("geo-instancing", "Hardware Instancing", "DrawInstancedPrimitives with a per-instance world matrix stream, against one call per cube", "instancing",
            {"VertexBufferBinding", "GraphicsDevice.GetVertexBuffers", "GraphicsDevice.SetVertexBuffers", "GraphicsDevice.DrawInstancedPrimitives", "GraphicsCapability.Instancing", "GraphicsDevice.DrawInstancedPrimitivesBaseInstanceEXT", "RendererFeature.BaseInstanceDrawing"}),
        Geo("geo-dynindex", "Dynamic Index Buffers", "A DynamicIndexBuffer rewritten every frame to draw only the part of a big mesh near a moving point", "dynindex",
            {"DynamicIndexBuffer", "SetDataOptions.Discard", "GraphicsDevice.DrawIndexedPrimitives", "IndexBuffer"}),
        [] {
            RoomSpec r = Geo("geo-terrain", "Heightmap Terrain", "A heightmap read with GetData becomes a vertex grid with 16-bit indices, normals from neighbouring heights and a ball that follows the ground", "",
                             {"Texture2D.GetData", "VertexBuffer", "IndexBuffer", "VertexPositionNormalTexture", "GraphicsDevice.DrawIndexedPrimitives", "RasterizerState.FillMode", "MathHelper.Lerp"});
            r.exhibit = "terrain-lab";
            return r;
        }(),
    };
    return w;
}

WingSpec TextureWing()
{
    WingSpec w;
    w.name = "Textures";
    w.tagline = "Mip levels, filters, addressing, UVs, formats, alpha, cube and volume textures, image encode/decode";
    w.theme = "corridor-green";
    w.accent = {0.55f, 0.90f, 0.40f};
    w.spineSide = Wall::West;
    const auto tex = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r = Geo(key, title, subtitle, mode, std::move(apis));
        r.exhibit = "textures";
        r.theme = "white-cube";
        return r;
    };
    w.rooms = {
        tex("tex-mip", "Mip Levels and Filtering", "A coloured mip chain on a receding plane with every SamplerState property live", "mip",
            {"Texture2D", "SamplerState.Filter", "TextureFilter.MinLinearMagPointMipLinear", "TextureFilter.MinPointMagLinearMipPoint", "SamplerState.MipMapLevelOfDetailBias", "SamplerState.MaxMipLevel", "SamplerState.MaxAnisotropy"}),
        tex("tex-filter", "Magnification Filters", "Point, Linear and Anisotropic on a tiny texture shown at a steep angle", "filter", {"SamplerState.PointClamp", "SamplerState.LinearClamp", "SamplerState.AnisotropicClamp"}),
        tex("tex-address", "Addressing Modes", "Wrap, Clamp and Mirror with UVs outside the 0..1 range", "address", {"TextureAddressMode", "SamplerState.AddressU"}),
        tex("tex-uv", "UV Transforms", "Tiling, scrolling and rotating texture coordinates without a shader", "uv", {"VertexPositionColorTexture", "SamplerState.LinearWrap"}),
        tex("tex-dynamic", "Dynamic Textures and Readback", "Texture2D.SetData every frame and GetData on one texel", "dynamic", {"Texture2D.SetData", "Texture2D.GetData"}),
        tex("tex-formats", "Surface Formats", "The same gradient in Color, Bgr565, Bgra4444, Alpha8 and HalfVector4", "formats", {"SurfaceFormat", "PackedVector.Bgr565", "PackedVector.Bgra4444", "PackedVector.Alpha8", "PackedVector.HalfVector4"}),
        tex("tex-alpha", "Alpha: Test, Blend, Opaque", "AlphaTestEffect with a reference alpha against blending and opaque drawing", "alpha", {"AlphaTestEffect", "BlendState.NonPremultiplied", "CompareFunction"}),
        tex("tex-cube", "Cube Maps and Environment Maps", "A procedural TextureCube reflected by EnvironmentMapEffect", "cube", {"TextureCube", "EnvironmentMapEffect", "CubeMapFace", "TextureCube.SetData", "TextureCube.GetData"}),
        tex("tex-volume", "Volume Textures", "Texture3D storage and readback in Color, float and Vector4 formats, whole volume or a box, shown slice by slice", "volume", {"Texture3D", "Texture3D.SetData", "Texture3D.GetData", "Texture3D.GetData(level, left, front)", "SurfaceFormat.Single", "SurfaceFormat.Vector4"}),
        []{
            RoomSpec r;
            r.key = "tex-volshader";
            r.title = "Sampling a Volume in a Shader";
            r.subtitle = "ShaderEffect.SetTexture(unit, Texture3D) feeds a sampler3D; the third coordinate, AddressW and the filter decide what each pixel reads";
            r.exhibit = "volumeshader-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.2f;
            r.theme = "white-cube";
            r.apis = {"ShaderEffect.SetTexture(Texture3D)", "SamplerState.AddressW", "SamplerState.AddressU", "SamplerState.AddressV", "Texture3D", "TextureAddressMode", "TextureFilter"};
            r.origin = ApiOrigin::CnaExtension;
            return r;
        }(),
        tex("tex-io", "Encoding and Decoding Images", "Texture2D.SaveAsPng and SaveAsJpeg into a stream, then Texture2D.FromStream back with fit and zoom", "io",
            {"Texture2D.SaveAsPng", "Texture2D.SaveAsJpeg", "Texture2D.FromStream", "System.IO.MemoryStream"}),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor3()
{
    FloorSpec f;
    f.index = 3;
    f.name = "Three-Dimensional Fundamentals";
    f.tagline = "Vertex data, buffers, primitives and textures";
    f.theme = "concourse";
    f.accent = {0.40f, 0.85f, 0.75f};
    f.wings.push_back(BuffersWing());
    f.wings.push_back(TextureWing());
    return f;
}
