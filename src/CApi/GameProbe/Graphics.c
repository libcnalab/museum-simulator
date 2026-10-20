/* The C game probe's graphics checks, run inside a draw callback: the renderer's identity (graphics.h), a render
 * target cleared and drawn into and read back pixel by pixel (render_target.h, texture.h, graphics_device.h), the
 * XNA state presets (graphics_state.h), user primitives with BasicEffect, vertex and index buffers (vertex_resources.h,
 * index_resources.h), SpriteBatch, PNG round trips, the GraphicsResource contract (graphics_resource.h) and the
 * Viewport value arithmetic. Expectations follow XNA 4.0 (Viewport.Project and ToString, the state presets). */
#include <math.h>

#include "CApi/GameProbe/GameProbe.h"

#define TARGET_SIZE 16

static int Same(CNA_Color a, CNA_Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static CNA_Color Pixel(const CNA_Color* pixels, int x, int y)
{
    return pixels[y * TARGET_SIZE + x];
}

/* Unbinds the target and reads all of it back. */
static CNA_Result ReadTarget(CNA_Handle device, CNA_Handle target, CNA_Color* pixels)
{
    uint64_t count = 0;
    const CNA_Result unbound = cna_graphics_device_set_render_target2d(device, CNA_INVALID_HANDLE);
    if (unbound != CNA_RESULT_SUCCESS) return unbound;
    return cna_texture2d_get_data_rgba8(target, pixels, TARGET_SIZE * TARGET_SIZE, &count);
}

static int disposingCalls;

static void OnDisposing(CNA_Handle resource, void* context)
{
    (void)resource;
    (void)context;
    disposingCalls += 1;
}

static void RendererIdentity(GameProbe* g, CNA_Handle device)
{
    CProbe* p = &g->probe;
    CNA_RendererInfo info;
    VERSIONED(info);
    cna_graphics_device_get_renderer_info(device, &info);
    char name[96];
    uint64_t nameSize = 0, nameBytes = 0;
    cna_graphics_device_get_renderer_name_size(device, &nameSize);
    const CNA_Result named = cna_graphics_device_copy_renderer_name(device, name, sizeof name - 1, &nameBytes);
    cprobe_terminate(name, sizeof name, named, &nameBytes);
    CNA_ShaderDialect dialect = 0;
    const CNA_Result dialectResult = cna_graphics_device_get_shader_dialect_ext(device, &dialect);
    uint64_t reportSize = 0;
    cna_graphics_device_get_capability_report_size_ext(device, &reportSize);
    static char report[65536];
    uint64_t reportBytes = 0;
    const CNA_Result reportResult = cna_graphics_device_copy_capability_report_ext(device, report, sizeof report - 1, &reportBytes);
    cprobe_terminate(report, sizeof report, reportResult, &reportBytes);
    CNA_BackBufferInfo backBuffer;
    VERSIONED(backBuffer);
    cna_graphics_device_get_backbuffer_info(device, &backBuffer);
    CNA_GraphicsProfile profile = 99;
    cna_graphics_device_get_graphics_profile(device, &profile);
    char deviceType[96];
    uint64_t deviceTypeBytes = 0;
    const CNA_Result deviceTypeResult = cna_graphics_device_copy_type_name(device, deviceType, sizeof deviceType - 1, &deviceTypeBytes);
    cprobe_terminate(deviceType, sizeof deviceType, deviceTypeResult, &deviceTypeBytes);
    cprobe_got(p, "renderer \"%s\" (%llu = %llu bytes), type %u, max texture %u, flags 0x%X, 3D %u; dialect %u (%u); report %llu bytes; back buffer %ux%u format %u; profile %u; \"%s\"", name,
               (unsigned long long)nameSize, (unsigned long long)info.renderer_name_byte_length, (unsigned)info.renderer_type, (unsigned)info.max_texture_dimension,
               (unsigned)info.capability_flags, g->threeD, (unsigned)dialect, (unsigned)dialectResult, (unsigned long long)reportSize, (unsigned)backBuffer.width, (unsigned)backBuffer.height,
               (unsigned)backBuffer.format, (unsigned)profile, deviceType);
    CPROBE_CHECK(p, "cna_graphics_device_get_renderer_info/get_renderer_name_size/copy_renderer_name/get_shader_dialect_ext/get_capability_report_size_ext/copy_capability_report_ext/get_backbuffer_info/get_graphics_profile/copy_type_name",
                 "a named renderer whose sizes agree, at least XNA Reach's 2048 texels, a capability report, a non-empty Color back buffer, \"Microsoft.Xna.Framework.Graphics.GraphicsDevice\"",
                 named == CNA_RESULT_SUCCESS && nameBytes > 0u && nameSize == nameBytes && info.renderer_name_byte_length == nameBytes && info.max_texture_dimension >= 2048u &&
                     dialectResult == CNA_RESULT_SUCCESS && reportResult == CNA_RESULT_SUCCESS && reportBytes > 0u && reportSize == reportBytes && backBuffer.width > 0u && backBuffer.height > 0u &&
                     backBuffer.format == CNA_SURFACE_FORMAT_COLOR && profile <= CNA_GRAPHICS_PROFILE_HI_DEF && strcmp(deviceType, "Microsoft.Xna.Framework.Graphics.GraphicsDevice") == 0);
}

static void ViewportValues(GameProbe* g)
{
    CProbe* p = &g->probe;
    CNA_Viewport viewport;
    memset(&viewport, 0, sizeof viewport);
    cna_viewport_init_bounds(0, 0, 640, 480, &viewport);
    float aspect = 0.0f;
    cna_viewport_get_aspect_ratio(viewport, &aspect);
    const CNA_Matrix identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const CNA_Vector3 source = {0.5f, -0.5f, 0.25f};
    CNA_Vector3 projected = {0, 0, 0}, back = {0, 0, 0};
    cna_viewport_project(viewport, source, identity, identity, identity, &projected);
    cna_viewport_unproject(viewport, projected, identity, identity, identity, &back);
    char text[128];
    uint64_t bytes = 0;
    const CNA_Result copied = cna_viewport_copy_string(viewport, text, sizeof text - 1, &bytes);
    cprobe_terminate(text, sizeof text, copied, &bytes);
    CNA_Rectangle safe = {0, 0, 0, 0};
    cna_viewport_get_title_safe_area(viewport, &safe);
    cprobe_got(p, "aspect %.6f, project (%g, %g, %g), unproject (%g, %g, %g), \"%s\", title safe %d,%d %dx%d, depth %g..%g", (double)aspect, (double)projected.x, (double)projected.y,
               (double)projected.z, (double)back.x, (double)back.y, (double)back.z, text, (int)safe.x, (int)safe.y, (int)safe.width, (int)safe.height, (double)viewport.min_depth,
               (double)viewport.max_depth);
    CPROBE_CHECK(p, "cna_viewport_init_bounds/get_aspect_ratio/project/unproject/copy_string/get_title_safe_area (640x480)",
                 "1.333333; (0.5, -0.5, 0.25) projects to (480, 360, 0.25) and back; \"{X:0 Y:0 Width:640 Height:480 MinDepth:0 MaxDepth:1}\"; XNA's Windows title-safe area is the whole viewport",
                 fabsf(aspect - 4.0f / 3.0f) < 1e-6f && fabsf(projected.x - 480.0f) < 1e-3f && fabsf(projected.y - 360.0f) < 1e-3f && fabsf(projected.z - 0.25f) < 1e-6f &&
                     fabsf(back.x - 0.5f) < 1e-5f && fabsf(back.y + 0.5f) < 1e-5f && fabsf(back.z - 0.25f) < 1e-5f && strcmp(text, "{X:0 Y:0 Width:640 Height:480 MinDepth:0 MaxDepth:1}") == 0 &&
                     safe.x == 0 && safe.y == 0 && safe.width == 640 && safe.height == 480 && viewport.min_depth == 0.0f && viewport.max_depth == 1.0f);
}

static void StatePresets(GameProbe* g, CNA_Handle device)
{
    CProbe* p = &g->probe;
    CNA_BlendState opaque, nonPremultiplied, readBack;
    VERSIONED(opaque);
    VERSIONED(nonPremultiplied);
    VERSIONED(readBack);
    cna_blend_state_init(CNA_BLEND_STATE_PRESET_OPAQUE, &opaque);
    cna_blend_state_init(CNA_BLEND_STATE_PRESET_NON_PREMULTIPLIED, &nonPremultiplied);
    const CNA_Result setBlend = cna_graphics_device_set_blend_state(device, &nonPremultiplied);
    cna_graphics_device_get_blend_state(device, &readBack);
    CNA_SamplerState linearClamp, sampler;
    VERSIONED(linearClamp);
    VERSIONED(sampler);
    cna_sampler_state_init(CNA_SAMPLER_STATE_PRESET_LINEAR_CLAMP, &linearClamp);
    cna_graphics_device_set_sampler_state(device, CNA_SHADER_STAGE_PIXEL, 1u, &linearClamp);
    cna_graphics_device_get_sampler_state(device, CNA_SHADER_STAGE_PIXEL, 1u, &sampler);
    CNA_RasterizerState cullNone, raster;
    VERSIONED(cullNone);
    VERSIONED(raster);
    cna_rasterizer_state_init(CNA_RASTERIZER_STATE_PRESET_CULL_NONE, &cullNone);
    cna_graphics_device_set_rasterizer_state(device, &cullNone);
    cna_graphics_device_get_rasterizer_state(device, &raster);
    CNA_DepthStencilState none, depth;
    VERSIONED(none);
    VERSIONED(depth);
    cna_depth_stencil_state_init(CNA_DEPTH_STENCIL_STATE_PRESET_NONE, &none);
    cna_graphics_device_set_depth_stencil_state(device, &none);
    cna_graphics_device_get_depth_stencil_state(device, &depth);
    cna_graphics_device_set_blend_state(device, &opaque);
    cprobe_got(p, "Opaque src %u dst %u; NonPremultiplied src %u dst %u set %u, read back src %u dst %u; LinearClamp filter %u address %u/%u, slot 1 read back %u/%u; CullNone %u read back %u; None depth %u/%u",
               (unsigned)opaque.color_source_blend, (unsigned)opaque.color_destination_blend, (unsigned)nonPremultiplied.color_source_blend, (unsigned)nonPremultiplied.color_destination_blend,
               (unsigned)setBlend, (unsigned)readBack.color_source_blend, (unsigned)readBack.color_destination_blend, (unsigned)linearClamp.filter, (unsigned)linearClamp.address_u,
               (unsigned)linearClamp.address_v, (unsigned)sampler.filter, (unsigned)sampler.address_u, (unsigned)cullNone.cull_mode, (unsigned)raster.cull_mode, depth.depth_buffer_enable,
               depth.depth_buffer_write_enable);
    CPROBE_CHECK(p, "cna_blend_state_init/sampler_state_init/rasterizer_state_init/depth_stencil_state_init and cna_graphics_device_set+get_blend/sampler/rasterizer/depth_stencil_state",
                 "XNA presets: Opaque One/Zero, NonPremultiplied SourceAlpha/InverseSourceAlpha, LinearClamp Linear+Clamp, CullNone, None without depth; each read back as set",
                 opaque.color_source_blend == CNA_BLEND_ONE && opaque.color_destination_blend == CNA_BLEND_ZERO && nonPremultiplied.color_source_blend == CNA_BLEND_SOURCE_ALPHA &&
                     nonPremultiplied.color_destination_blend == CNA_BLEND_INVERSE_SOURCE_ALPHA && setBlend == CNA_RESULT_SUCCESS && readBack.color_source_blend == CNA_BLEND_SOURCE_ALPHA &&
                     readBack.color_destination_blend == CNA_BLEND_INVERSE_SOURCE_ALPHA && linearClamp.filter == CNA_TEXTURE_FILTER_LINEAR && linearClamp.address_u == CNA_TEXTURE_ADDRESS_CLAMP &&
                     sampler.filter == CNA_TEXTURE_FILTER_LINEAR && sampler.address_u == CNA_TEXTURE_ADDRESS_CLAMP && cullNone.cull_mode == CNA_CULL_NONE && raster.cull_mode == CNA_CULL_NONE &&
                     depth.depth_buffer_enable == CNA_FALSE && depth.depth_buffer_write_enable == CNA_FALSE);

    CNA_Rectangle scissor = {2, 3, 10, 9}, scissorBack = {0, 0, 0, 0};
    cna_graphics_device_set_scissor_rectangle(device, scissor);
    cna_graphics_device_get_scissor_rectangle(device, &scissorBack);
    CNA_Color factor = {1, 2, 3, 4}, factorBack = {0, 0, 0, 0};
    cna_graphics_device_set_blend_factor(device, factor);
    cna_graphics_device_get_blend_factor(device, &factorBack);
    int32_t stencil = 0, mask = 0;
    cna_graphics_device_set_reference_stencil(device, 5);
    cna_graphics_device_get_reference_stencil(device, &stencil);
    cna_graphics_device_set_multi_sample_mask(device, 0x0F);
    cna_graphics_device_get_multi_sample_mask(device, &mask);
    cna_graphics_device_set_multi_sample_mask(device, -1);
    cprobe_got(p, "scissor %d,%d %dx%d, blend factor (%u, %u, %u, %u), reference stencil %d, multisample mask 0x%X", (int)scissorBack.x, (int)scissorBack.y, (int)scissorBack.width,
               (int)scissorBack.height, factorBack.r, factorBack.g, factorBack.b, factorBack.a, (int)stencil, (unsigned)mask);
    CPROBE_CHECK(p, "cna_graphics_device_set+get_scissor_rectangle/blend_factor/reference_stencil/multi_sample_mask", "every value read back as set: 2,3 10x9, (1, 2, 3, 4), 5, 0xF",
                 scissorBack.x == 2 && scissorBack.y == 3 && scissorBack.width == 10 && scissorBack.height == 9 && Same(factorBack, factor) && stencil == 5 && mask == 0x0F);
}

static CNA_Handle MakeTarget(GameProbe* g, CNA_Handle device)
{
    CProbe* p = &g->probe;
    CNA_RenderTarget2DCreateInfo create;
    VERSIONED(create);
    create.width = TARGET_SIZE;
    create.height = TARGET_SIZE;
    create.format = CNA_SURFACE_FORMAT_COLOR;
    create.depth_format = CNA_DEPTH_FORMAT_NONE;
    create.usage = CNA_RENDER_TARGET_USAGE_PRESERVE_CONTENTS;
    CNA_Handle target = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_render_target2d_create(device, &create, &target);
    CNA_RenderTargetInfo info;
    VERSIONED(info);
    cna_render_target_get_info(target, &info);
    CNA_Bool preserves = CNA_FALSE, discards = CNA_TRUE;
    cna_render_target_usage_preserves_contents(CNA_RENDER_TARGET_USAGE_PRESERVE_CONTENTS, &preserves);
    cna_render_target_usage_preserves_contents(CNA_RENDER_TARGET_USAGE_DISCARD_CONTENTS, &discards);
    const CNA_Result bound = cna_graphics_device_set_render_target2d(device, target);
    uint64_t bindings = 0, copiedBindings = 0;
    cna_graphics_device_get_render_target_count(device, &bindings);
    CNA_RenderTargetBinding binding[2];
    memset(binding, 0, sizeof binding);
    for (int i = 0; i < 2; ++i) {
        binding[i].struct_size = (uint32_t)sizeof binding[i];
        binding[i].struct_version = 1u;
    }
    cna_graphics_device_copy_render_targets(device, binding, 2u, &copiedBindings);
    cprobe_got(p, "create %u: kind %u, %ux%u, %u level(s), format %u, depth %u, usage %u, renderer storage %u; PreserveContents preserves %u, DiscardContents %u; bind %u, %llu binding(s), first %s",
               (unsigned)created, (unsigned)info.kind, (unsigned)info.width, (unsigned)info.height, (unsigned)info.level_count, (unsigned)info.format, (unsigned)info.depth_format,
               (unsigned)info.usage, info.renderer_available, preserves, discards, (unsigned)bound, (unsigned long long)bindings,
               copiedBindings == 1u && binding[0].render_target == target ? "the target" : "something else");
    CPROBE_CHECK(p, "cna_render_target2d_create/get_info, cna_render_target_usage_preserves_contents, cna_graphics_device_set_render_target2d/get_render_target_count/copy_render_targets",
                 "a 16x16 Color 2D target with one level, no depth, PreserveContents and real storage; PreserveContents preserves, DiscardContents does not; bound as the only binding",
                 created == CNA_RESULT_SUCCESS && info.kind == CNA_RENDER_TARGET_KIND_2D && info.width == TARGET_SIZE && info.height == TARGET_SIZE && info.level_count == 1u &&
                     info.format == CNA_SURFACE_FORMAT_COLOR && info.depth_format == CNA_DEPTH_FORMAT_NONE && info.usage == CNA_RENDER_TARGET_USAGE_PRESERVE_CONTENTS &&
                     info.renderer_available == CNA_TRUE && preserves == CNA_TRUE && discards == CNA_FALSE && bound == CNA_RESULT_SUCCESS && bindings == 1u && copiedBindings == 1u &&
                     binding[0].render_target == target);
    return target;
}

static void DrawAndReadBack(GameProbe* g, CNA_Handle device, CNA_Handle target)
{
    CProbe* p = &g->probe;
    static CNA_Color pixels[TARGET_SIZE * TARGET_SIZE];

    /* A clear, read back whole. */
    const CNA_Color clearColor = {10, 200, 30, 255};
    cna_graphics_device_set_render_target2d(device, target);
    CNA_Viewport viewport;
    cna_viewport_init_bounds(0, 0, TARGET_SIZE, TARGET_SIZE, &viewport);
    cna_graphics_device_set_viewport(device, viewport); /* the whole 16x16 target */
    CNA_Viewport viewportBack;
    memset(&viewportBack, 0, sizeof viewportBack);
    cna_graphics_device_get_viewport(device, &viewportBack);
    const CNA_Result cleared = cna_graphics_device_clear_options(device, CNA_CLEAR_OPTION_TARGET, clearColor, 1.0f, 0);
    const CNA_Result read = ReadTarget(device, target, pixels);
    int matching = 0;
    for (int i = 0; i < TARGET_SIZE * TARGET_SIZE; ++i) matching += Same(pixels[i], clearColor);
    cprobe_got(p, "viewport %dx%d, clear %u, read %u: %d of 256 pixels (10, 200, 30, 255), [0] (%u, %u, %u, %u)", (int)viewportBack.width, (int)viewportBack.height, (unsigned)cleared,
               (unsigned)read, matching, pixels[0].r, pixels[0].g, pixels[0].b, pixels[0].a);
    CPROBE_CHECK(p, "cna_graphics_device_set+get_viewport, clear_options(TARGET), cna_texture2d_get_data_rgba8 on the render target", "a 16x16 viewport; all 256 pixels exactly the clear colour",
                 viewportBack.width == TARGET_SIZE && viewportBack.height == TARGET_SIZE && cleared == CNA_RESULT_SUCCESS && read == CNA_RESULT_SUCCESS && matching == TARGET_SIZE * TARGET_SIZE);

    /* User primitives through BasicEffect: a red triangle over the lower-left half of a blue target. */
    const CNA_Matrix identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    CNA_EffectHandle effect = CNA_INVALID_HANDLE;
    const CNA_Result effectCreated = cna_basic_effect_create(device, &effect);
    cna_basic_effect_set_vertex_color_enabled(effect, CNA_TRUE);
    cna_effect_matrices_set_world(effect, identity);
    cna_effect_matrices_set_view(effect, identity);
    cna_effect_matrices_set_projection(effect, identity);
    const CNA_VertexPositionColor triangle[3] = {{{-1.0f, -1.0f, 0.0f}, {255, 0, 0, 255}}, {{1.0f, -1.0f, 0.0f}, {255, 0, 0, 255}}, {{-1.0f, 1.0f, 0.0f}, {255, 0, 0, 255}}};
    cna_graphics_device_set_render_target2d(device, target);
    cna_graphics_device_set_viewport(device, viewport);
    const CNA_Result blueClear = cna_graphics_device_clear_options(device, CNA_CLEAR_OPTION_TARGET, (CNA_Color){0, 0, 255, 255}, 1.0f, 0);
    CNA_RasterizerState drawCullNone;
    VERSIONED(drawCullNone);
    cna_rasterizer_state_init(CNA_RASTERIZER_STATE_PRESET_CULL_NONE, &drawCullNone);
    cna_graphics_device_set_rasterizer_state(device, &drawCullNone);
    CNA_DepthStencilState noDepth;
    VERSIONED(noDepth);
    cna_depth_stencil_state_init(CNA_DEPTH_STENCIL_STATE_PRESET_NONE, &noDepth);
    cna_graphics_device_set_depth_stencil_state(device, &noDepth);
    const CNA_Result applied = cna_effect_apply(effect);
    CNA_UserPrimitives primitives;
    VERSIONED(primitives);
    primitives.primitive_type = CNA_PRIMITIVE_TRIANGLE_LIST;
    primitives.vertex_source = CNA_USER_VERTEX_SOURCE_POSITION_COLOR;
    primitives.vertex_data = triangle;
    primitives.primitive_count = 1;
    const CNA_Result drawn = cna_graphics_device_draw_user_primitives(device, &primitives);
    int32_t vertexCount = 0;
    cna_primitive_type_get_vertex_count(CNA_PRIMITIVE_TRIANGLE_STRIP, 2, &vertexCount);

    /* Then a green quad over the right half through a vertex buffer and a 16-bit index buffer. */
    CNA_VertexElement elements[4];
    uint64_t elementCount = 0;
    uint32_t stride = 0;
    cna_vertex_type_get_stride(CNA_VERTEX_TYPE_POSITION_COLOR, &stride);
    cna_vertex_type_copy_elements(CNA_VERTEX_TYPE_POSITION_COLOR, elements, 4u, &elementCount);
    CNA_VertexDeclarationHandle declaration = CNA_INVALID_HANDLE;
    const CNA_Result declared = cna_vertex_declaration_create_with_stride((int32_t)stride, elements, elementCount, &declaration);
    int32_t declarationStride = 0;
    cna_vertex_declaration_get_stride(declaration, &declarationStride);
    CNA_VertexBufferCreateInfo bufferInfo;
    VERSIONED(bufferInfo);
    bufferInfo.vertex_declaration = declaration;
    bufferInfo.vertex_count = 4;
    bufferInfo.buffer_usage = CNA_BUFFER_USAGE_NONE;
    CNA_VertexBufferHandle vertices = CNA_INVALID_HANDLE;
    const CNA_Result bufferCreated = cna_vertex_buffer_create(device, &bufferInfo, &vertices);
    const CNA_VertexPositionColor quad[4] = {{{0.0f, -1.0f, 0.0f}, {0, 255, 0, 255}}, {{1.0f, -1.0f, 0.0f}, {0, 255, 0, 255}}, {{0.0f, 1.0f, 0.0f}, {0, 255, 0, 255}}, {{1.0f, 1.0f, 0.0f}, {0, 255, 0, 255}}};
    CNA_VertexBufferTransfer transfer;
    VERSIONED(transfer);
    transfer.vertex_type = CNA_VERTEX_TYPE_POSITION_COLOR;
    transfer.element_count = 4u;
    const CNA_Result vertexSet = cna_vertex_buffer_set_data(vertices, &transfer, quad, 4u);
    CNA_VertexPositionColor quadBack[4];
    memset(quadBack, 0, sizeof quadBack);
    uint64_t quadBackCount = 0;
    const CNA_Result vertexGet = cna_vertex_buffer_get_data(vertices, &transfer, quadBack, 4u, &quadBackCount);
    CNA_VertexBufferInfo vertexInfo;
    VERSIONED(vertexInfo);
    cna_vertex_buffer_get_info(vertices, &vertexInfo);
    CNA_IndexBufferCreateInfo indexInfo;
    VERSIONED(indexInfo);
    indexInfo.index_count = 6;
    indexInfo.index_element_size = CNA_INDEX_ELEMENT_SIZE_SIXTEEN_BITS;
    indexInfo.buffer_usage = CNA_BUFFER_USAGE_NONE;
    CNA_IndexBufferHandle indices = CNA_INVALID_HANDLE;
    const CNA_Result indexCreated = cna_index_buffer_create(device, &indexInfo, &indices);
    const uint16_t quadIndices[6] = {0, 1, 2, 2, 1, 3};
    CNA_IndexBufferTransfer indexTransfer;
    VERSIONED(indexTransfer);
    indexTransfer.index_element_size = CNA_INDEX_ELEMENT_SIZE_SIXTEEN_BITS;
    indexTransfer.element_count = 6u;
    const CNA_Result indexSet = cna_index_buffer_set_data(indices, &indexTransfer, quadIndices, 6u);
    uint16_t indicesBack[6] = {9, 9, 9, 9, 9, 9};
    uint64_t indicesBackCount = 0;
    const CNA_Result indexGet = cna_index_buffer_get_data(indices, &indexTransfer, indicesBack, 6u, &indicesBackCount);
    cna_graphics_device_set_vertex_buffer(device, vertices);
    cna_graphics_device_set_index_buffer(device, indices);
    CNA_IndexBufferHandle boundIndices = CNA_INVALID_HANDLE;
    cna_graphics_device_get_index_buffer(device, &boundIndices);
    cna_effect_apply(effect);
    const CNA_Result indexedDrawn = cna_graphics_device_draw_indexed_primitives(device, CNA_PRIMITIVE_TRIANGLE_LIST, 0, 0, 4, 0, 2);
    const CNA_Result drawnRead = ReadTarget(device, target, pixels);
    const CNA_Color red = {255, 0, 0, 255}, green = {0, 255, 0, 255}, blue = {0, 0, 255, 255};
    const CNA_Color lowerLeft = Pixel(pixels, 1, 14), right = Pixel(pixels, 14, 8), upperLeft = Pixel(pixels, 4, 1);
    cprobe_got(p, "blue clear %u; effect %u apply %u, user draw %u (strip of 2 = %d vertices); declaration %u stride %d, vertex buffer %u set %u get %u (%llu, %s), stride %d; index buffer %u set %u get %u (%llu, %u %u %u %u %u %u), bound %s; indexed draw %u; read %u: (1,14) (%u,%u,%u,%u) (14,8) (%u,%u,%u,%u) (4,1) (%u,%u,%u,%u)",
               (unsigned)blueClear, (unsigned)effectCreated, (unsigned)applied, (unsigned)drawn, (int)vertexCount, (unsigned)declared, (int)declarationStride, (unsigned)bufferCreated, (unsigned)vertexSet,
               (unsigned)vertexGet, (unsigned long long)quadBackCount, memcmp(quad, quadBack, sizeof quad) == 0 ? "same" : "different", (int)vertexInfo.vertex_stride, (unsigned)indexCreated,
               (unsigned)indexSet, (unsigned)indexGet, (unsigned long long)indicesBackCount, indicesBack[0], indicesBack[1], indicesBack[2], indicesBack[3], indicesBack[4], indicesBack[5],
               boundIndices == indices ? "yes" : "no", (unsigned)indexedDrawn, (unsigned)drawnRead, lowerLeft.r, lowerLeft.g, lowerLeft.b, lowerLeft.a, right.r, right.g, right.b, right.a,
               upperLeft.r, upperLeft.g, upperLeft.b, upperLeft.a);
    CPROBE_CHECK(p, "cna_basic_effect_create/set_vertex_color_enabled, cna_effect_matrices_set_world/view/projection, cna_effect_apply, cna_graphics_device_draw_user_primitives/draw_indexed_primitives, cna_vertex_declaration_create_with_stride, cna_vertex_buffer_create/set_data/get_data/get_info, cna_index_buffer_create/set_data/get_data, cna_graphics_device_set_vertex_buffer/set_index_buffer/get_index_buffer, cna_primitive_type_get_vertex_count",
                 "the buffers read back what was written (16-byte stride); a strip of 2 has 4 vertices; the red triangle covers (1,14), the green indexed quad (14,8), the blue clear stays at (4,1)",
                 blueClear == CNA_RESULT_SUCCESS && effectCreated == CNA_RESULT_SUCCESS && applied == CNA_RESULT_SUCCESS && drawn == CNA_RESULT_SUCCESS && vertexCount == 4 && declared == CNA_RESULT_SUCCESS && declarationStride == 16 &&
                     bufferCreated == CNA_RESULT_SUCCESS && vertexSet == CNA_RESULT_SUCCESS && vertexGet == CNA_RESULT_SUCCESS && quadBackCount == 4u && memcmp(quad, quadBack, sizeof quad) == 0 &&
                     vertexInfo.vertex_stride == 16 && indexCreated == CNA_RESULT_SUCCESS && indexSet == CNA_RESULT_SUCCESS && indexGet == CNA_RESULT_SUCCESS && indicesBackCount == 6u &&
                     memcmp(quadIndices, indicesBack, sizeof quadIndices) == 0 && boundIndices == indices && indexedDrawn == CNA_RESULT_SUCCESS && drawnRead == CNA_RESULT_SUCCESS &&
                     Same(lowerLeft, red) && Same(right, green) && Same(upperLeft, blue));
    cna_graphics_device_set_vertex_buffer(device, CNA_INVALID_HANDLE);
    cna_graphics_device_set_index_buffer(device, CNA_INVALID_HANDLE);
    /* XNA: GetData on a BufferUsage.WriteOnly buffer throws NotSupportedException. */
    bufferInfo.buffer_usage = CNA_BUFFER_USAGE_WRITE_ONLY;
    CNA_VertexBufferHandle writeOnly = CNA_INVALID_HANDLE;
    cna_vertex_buffer_create(device, &bufferInfo, &writeOnly);
    cna_vertex_buffer_set_data(writeOnly, &transfer, quad, 4u);
    uint64_t writeOnlyCount = 0;
    const CNA_Result writeOnlyGet = cna_vertex_buffer_get_data(writeOnly, &transfer, quadBack, 4u, &writeOnlyCount);
    cna_vertex_buffer_destroy(writeOnly);
    indexInfo.buffer_usage = CNA_BUFFER_USAGE_WRITE_ONLY;
    CNA_IndexBufferHandle writeOnlyIndices = CNA_INVALID_HANDLE;
    cna_index_buffer_create(device, &indexInfo, &writeOnlyIndices);
    cna_index_buffer_set_data(writeOnlyIndices, &indexTransfer, quadIndices, 6u);
    uint64_t writeOnlyIndexCount = 0;
    const CNA_Result writeOnlyIndexGet = cna_index_buffer_get_data(writeOnlyIndices, &indexTransfer, indicesBack, 6u, &writeOnlyIndexCount);
    cna_index_buffer_destroy(writeOnlyIndices);
    cprobe_got(p, "vertex buffer get_data %u, index buffer get_data %u", (unsigned)writeOnlyGet, (unsigned)writeOnlyIndexGet);
    CPROBE_CHECK(p, "cna_vertex_buffer_get_data / cna_index_buffer_get_data on BufferUsage.WriteOnly buffers", "CNA_RESULT_NOT_SUPPORTED twice (XNA throws NotSupportedException)",
                 writeOnlyGet == CNA_RESULT_NOT_SUPPORTED && writeOnlyIndexGet == CNA_RESULT_NOT_SUPPORTED);
    cna_index_buffer_destroy(indices);
    cna_vertex_buffer_destroy(vertices);
    cna_vertex_declaration_destroy(declaration);
    cna_effect_destroy(effect);

    /* SpriteBatch: a 1x1 magenta texture stretched over the whole target. */
    const CNA_Color magenta = {255, 0, 255, 255};
    CNA_Handle dot = CNA_INVALID_HANDLE;
    const CNA_Result dotCreated = cna_texture2d_create_from_rgba8(device, 1u, 1u, &magenta, 1u, &dot);
    CNA_Handle batch = CNA_INVALID_HANDLE;
    const CNA_Result batchCreated = cna_sprite_batch_create(device, &batch);
    cna_graphics_device_set_render_target2d(device, target);
    cna_graphics_device_clear_options(device, CNA_CLEAR_OPTION_TARGET, (CNA_Color){0, 0, 0, 255}, 1.0f, 0);
    CNA_SpriteBatchBeginInfo begin;
    VERSIONED(begin);
    begin.sort_mode = CNA_SPRITE_SORT_MODE_DEFERRED;
    const CNA_Result begun = cna_sprite_batch_begin(batch, &begin);
    CNA_SpriteCommand command;
    VERSIONED(command);
    command.texture = dot;
    command.destination = (CNA_Rectangle){0, 0, TARGET_SIZE, TARGET_SIZE};
    command.source = (CNA_Rectangle){0, 0, 1, 1};
    command.color = (CNA_Color){255, 255, 255, 255};
    command.effects = CNA_SPRITE_EFFECT_NONE;
    const CNA_Result submitted = cna_sprite_batch_submit_many(batch, &command, 1u);
    char submitMessage[200];
    submitMessage[0] = '\0';
    if (submitted != CNA_RESULT_SUCCESS) cprobe_last_message(submitMessage, sizeof submitMessage);
    const CNA_Result ended = cna_sprite_batch_end(batch);
    const CNA_Result spriteRead = ReadTarget(device, target, pixels);
    int magentaPixels = 0;
    for (int i = 0; i < TARGET_SIZE * TARGET_SIZE; ++i) magentaPixels += Same(pixels[i], magenta);
    char batchType[96];
    uint64_t batchTypeBytes = 0;
    const CNA_Result batchTypeResult = cna_sprite_batch_copy_type_name(batch, batchType, sizeof batchType - 1, &batchTypeBytes);
    cprobe_terminate(batchType, sizeof batchType, batchTypeResult, &batchTypeBytes);
    g->spriteBatch = batch;
    cna_texture2d_destroy(dot);
    cprobe_got(p, "texture %u, batch %u, begin %u, submit %u \"%s\", end %u, read %u: %d of 256 magenta; \"%s\"", (unsigned)dotCreated, (unsigned)batchCreated, (unsigned)begun, (unsigned)submitted,
               submitMessage, (unsigned)ended, (unsigned)spriteRead, magentaPixels, batchType);
    CPROBE_CHECK(p, "cna_texture2d_create_from_rgba8, cna_sprite_batch_create/begin/submit_many/end/copy_type_name", "the stretched 1x1 magenta texture covers all 256 pixels; \"Microsoft.Xna.Framework.Graphics.SpriteBatch\"",
                 dotCreated == CNA_RESULT_SUCCESS && batchCreated == CNA_RESULT_SUCCESS && begun == CNA_RESULT_SUCCESS && submitted == CNA_RESULT_SUCCESS && ended == CNA_RESULT_SUCCESS &&
                     spriteRead == CNA_RESULT_SUCCESS && magentaPixels == TARGET_SIZE * TARGET_SIZE && strcmp(batchType, "Microsoft.Xna.Framework.Graphics.SpriteBatch") == 0);
}

static void TextureChecks(GameProbe* g, CNA_Handle device)
{
    CProbe* p = &g->probe;
    const CNA_Color four[4] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}, {255, 255, 255, 128}};
    CNA_Texture2DCreateInfo create;
    VERSIONED(create);
    create.width = 2u;
    create.height = 2u;
    create.format = CNA_SURFACE_FORMAT_COLOR;
    CNA_Handle texture = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_texture2d_create(device, &create, &texture);
    const CNA_Result set = cna_texture2d_set_data_rgba8(texture, four, 4u);
    CNA_Color back[4];
    memset(back, 0, sizeof back);
    uint64_t backCount = 0;
    const CNA_Result got = cna_texture2d_get_data_rgba8(texture, back, 4u, &backCount);
    CNA_Texture2DInfo info;
    VERSIONED(info);
    cna_texture2d_get_info(texture, &info);
    int32_t formatSize = 0;
    cna_texture_get_format_size(CNA_SURFACE_FORMAT_COLOR, &formatSize);
    char typeName[96];
    uint64_t typeBytes = 0;
    const CNA_Result typed = cna_texture2d_copy_type_name(texture, typeName, sizeof typeName - 1, &typeBytes);
    cprobe_terminate(typeName, sizeof typeName, typed, &typeBytes);
    static uint8_t png[4096];
    uint64_t pngBytes = 0, pngCount = 0;
    cna_texture2d_get_encoded_byte_count(texture, CNA_TEXTURE_IMAGE_FORMAT_PNG, 2u, 2u, &pngCount);
    const CNA_Result encoded = cna_texture2d_copy_encoded(texture, CNA_TEXTURE_IMAGE_FORMAT_PNG, 2u, 2u, png, sizeof png, &pngBytes);
    CNA_Handle decoded = CNA_INVALID_HANDLE;
    const CNA_Result decodedResult = cna_texture2d_create_from_encoded_memory(device, png, pngBytes, NULL, &decoded);
    CNA_Color decodedPixels[4];
    memset(decodedPixels, 0, sizeof decodedPixels);
    uint64_t decodedCount = 0;
    cna_texture2d_get_data_rgba8(decoded, decodedPixels, 4u, &decodedCount);
    cna_texture2d_destroy(decoded);
    const int pngSignature = pngBytes > 8u && png[0] == 0x89 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G';
    cprobe_got(p, "create %u, set %u, get %u (%llu pixels, %s), %ux%u levels %u format %u, Color is %d bytes, \"%s\"; PNG %u: %llu bytes (count %llu, signature %s), decoded %u: %s", (unsigned)created,
               (unsigned)set, (unsigned)got, (unsigned long long)backCount, memcmp(back, four, sizeof four) == 0 ? "same" : "different", (unsigned)info.width, (unsigned)info.height,
               (unsigned)info.level_count, (unsigned)info.format, (int)formatSize, typeName, (unsigned)encoded, (unsigned long long)pngBytes, (unsigned long long)pngCount, pngSignature ? "yes" : "no",
               (unsigned)decodedResult, memcmp(decodedPixels, four, sizeof four) == 0 ? "the same four pixels" : "different pixels");
    CPROBE_CHECK(p, "cna_texture2d_create/set_data_rgba8/get_data_rgba8/get_info/copy_type_name, cna_texture_get_format_size, cna_texture2d_get_encoded_byte_count/copy_encoded(PNG), cna_texture2d_create_from_encoded_memory",
                 "the four pixels (one half-transparent) back exactly; 2x2, 1 level, Color = 4 bytes; \"Microsoft.Xna.Framework.Graphics.Texture2D\"; a PNG (signature 89 'PNG') that decodes to the same pixels",
                 created == CNA_RESULT_SUCCESS && set == CNA_RESULT_SUCCESS && got == CNA_RESULT_SUCCESS && backCount == 4u && memcmp(back, four, sizeof four) == 0 && info.width == 2u &&
                     info.height == 2u && info.level_count == 1u && info.format == CNA_SURFACE_FORMAT_COLOR && formatSize == 4 &&
                     strcmp(typeName, "Microsoft.Xna.Framework.Graphics.Texture2D") == 0 && encoded == CNA_RESULT_SUCCESS && pngCount == pngBytes && pngSignature &&
                     decodedResult == CNA_RESULT_SUCCESS && memcmp(decodedPixels, four, sizeof four) == 0);

    /* The GraphicsResource contract on that texture: name, ToString, tag, owning device, disposal and its event. */
    cna_graphics_resource_set_name(texture, cprobe_view("probe-texture"));
    char name[32], text[96];
    uint64_t nameBytes = 0, textBytes = 0, unnamedBytes = 0;
    const CNA_Result nameResult = cna_graphics_resource_copy_name(texture, name, sizeof name - 1, &nameBytes);
    cprobe_terminate(name, sizeof name, nameResult, &nameBytes);
    const CNA_Result textResult = cna_graphics_resource_copy_string(texture, text, sizeof text - 1, &textBytes);
    cprobe_terminate(text, sizeof text, textResult, &textBytes);
    cna_graphics_resource_set_tag(texture, UINT64_C(0xC0FFEE));
    CNA_GraphicsResourceTag tag = 0;
    cna_graphics_resource_get_tag(texture, &tag);
    CNA_Handle owner = CNA_INVALID_HANDLE;
    cna_graphics_resource_get_graphics_device(texture, &owner);
    cna_graphics_resource_set_name(texture, cprobe_view(""));
    char unnamed[96];
    const CNA_Result unnamedResult = cna_graphics_resource_copy_string(texture, unnamed, sizeof unnamed - 1, &unnamedBytes);
    cprobe_terminate(unnamed, sizeof unnamed, unnamedResult, &unnamedBytes);
    CNA_GraphicsResourceEventRegistrationHandle registration = CNA_INVALID_HANDLE;
    disposingCalls = 0;
    cna_graphics_resource_subscribe_disposing(texture, OnDisposing, NULL, &registration);
    CNA_Bool disposedBefore = CNA_TRUE, disposedAfter = CNA_FALSE;
    cna_graphics_resource_get_is_disposed(texture, &disposedBefore);
    const CNA_Result disposed = cna_graphics_resource_dispose(texture);
    const CNA_Result disposedAgain = cna_graphics_resource_dispose(texture);
    cna_graphics_resource_get_is_disposed(texture, &disposedAfter);
    cna_graphics_resource_unsubscribe_disposing(registration);
    const CNA_Result destroyed = cna_texture2d_destroy(texture);
    const CNA_Result destroyedAgain = cna_texture2d_destroy(texture);
    cprobe_got(p, "name \"%s\", string \"%s\", tag 0x%llX, owner %s, unnamed string \"%s\"; disposed %u -> dispose %u, again %u, disposing events %d, disposed %u; destroy %u, again %u", name, text,
               (unsigned long long)tag, owner == device ? "this device" : "another handle", unnamed, disposedBefore, (unsigned)disposed, (unsigned)disposedAgain, disposingCalls, disposedAfter,
               (unsigned)destroyed, (unsigned)destroyedAgain);
    CPROBE_CHECK(p, "cna_graphics_resource_set_name/copy_name/copy_string/set_tag/get_tag/get_graphics_device/subscribe_disposing/get_is_disposed/dispose/unsubscribe_disposing, cna_texture2d_destroy",
                 "the name, ToString = the name (the type name when unnamed), the tag, the borrowed device; one disposing event, a repeated dispose is a no-op; destroy once, then INVALID_HANDLE",
                 strcmp(name, "probe-texture") == 0 && strcmp(text, "probe-texture") == 0 && tag == UINT64_C(0xC0FFEE) && owner == device &&
                     strcmp(unnamed, "Microsoft.Xna.Framework.Graphics.Texture2D") == 0 && disposedBefore == CNA_FALSE && disposed == CNA_RESULT_SUCCESS && disposedAgain == CNA_RESULT_SUCCESS &&
                     disposingCalls == 1 && disposedAfter == CNA_TRUE && destroyed == CNA_RESULT_SUCCESS && destroyedAgain == CNA_RESULT_INVALID_HANDLE);
}

void gp_graphics(GameProbe* g, CNA_Handle game, CNA_Handle device)
{
    (void)game;
    RendererIdentity(g, device);
    ViewportValues(g);
    StatePresets(g, device);
    TextureChecks(g, device);
    if (!g->threeD) {
        game_probe_skip(g, "render target, BasicEffect, buffers and SpriteBatch drawn and read back", "the active renderer reports no 3D capability");
        return;
    }
    g->target = MakeTarget(g, device);
    DrawAndReadBack(g, device, g->target);
}

static void VolumeChecks(GameProbe* g, CNA_Handle device)
{
    CProbe* p = &g->probe;
    CNA_GraphicsProfile volumeProfile = CNA_GRAPHICS_PROFILE_REACH;
    cna_graphics_device_get_graphics_profile(device, &volumeProfile);
    CNA_Color volume[8];
    for (int i = 0; i < 8; ++i) volume[i] = (CNA_Color){(uint8_t)(i * 30), (uint8_t)(255 - i * 30), (uint8_t)i, 255};
    CNA_Texture3DCreateInfo create3d;
    VERSIONED(create3d);
    create3d.width = 2u;
    create3d.height = 2u;
    create3d.depth = 2u;
    create3d.format = CNA_SURFACE_FORMAT_COLOR;
    CNA_Handle texture3d = CNA_INVALID_HANDLE;
    const CNA_Result created3d = cna_texture3d_create(device, &create3d, &texture3d);
    CNA_Texture3DTransfer transfer3d;
    VERSIONED(transfer3d);
    transfer3d.right = 2;
    transfer3d.bottom = 2;
    transfer3d.back = 2;
    transfer3d.element_count = 8u;
    const CNA_Result set3d = cna_texture3d_set_data(texture3d, &transfer3d, volume, 8u);
    CNA_Color volumeBack[8];
    memset(volumeBack, 0, sizeof volumeBack);
    uint64_t required3d = 0;
    const CNA_Result get3d = cna_texture3d_get_data(texture3d, &transfer3d, volumeBack, 8u, &required3d);
    CNA_Texture3DInfo info3d;
    VERSIONED(info3d);
    cna_texture3d_get_info(texture3d, &info3d);
    char type3d[96];
    uint64_t type3dBytes = 0;
    const CNA_Result typed3d = cna_texture3d_copy_type_name(texture3d, type3d, sizeof type3d - 1, &type3dBytes);
    cprobe_terminate(type3d, sizeof type3d, typed3d, &type3dBytes);
    if (created3d == CNA_RESULT_SUCCESS) cna_texture3d_destroy(texture3d);

    const CNA_Color face[4] = {{1, 2, 3, 255}, {4, 5, 6, 255}, {7, 8, 9, 255}, {10, 11, 12, 255}};
    CNA_TextureCubeCreateInfo createCube;
    VERSIONED(createCube);
    createCube.size = 2u;
    createCube.format = CNA_SURFACE_FORMAT_COLOR;
    CNA_Handle cube = CNA_INVALID_HANDLE;
    const CNA_Result createdCube = cna_texturecube_create(device, &createCube, &cube);
    CNA_TextureCubeTransfer transferCube;
    VERSIONED(transferCube);
    transferCube.face = CNA_CUBE_MAP_FACE_NEGATIVE_Z;
    transferCube.element_count = 4u;
    const CNA_Result setCube = cna_texturecube_set_data(cube, &transferCube, face, 4u);
    CNA_Color faceBack[4];
    memset(faceBack, 0, sizeof faceBack);
    uint64_t requiredCube = 0;
    const CNA_Result getCube = cna_texturecube_get_data(cube, &transferCube, faceBack, 4u, &requiredCube);
    char typeCube[96];
    uint64_t typeCubeBytes = 0;
    const CNA_Result typedCube = cna_texturecube_copy_type_name(cube, typeCube, sizeof typeCube - 1, &typeCubeBytes);
    cprobe_terminate(typeCube, sizeof typeCube, typedCube, &typeCubeBytes);
    if (createdCube == CNA_RESULT_SUCCESS) cna_texturecube_destroy(cube);
    cprobe_got(p, "profile %u; Texture3D create %u set %u get %u (%s) %ux%ux%u \"%s\"; TextureCube create %u set %u get %u (%s) \"%s\"", (unsigned)volumeProfile, (unsigned)created3d, (unsigned)set3d,
               (unsigned)get3d, memcmp(volume, volumeBack, sizeof volume) == 0 ? "same" : "different", (unsigned)info3d.width, (unsigned)info3d.height, (unsigned)info3d.depth, type3d,
               (unsigned)createdCube, (unsigned)setCube, (unsigned)getCube, memcmp(face, faceBack, sizeof face) == 0 ? "same" : "different", typeCube);
    const int volumeOk = volumeProfile == CNA_GRAPHICS_PROFILE_HI_DEF
                             ? (created3d == CNA_RESULT_SUCCESS && set3d == CNA_RESULT_SUCCESS && get3d == CNA_RESULT_SUCCESS && memcmp(volume, volumeBack, sizeof volume) == 0 &&
                                info3d.depth == 2u && strcmp(type3d, "Microsoft.Xna.Framework.Graphics.Texture3D") == 0)
                             : created3d == CNA_RESULT_NOT_SUPPORTED;
    CPROBE_CHECK(p, "cna_texture3d_create/set_data/get_data/get_info/copy_type_name/destroy, cna_texturecube_create/set_data/get_data/copy_type_name/destroy",
                 "HiDef: a 2x2x2 volume and a cube face (NegativeZ) read back exactly, \"Microsoft.Xna.Framework.Graphics.Texture3D\" and \"...TextureCube\" (Reach refuses Texture3D as XNA does)",
                 volumeOk && createdCube == CNA_RESULT_SUCCESS && setCube == CNA_RESULT_SUCCESS && getCube == CNA_RESULT_SUCCESS && memcmp(face, faceBack, sizeof face) == 0 &&
                     strcmp(typeCube, "Microsoft.Xna.Framework.Graphics.TextureCube") == 0);
}

static void DisplayChecks(GameProbe* g, CNA_Handle device)
{
    CProbe* p = &g->probe;
    CNA_DisplayMode mode, same, current;
    VERSIONED(mode);
    VERSIONED(same);
    VERSIONED(current);
    cna_display_mode_init(800, 600, CNA_SURFACE_FORMAT_COLOR, &mode);
    cna_display_mode_init(800, 600, CNA_SURFACE_FORMAT_COLOR, &same);
    CNA_Bool equal = CNA_FALSE;
    cna_display_mode_equals(&mode, &same, &equal);
    uint64_t adapters = 0;
    cna_graphics_adapter_get_count(device, &adapters);
    CNA_GraphicsAdapterInfo adapter;
    VERSIONED(adapter);
    const CNA_Result adapterResult = cna_graphics_adapter_get_info(device, 0u, &adapter);
    char description[256];
    uint64_t descriptionBytes = 0;
    const CNA_Result described = cna_graphics_adapter_copy_description(device, 0u, description, sizeof description - 1, &descriptionBytes);
    cprobe_terminate(description, sizeof description, described, &descriptionBytes);
    const CNA_Result currentResult = cna_graphics_adapter_get_current_display_mode(device, 0u, &current);
    CNA_Bool reach = CNA_FALSE;
    cna_graphics_adapter_is_profile_supported(device, 0u, CNA_GRAPHICS_PROFILE_REACH, &reach);
    CNA_PresentationParameters defaults, deviceParameters;
    VERSIONED(defaults);
    VERSIONED(deviceParameters);
    cna_presentation_parameters_init(&defaults);
    cna_graphics_device_get_presentation_parameters(device, &deviceParameters);
    CNA_Rectangle bounds = {0, 0, 0, 0};
    cna_presentation_parameters_get_bounds(&deviceParameters, &bounds);
    CNA_BackBufferInfo displayBackBuffer;
    VERSIONED(displayBackBuffer);
    cna_graphics_device_get_backbuffer_info(device, &displayBackBuffer);
    cprobe_got(p, "800x600 aspect %.6f equal %u; %llu adapter(s), [0] %u default %u, \"%.60s\" (%llu = %llu); current mode %u: %dx%d aspect %.4f; Reach %u; device parameters %dx%d, bounds %dx%d, back buffer %ux%u",
               (double)mode.aspect_ratio, equal, (unsigned long long)adapters, (unsigned)adapterResult, adapter.is_default_adapter, description, (unsigned long long)descriptionBytes,
               (unsigned long long)adapter.description_byte_length, (unsigned)currentResult, (int)current.width, (int)current.height, (double)current.aspect_ratio, reach,
               (int)deviceParameters.back_buffer_width, (int)deviceParameters.back_buffer_height, (int)bounds.width, (int)bounds.height, (unsigned)displayBackBuffer.width, (unsigned)displayBackBuffer.height);
    CPROBE_CHECK(p, "cna_display_mode_init/equals, cna_graphics_adapter_get_count/get_info/copy_description/get_current_display_mode/is_profile_supported, cna_presentation_parameters_get_bounds, cna_graphics_device_get_presentation_parameters",
                 "800x600 = 1.333333 and equal; a default adapter 0 whose description size agrees, a current mode with width/height = its aspect ratio, Reach supported; the device's bounds = its back buffer",
                 fabsf(mode.aspect_ratio - 4.0f / 3.0f) < 1e-6f && equal == CNA_TRUE && adapters >= 1u && adapterResult == CNA_RESULT_SUCCESS && adapter.is_default_adapter == CNA_TRUE &&
                     described == CNA_RESULT_SUCCESS && descriptionBytes == adapter.description_byte_length && currentResult == CNA_RESULT_SUCCESS && current.width > 0 && current.height > 0 &&
                     fabsf(current.aspect_ratio - (float)current.width / (float)current.height) < 1e-4f && reach == CNA_TRUE && bounds.width == (int32_t)displayBackBuffer.width &&
                     bounds.height == (int32_t)displayBackBuffer.height);
    CNA_PresentationParameters copy;
    VERSIONED(copy);
    cna_presentation_parameters_clone(&deviceParameters, &copy);
    cprobe_got(p, "defaults: %dx%d, format %u, full screen %u; clone of the device's: %dx%d", (int)defaults.back_buffer_width, (int)defaults.back_buffer_height, (unsigned)defaults.back_buffer_format,
               defaults.is_full_screen, (int)copy.back_buffer_width, (int)copy.back_buffer_height);
    CPROBE_CHECK(p, "cna_presentation_parameters_init (documented as XNA-compatible defaults), cna_presentation_parameters_clone",
                 "XNA 4.0 PresentationParameters(): IsFullScreen true, Color, a 0x0 back buffer (only IsFullScreen is set; the rest stays zero); the clone equals the device's",
                 defaults.is_full_screen == CNA_TRUE && defaults.back_buffer_format == CNA_SURFACE_FORMAT_COLOR && defaults.back_buffer_width == 0 && defaults.back_buffer_height == 0 &&
                     copy.back_buffer_width == deviceParameters.back_buffer_width && copy.back_buffer_height == deviceParameters.back_buffer_height);
}

static void ExtensionChecks(GameProbe* g, CNA_Handle device)
{
    CProbe* p = &g->probe;
    CNA_Bool available = CNA_FALSE;
    cna_graphics_ext_is_available(&available);
    CNA_DebugDrawHandle debug = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_debug_draw_create(device, &debug);
    const CNA_Matrix identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    cna_debug_draw_begin(debug, &identity, &identity);
    const CNA_Vector3 from = {0, 0, 0}, to = {1, 2, 3};
    cna_debug_draw_add_line(debug, &from, &to, (CNA_Color){255, 0, 0, 255});
    CNA_BoundingBox box = {{-1, -1, -1}, {1, 1, 1}};
    cna_debug_draw_set_depth_tested(debug, CNA_FALSE);
    cna_debug_draw_add_box(debug, &box, (CNA_Color){0, 255, 0, 255});
    int32_t lines = 0;
    cna_debug_draw_get_line_count(debug, &lines);
    uint64_t tested = 0, overlay = 0;
    cna_debug_draw_copy_vertices(debug, CNA_TRUE, NULL, 0u, &tested);
    cna_debug_draw_copy_vertices(debug, CNA_FALSE, NULL, 0u, &overlay);
    CNA_VertexPositionColor first[2];
    memset(first, 0, sizeof first);
    uint64_t firstCount = 0;
    cna_debug_draw_copy_vertices(debug, CNA_TRUE, first, 2u, &firstCount);
    cna_debug_draw_clear(debug);
    int32_t afterClear = -1;
    cna_debug_draw_get_line_count(debug, &afterClear);
    cna_debug_draw_destroy(debug);
    CNA_EffectHandle crt = CNA_INVALID_HANDLE;
    const CNA_Result crtCreated = cna_crt_effect_create(device, &crt);
    float scanlines = -1.0f;
    cna_crt_effect_set_scanline_intensity(crt, 0.25f);
    cna_crt_effect_get_scanline_intensity(crt, &scanlines);
    if (crtCreated == CNA_RESULT_SUCCESS) cna_effect_destroy(crt);
    CNA_ImageBasedLightEXT light;
    VERSIONED(light);
    cna_image_based_light_ext_init(&light);
    CNA_Bool valid = CNA_TRUE;
    cna_image_based_light_ext_is_valid(&light, &valid);
    cprobe_got(p, "extensions %u; DebugDraw %u: %d lines (%llu depth-tested vertices, %llu overlay), first line (%g,%g,%g)-(%g,%g,%g), after clear %d; CRT %u scanlines %g; empty image-based light valid %u",
               available, (unsigned)created, (int)lines, (unsigned long long)tested, (unsigned long long)overlay, (double)first[0].position.x, (double)first[0].position.y, (double)first[0].position.z,
               (double)first[1].position.x, (double)first[1].position.y, (double)first[1].position.z, (int)afterClear, (unsigned)crtCreated, (double)scanlines, valid);
    CPROBE_CHECK(p, "cna_graphics_ext_is_available, cna_debug_draw_create/begin/add_line/set_depth_tested/add_box/get_line_count/copy_vertices/clear/destroy, cna_crt_effect_create/set+get_scanline_intensity, cna_image_based_light_ext_init/is_valid",
                 "available (CNA_CNAEXT=ON); 1 + 12 box edges = 13 lines: 2 depth-tested vertices (the line) and 24 overlay; cleared to 0; scanlines 0.25; a light without textures is not valid",
                 available == CNA_TRUE && created == CNA_RESULT_SUCCESS && lines == 13 && tested == 2u && overlay == 24u && firstCount == 2u && first[1].position.z == 3.0f && afterClear == 0 &&
                     crtCreated == CNA_RESULT_SUCCESS && scanlines == 0.25f && valid == CNA_FALSE);
}

static void TargetLifetime(GameProbe* g, CNA_Handle device)
{
    CProbe* p = &g->probe;
    if (g->target == CNA_INVALID_HANDLE) {
        game_probe_skip(g, "cna_render_target_destroy while bound / unbound", "no render target was created");
        return;
    }
    cna_graphics_device_set_render_target2d(device, g->target);
    const CNA_Result whileBound = cna_render_target_destroy(g->target);
    uint64_t stillBound = 0;
    cna_graphics_device_get_render_target_count(device, &stillBound);
    cna_graphics_device_set_render_target2d(device, CNA_INVALID_HANDLE);
    const CNA_Result unbound = cna_render_target_destroy(g->target);
    const CNA_Result again = cna_render_target_destroy(g->target);
    g->target = CNA_INVALID_HANDLE;
    cprobe_got(p, "destroy while bound %u (still %llu binding), after unbinding %u, again %u", (unsigned)whileBound, (unsigned long long)stillBound, (unsigned)unbound, (unsigned)again);
    CPROBE_CHECK(p, "cna_render_target_destroy while bound, then unbound", "INVALID_STATE while bound (nothing changes), SUCCESS once unbound, then INVALID_HANDLE",
                 whileBound == CNA_RESULT_INVALID_STATE && stillBound == 1u && unbound == CNA_RESULT_SUCCESS && again == CNA_RESULT_INVALID_HANDLE);
}

void gp_graphics_more(GameProbe* g, CNA_Handle game, CNA_Handle device)
{
    (void)game;
    VolumeChecks(g, device);
    DisplayChecks(g, device);
    ExtensionChecks(g, device);
    TargetLifetime(g, device);
}
