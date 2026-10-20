/* The C game probe's effects, models, sprite fonts and content: the stock effects' XNA 4.0 defaults and parameter
 * tables (effects.h), a bone hierarchy and a compiled XNA model (models.h), a SpriteFont built from a glyph table and
 * measured with XNA's own rule (sprite_font.h), and a content manager over the museum's Content folder: compiled
 * .xnb assets, a loose PNG, a missing asset and its canonical exception (content.h, content_readers.h). */
#include <math.h>
#include <stdio.h>

#include "CApi/GameProbe/GameProbe.h"

static int NearV3(CNA_Vector3 v, float x, float y, float z, float tolerance)
{
    return fabsf(v.x - x) <= tolerance && fabsf(v.y - y) <= tolerance && fabsf(v.z - z) <= tolerance;
}

static void StockEffects(GameProbe* g, CNA_Handle device)
{
    CProbe* p = &g->probe;
    CNA_EffectHandle basic = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_basic_effect_create(device, &basic);
    CNA_Vector3 diffuse = {0, 0, 0}, specular = {0, 0, 0}, emissive = {9, 9, 9};
    float power = 0.0f, alpha = 0.0f, fogEnd = 0.0f;
    CNA_Bool lighting = CNA_TRUE;
    cna_basic_effect_get_diffuse_color(basic, &diffuse);
    cna_basic_effect_get_specular_color(basic, &specular);
    cna_basic_effect_get_emissive_color(basic, &emissive);
    cna_basic_effect_get_specular_power(basic, &power);
    cna_basic_effect_get_alpha(basic, &alpha);
    cna_effect_fog_get_end(basic, &fogEnd);
    cna_effect_lights_get_enabled(basic, &lighting);
    cprobe_got(p, "create %u: diffuse (%g, %g, %g), specular (%g, %g, %g), emissive (%g, %g, %g), power %g, alpha %g, fog end %g, lighting %u", (unsigned)created, (double)diffuse.x,
               (double)diffuse.y, (double)diffuse.z, (double)specular.x, (double)specular.y, (double)specular.z, (double)emissive.x, (double)emissive.y, (double)emissive.z, (double)power,
               (double)alpha, (double)fogEnd, lighting);
    CPROBE_CHECK(p, "cna_basic_effect_create and its getters (diffuse, specular, emissive, specular power, alpha), cna_effect_fog_get_end, cna_effect_lights_get_enabled",
                 "XNA 4.0 BasicEffect defaults: diffuse and specular (1, 1, 1), emissive 0, SpecularPower 16, Alpha 1, FogEnd 1, lighting off",
                 created == CNA_RESULT_SUCCESS && NearV3(diffuse, 1, 1, 1, 0) && NearV3(specular, 1, 1, 1, 0) && NearV3(emissive, 0, 0, 0, 0) && power == 16.0f && alpha == 1.0f &&
                     fogEnd == 1.0f && lighting == CNA_FALSE);

    /* EnableDefaultLighting: XNA's fixed three-light rig (EffectHelpers.EnableDefaultLighting). */
    cna_effect_lights_enable_default(basic);
    CNA_Vector3 ambient = {0, 0, 0}, direction = {0, 0, 0}, lightDiffuse = {0, 0, 0};
    cna_effect_lights_get_ambient_color(basic, &ambient);
    CNA_DirectionalLightHandle light0 = CNA_INVALID_HANDLE;
    cna_effect_lights_get_directional_light(basic, 0u, &light0);
    cna_directional_light_get_direction(light0, &direction);
    cna_directional_light_get_diffuse_color(light0, &lightDiffuse);
    cna_directional_light_destroy(light0);
    CNA_Bool lit = CNA_FALSE;
    cna_effect_lights_get_enabled(basic, &lit);
    cprobe_got(p, "ambient (%.7f, %.7f, %.7f), light 0 direction (%.7f, %.7f, %.7f) diffuse (%.7f, %.7f, %.7f), lighting %u", (double)ambient.x, (double)ambient.y, (double)ambient.z,
               (double)direction.x, (double)direction.y, (double)direction.z, (double)lightDiffuse.x, (double)lightDiffuse.y, (double)lightDiffuse.z, lit);
    CPROBE_CHECK(p, "cna_effect_lights_enable_default, get_ambient_color, get_directional_light, cna_directional_light_get_direction/get_diffuse_color/destroy",
                 "ambient (0.0533333, 0.0988235, 0.1819608), key light direction (-0.5265408, -0.5735765, -0.6275069) and colour (1, 0.9607844, 0.8078432), lighting on",
                 NearV3(ambient, 0.05333332f, 0.09882354f, 0.1819608f, 1e-6f) && NearV3(direction, -0.5265408f, -0.5735765f, -0.6275069f, 1e-6f) &&
                     NearV3(lightDiffuse, 1.0f, 0.9607844f, 0.8078432f, 1e-6f) && lit == CNA_TRUE);

    /* The parameter and technique tables an XNA game reaches through Effect.Parameters / Techniques. */
    CNA_EffectParameterCollectionHandle parameters = CNA_INVALID_HANDLE;
    cna_effect_get_parameters(basic, &parameters);
    uint64_t parameterCount = 0;
    cna_effect_parameter_collection_get_count(parameters, &parameterCount);
    CNA_Bool foundDiffuse = CNA_FALSE, foundWvp = CNA_FALSE, foundBogus = CNA_TRUE;
    CNA_EffectParameterHandle diffuseParameter = CNA_INVALID_HANDLE, wvp = CNA_INVALID_HANDLE, bogus = CNA_INVALID_HANDLE;
    cna_effect_parameter_collection_find_name(parameters, cprobe_view("DiffuseColor"), &foundDiffuse, &diffuseParameter);
    cna_effect_parameter_collection_find_name(parameters, cprobe_view("WorldViewProj"), &foundWvp, &wvp);
    cna_effect_parameter_collection_find_name(parameters, cprobe_view("NoSuchParameter"), &foundBogus, &bogus);
    CNA_EffectParameterInfo diffuseInfo, wvpInfo;
    VERSIONED(diffuseInfo);
    VERSIONED(wvpInfo);
    cna_effect_parameter_get_info(diffuseParameter, &diffuseInfo);
    cna_effect_parameter_get_info(wvp, &wvpInfo);
    if (diffuseParameter != CNA_INVALID_HANDLE) cna_effect_parameter_destroy(diffuseParameter);
    if (wvp != CNA_INVALID_HANDLE) cna_effect_parameter_destroy(wvp);
    cna_effect_parameter_collection_destroy(parameters);
    CNA_EffectTechniqueCollectionHandle techniques = CNA_INVALID_HANDLE;
    cna_effect_get_techniques(basic, &techniques);
    uint64_t techniqueCount = 0;
    cna_effect_technique_collection_get_count(techniques, &techniqueCount);
    cna_effect_technique_collection_destroy(techniques);
    CNA_EffectTechniqueHandle current = CNA_INVALID_HANDLE;
    cna_effect_get_current_technique(basic, &current);
    char techniqueName[64];
    uint64_t techniqueBytes = 0;
    const CNA_Result techniqueNamed = cna_effect_technique_copy_name(current, techniqueName, sizeof techniqueName - 1, &techniqueBytes);
    cprobe_terminate(techniqueName, sizeof techniqueName, techniqueNamed, &techniqueBytes);
    if (current != CNA_INVALID_HANDLE) cna_effect_technique_destroy(current);
    char typeName[96];
    uint64_t typeBytes = 0;
    const CNA_Result typed = cna_effect_copy_type_name(basic, typeName, sizeof typeName - 1, &typeBytes);
    cprobe_terminate(typeName, sizeof typeName, typed, &typeBytes);
    CNA_EffectHandle clone = CNA_INVALID_HANDLE;
    cna_basic_effect_set_diffuse_color(basic, (CNA_Vector3){0.25f, 0.5f, 0.75f});
    const CNA_Result cloned = cna_effect_clone(basic, &clone);
    CNA_Vector3 cloneDiffuse = {0, 0, 0};
    cna_basic_effect_get_diffuse_color(clone, &cloneDiffuse);
    if (clone != CNA_INVALID_HANDLE) cna_effect_destroy(clone);
    cprobe_got(p, "%llu parameters: DiffuseColor %u (class %u, %dx%d, type %u), WorldViewProj %u (class %u, %dx%d), NoSuchParameter %u; %llu technique(s), current \"%s\"; \"%s\"; clone %u diffuse (%g, %g, %g)",
               (unsigned long long)parameterCount, foundDiffuse, (unsigned)diffuseInfo.parameter_class, (int)diffuseInfo.row_count, (int)diffuseInfo.column_count,
               (unsigned)diffuseInfo.parameter_type, foundWvp, (unsigned)wvpInfo.parameter_class, (int)wvpInfo.row_count, (int)wvpInfo.column_count, foundBogus,
               (unsigned long long)techniqueCount, techniqueName, typeName, (unsigned)cloned, (double)cloneDiffuse.x, (double)cloneDiffuse.y, (double)cloneDiffuse.z);
    CPROBE_CHECK(p, "cna_effect_get_parameters, cna_effect_parameter_collection_get_count/find_name/destroy, cna_effect_parameter_get_info/destroy, cna_effect_get_techniques, cna_effect_technique_collection_get_count, cna_effect_get_current_technique, cna_effect_technique_copy_name, cna_effect_copy_type_name, cna_effect_clone",
                 "XNA's BasicEffect parameters: DiffuseColor a 1x4 single vector, WorldViewProj a 4x4 matrix, an unknown name not found; a named current technique; \"Microsoft.Xna.Framework.Graphics.BasicEffect\"; the clone keeps the diffuse colour",
                 parameterCount > 0u && foundDiffuse == CNA_TRUE && diffuseInfo.parameter_class == CNA_EFFECT_PARAMETER_CLASS_VECTOR && diffuseInfo.row_count == 1 && diffuseInfo.column_count == 4 &&
                     diffuseInfo.parameter_type == CNA_EFFECT_PARAMETER_TYPE_SINGLE && foundWvp == CNA_TRUE && wvpInfo.parameter_class == CNA_EFFECT_PARAMETER_CLASS_MATRIX &&
                     wvpInfo.row_count == 4 && wvpInfo.column_count == 4 && foundBogus == CNA_FALSE && techniqueCount >= 1u && techniqueNamed == CNA_RESULT_SUCCESS && techniqueBytes > 0u &&
                     strcmp(typeName, "Microsoft.Xna.Framework.Graphics.BasicEffect") == 0 && cloned == CNA_RESULT_SUCCESS && NearV3(cloneDiffuse, 0.25f, 0.5f, 0.75f, 0));
    cna_effect_destroy(basic);

    /* The other stock effects' XNA defaults. */
    CNA_EffectHandle alphaTest = CNA_INVALID_HANDLE, environment = CNA_INVALID_HANDLE, skinned = CNA_INVALID_HANDLE, dual = CNA_INVALID_HANDLE;
    const CNA_Result alphaCreated = cna_alpha_test_effect_create(device, &alphaTest);
    const CNA_Result environmentCreated = cna_environment_map_effect_create(device, &environment);
    const CNA_Result skinnedCreated = cna_skinned_effect_create(device, &skinned);
    const CNA_Result dualCreated = cna_dual_texture_effect_create(device, &dual);
    CNA_CompareFunction function = 99;
    int32_t referenceAlpha = -1, weights = 0;
    float fresnel = 0.0f, amount = 0.0f, skinnedPower = 0.0f;
    CNA_Vector3 dualDiffuse = {0, 0, 0};
    cna_alpha_test_effect_get_alpha_function(alphaTest, &function);
    cna_alpha_test_effect_get_reference_alpha(alphaTest, &referenceAlpha);
    cna_environment_map_effect_get_fresnel_factor(environment, &fresnel);
    cna_environment_map_effect_get_amount(environment, &amount);
    cna_skinned_effect_get_weights_per_vertex(skinned, &weights);
    cna_skinned_effect_get_specular_power(skinned, &skinnedPower);
    cna_dual_texture_effect_get_diffuse_color(dual, &dualDiffuse);
    const CNA_Result badWeights = cna_skinned_effect_set_weights_per_vertex(skinned, 3);
    if (alphaCreated == CNA_RESULT_SUCCESS) cna_effect_destroy(alphaTest);
    if (environmentCreated == CNA_RESULT_SUCCESS) cna_effect_destroy(environment);
    if (skinnedCreated == CNA_RESULT_SUCCESS) cna_effect_destroy(skinned);
    if (dualCreated == CNA_RESULT_SUCCESS) cna_effect_destroy(dual);
    cprobe_got(p, "AlphaTest %u: function %u, reference %d; EnvironmentMap %u: fresnel %g, amount %g; Skinned %u: weights %d, power %g, 3 weights -> %u; DualTexture %u: diffuse (%g, %g, %g)",
               (unsigned)alphaCreated, (unsigned)function, (int)referenceAlpha, (unsigned)environmentCreated, (double)fresnel, (double)amount, (unsigned)skinnedCreated, (int)weights,
               (double)skinnedPower, (unsigned)badWeights, (unsigned)dualCreated, (double)dualDiffuse.x, (double)dualDiffuse.y, (double)dualDiffuse.z);
    CPROBE_CHECK(p, "cna_alpha_test_effect_create/get_alpha_function/get_reference_alpha, cna_environment_map_effect_create/get_fresnel_factor/get_amount, cna_skinned_effect_create/get_weights_per_vertex/get_specular_power/set_weights_per_vertex, cna_dual_texture_effect_create/get_diffuse_color, cna_effect_destroy",
                 "XNA defaults: AlphaTest Greater with reference 0; EnvironmentMap FresnelFactor 1 and amount 1; Skinned 4 weights and power 16, 3 weights refused (XNA accepts 1, 2 or 4); DualTexture diffuse (1, 1, 1)",
                 alphaCreated == CNA_RESULT_SUCCESS && function == CNA_COMPARE_GREATER && referenceAlpha == 0 && environmentCreated == CNA_RESULT_SUCCESS && fresnel == 1.0f && amount == 1.0f &&
                     skinnedCreated == CNA_RESULT_SUCCESS && weights == 4 && skinnedPower == 16.0f && badWeights == CNA_RESULT_INVALID_ARGUMENT && dualCreated == CNA_RESULT_SUCCESS &&
                     NearV3(dualDiffuse, 1, 1, 1, 0));
}

static void Models(GameProbe* g, CNA_Handle device, CNA_Handle content)
{
    CProbe* p = &g->probe;
    /* A two-bone hierarchy built in C: the child's absolute transform is its own times its parent's. */
    CNA_ModelBoneHandle root = CNA_INVALID_HANDLE, child = CNA_INVALID_HANDLE;
    cna_model_bone_create(0, cprobe_view("root"), &root);
    cna_model_bone_create(1, cprobe_view("child"), &child);
    CNA_Matrix rootTransform, childTransform;
    cna_matrix_create_translation_xyz(1.0f, 2.0f, 3.0f, &rootTransform);
    cna_matrix_create_scale_scalar(2.0f, &childTransform);
    cna_model_bone_set_transform(root, rootTransform);
    cna_model_bone_set_transform(child, childTransform);
    const CNA_Result linked = cna_model_bone_add_child(root, child);
    const CNA_Result cycle = cna_model_bone_add_child(child, root);
    const CNA_ModelBoneHandle bones[2] = {root, child};
    CNA_ModelHandle model = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_model_create(device, bones, 2u, NULL, 0u, &model);
    CNA_Matrix absolute[2];
    memset(absolute, 0, sizeof absolute);
    uint64_t absoluteCount = 0;
    cna_model_copy_absolute_bone_transforms(model, absolute, 2u, &absoluteCount);
    CNA_ModelBoneCollectionHandle collection = CNA_INVALID_HANDLE;
    cna_model_get_bones(model, &collection);
    uint64_t boneCount = 0;
    cna_model_bone_collection_get_count(collection, &boneCount);
    CNA_Bool found = CNA_FALSE, hasParent = CNA_FALSE;
    CNA_ModelBoneHandle foundBone = CNA_INVALID_HANDLE, parent = CNA_INVALID_HANDLE;
    cna_model_bone_collection_find(collection, cprobe_view("child"), &found, &foundBone);
    cna_model_bone_get_parent(foundBone, &hasParent, &parent);
    char parentName[16];
    uint64_t parentBytes = 0;
    const CNA_Result parentNamed = cna_model_bone_copy_name(parent, parentName, sizeof parentName - 1, &parentBytes);
    cprobe_terminate(parentName, sizeof parentName, parentNamed, &parentBytes);
    int32_t foundIndex = -1;
    cna_model_bone_get_index(foundBone, &foundIndex);
    if (parent != CNA_INVALID_HANDLE) cna_model_bone_destroy(parent);
    if (foundBone != CNA_INVALID_HANDLE) cna_model_bone_destroy(foundBone);
    cna_model_bone_collection_destroy(collection);
    cna_model_destroy(model);
    cna_model_bone_destroy(child);
    cna_model_bone_destroy(root);
    cprobe_got(p, "add_child %u, cycle %u, model %u; %llu absolute transforms: child scale %g translation (%g, %g, %g); %llu bones, \"child\" found %u at index %d, parent %u \"%s\"", (unsigned)linked,
               (unsigned)cycle, (unsigned)created, (unsigned long long)absoluteCount, (double)absolute[1].m11, (double)absolute[1].m41, (double)absolute[1].m42, (double)absolute[1].m43,
               (unsigned long long)boneCount, found, (int)foundIndex, hasParent, parentName);
    CPROBE_CHECK(p, "cna_model_bone_create/set_transform/add_child/get_parent/copy_name/get_index/destroy, cna_model_create/copy_absolute_bone_transforms/get_bones/destroy, cna_model_bone_collection_get_count/find/destroy",
                 "a cycle refused; the child's absolute transform = scale 2 then the root's translation (1, 2, 3); 2 bones, \"child\" at index 1 with parent \"root\"",
                 linked == CNA_RESULT_SUCCESS && cycle != CNA_RESULT_SUCCESS && created == CNA_RESULT_SUCCESS && absoluteCount == 2u && absolute[1].m11 == 2.0f && absolute[1].m41 == 1.0f &&
                     absolute[1].m42 == 2.0f && absolute[1].m43 == 3.0f && boneCount == 2u && found == CNA_TRUE && foundIndex == 1 && hasParent == CNA_TRUE && strcmp(parentName, "root") == 0);

    /* A model compiled by XNA's own pipeline (the museum's Compiled/xnb/shape.xnb), through ContentManager.Load<Model>. */
    CNA_ModelHandle shape = CNA_INVALID_HANDLE;
    const CNA_Result loaded = cna_content_manager_load_model(content, cprobe_view("Compiled/xnb/shape"), &shape);
    CNA_ModelMeshCollectionHandle meshes = CNA_INVALID_HANDLE;
    cna_model_get_meshes(shape, &meshes);
    uint64_t meshCount = 0;
    cna_model_mesh_collection_get_count(meshes, &meshCount);
    CNA_ModelMeshHandle mesh = CNA_INVALID_HANDLE;
    cna_model_mesh_collection_get_at(meshes, 0u, &mesh);
    CNA_BoundingSphere sphere;
    memset(&sphere, 0, sizeof sphere);
    cna_model_mesh_get_bounding_sphere(mesh, &sphere);
    CNA_Bool hasRoot = CNA_FALSE;
    CNA_ModelBoneHandle shapeRoot = CNA_INVALID_HANDLE;
    cna_model_get_root(shape, &hasRoot, &shapeRoot);
    if (shapeRoot != CNA_INVALID_HANDLE) cna_model_bone_destroy(shapeRoot);
    if (mesh != CNA_INVALID_HANDLE) cna_model_mesh_destroy(mesh);
    if (meshes != CNA_INVALID_HANDLE) cna_model_mesh_collection_destroy(meshes);
    const CNA_Matrix identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const CNA_Result drawn = cna_model_draw(shape, identity, identity, identity);
    if (shape != CNA_INVALID_HANDLE) cna_model_destroy(shape);
    cprobe_got(p, "load %u: %llu mesh(es), first bounding sphere radius %g, root %u, draw %u", (unsigned)loaded, (unsigned long long)meshCount, (double)sphere.radius, hasRoot, (unsigned)drawn);
    CPROBE_CHECK(p, "cna_content_manager_load_model (an XNA-compiled .xnb), cna_model_get_meshes/get_root/draw, cna_model_mesh_collection_get_count/get_at/destroy, cna_model_mesh_get_bounding_sphere/destroy",
                 "the model loads with at least one mesh whose bounding sphere has a positive radius, a root bone, and draws",
                 loaded == CNA_RESULT_SUCCESS && meshCount >= 1u && sphere.radius > 0.0f && hasRoot == CNA_TRUE && drawn == CNA_RESULT_SUCCESS);
}

static void Fonts(GameProbe* g, CNA_Handle device)
{
    CProbe* p = &g->probe;
    const CNA_Color white[64] = {{255, 255, 255, 255}};
    CNA_Handle atlas = CNA_INVALID_HANDLE;
    cna_texture2d_create_from_rgba8(device, 8u, 8u, white, 64u, &atlas);
    CNA_SpriteFontGlyph glyphs[2];
    memset(glyphs, 0, sizeof glyphs);
    for (int i = 0; i < 2; ++i) {
        glyphs[i].struct_size = (uint32_t)sizeof glyphs[i];
        glyphs[i].struct_version = 1u;
    }
    glyphs[0].glyph_bounds = (CNA_Rectangle){0, 0, 4, 8};
    glyphs[0].cropping = (CNA_Rectangle){0, 0, 4, 9};
    glyphs[0].character = 'A';
    glyphs[0].kerning = (CNA_Vector3){1.0f, 6.0f, 2.0f};
    glyphs[1].glyph_bounds = (CNA_Rectangle){4, 0, 4, 8};
    glyphs[1].cropping = (CNA_Rectangle){0, 0, 4, 8};
    glyphs[1].character = 'B';
    glyphs[1].kerning = (CNA_Vector3){0.0f, 5.0f, 1.0f};
    CNA_SpriteFontCreateInfo create;
    VERSIONED(create);
    create.texture = atlas;
    create.glyphs = glyphs;
    create.glyph_count = 2u;
    create.line_spacing = 10;
    create.spacing = 1.0f;
    CNA_Handle font = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_sprite_font_create(&create, &font);
    CNA_Vector2 ab = {0, 0}, lines = {0, 0}, unknown = {0, 0}, fallback = {0, 0};
    cna_sprite_font_measure_utf8(font, cprobe_view("AB"), &ab);
    cna_sprite_font_measure_utf8(font, cprobe_view("A\nB"), &lines);
    const CNA_Result missing = cna_sprite_font_measure_utf8(font, cprobe_view("C"), &unknown);
    cna_sprite_font_set_default_character(font, CNA_TRUE, 'A');
    cna_sprite_font_measure_utf8(font, cprobe_view("C"), &fallback);
    cna_sprite_font_set_line_spacing(font, 12);
    cna_sprite_font_set_spacing(font, 0.5f);
    CNA_SpriteFontInfo info;
    VERSIONED(info);
    cna_sprite_font_get_info(font, &info);
    CNA_Char16 characters[4] = {0, 0, 0, 0};
    uint64_t characterCount = 0;
    cna_sprite_font_copy_characters(font, characters, 4u, &characterCount);
    CNA_SpriteFontGlyph glyphsBack[2];
    memset(glyphsBack, 0, sizeof glyphsBack);
    for (int i = 0; i < 2; ++i) {
        glyphsBack[i].struct_size = (uint32_t)sizeof glyphsBack[i];
        glyphsBack[i].struct_version = 1u;
    }
    uint64_t glyphCount = 0;
    cna_sprite_font_copy_glyphs(font, glyphsBack, 2u, &glyphCount);
    const CNA_Result atlasWhileUsed = cna_texture2d_destroy(atlas);
    cna_sprite_font_destroy(font);
    const CNA_Result atlasAfter = cna_texture2d_destroy(atlas);
    const int hasA = characters[0] == 'A' || characters[1] == 'A';
    const int hasB = characters[0] == 'B' || characters[1] == 'B';
    cprobe_got(p, "create %u; \"AB\" %gx%g, \"A\\nB\" %gx%g, \"C\" without a default -> %u, with default 'A' %gx%g; info line spacing %d spacing %g default %u/%c, %llu characters (%s), %llu glyphs; atlas destroy while used %u, after %u",
               (unsigned)created, (double)ab.x, (double)ab.y, (double)lines.x, (double)lines.y, (unsigned)missing, (double)fallback.x, (double)fallback.y, (int)info.line_spacing, (double)info.spacing,
               info.has_default_character, (char)info.default_character, (unsigned long long)characterCount, hasA && hasB ? "A and B" : "other", (unsigned long long)glyphCount,
               (unsigned)atlasWhileUsed, (unsigned)atlasAfter);
    CPROBE_CHECK(p, "cna_sprite_font_create/measure_utf8/set_default_character/set_line_spacing/set_spacing/get_info/copy_characters/copy_glyphs/destroy",
                 "XNA's SpriteFont.MeasureString: \"AB\" 16x10, \"A\\nB\" 9x20, an unknown character refused (INVALID_ARGUMENT) until DefaultCharacter 'A' makes \"C\" 9x10; the setters read back; A and B; the atlas outlives the font",
                 created == CNA_RESULT_SUCCESS && ab.x == 16.0f && ab.y == 10.0f && lines.x == 9.0f && lines.y == 20.0f && missing == CNA_RESULT_INVALID_ARGUMENT && fallback.x == 9.0f &&
                     fallback.y == 10.0f && info.line_spacing == 12 && info.spacing == 0.5f && info.has_default_character == CNA_TRUE && info.default_character == 'A' && characterCount == 2u &&
                     hasA && hasB && glyphCount == 2u && atlasWhileUsed != CNA_RESULT_SUCCESS && atlasAfter == CNA_RESULT_SUCCESS);
}

static int PngSize(const char* path, uint32_t* width, uint32_t* height)
{
    unsigned char header[24];
    FILE* file = fopen(path, "rb");
    if (file == NULL) return 0;
    const size_t n = fread(header, 1, sizeof header, file);
    fclose(file);
    if (n != sizeof header) return 0;
    *width = (uint32_t)header[16] << 24 | (uint32_t)header[17] << 16 | (uint32_t)header[18] << 8 | header[19];
    *height = (uint32_t)header[20] << 24 | (uint32_t)header[21] << 16 | (uint32_t)header[22] << 8 | header[23];
    return 1;
}

static CNA_Handle Content(GameProbe* g, CNA_Handle game, CNA_Handle device)
{
    CProbe* p = &g->probe;
    CNA_ContentManagerCreateInfo create;
    VERSIONED(create);
    create.root_directory = cprobe_view(g->contentRoot);
    CNA_Handle content = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_content_manager_create(device, &create, &content);
    char root[1024];
    uint64_t rootBytes = 0;
    const CNA_Result rootCopied = cna_content_manager_copy_root_directory(content, root, sizeof root - 1, &rootBytes);
    cprobe_terminate(root, sizeof root, rootCopied, &rootBytes);
    CNA_Handle picture = CNA_INVALID_HANDLE, checker = CNA_INVALID_HANDLE, missing = CNA_INVALID_HANDLE;
    const CNA_Result pictureLoaded = cna_content_manager_load_texture2d(content, cprobe_view("Compiled/xnb/picture"), &picture);
    const CNA_Result checkerLoaded = cna_content_manager_load_texture2d(content, cprobe_view("Textures/checker"), &checker);
    CNA_Texture2DInfo pictureInfo, checkerInfo;
    VERSIONED(pictureInfo);
    VERSIONED(checkerInfo);
    cna_texture2d_get_info(picture, &pictureInfo);
    cna_texture2d_get_info(checker, &checkerInfo);
    char checkerPath[1200];
    snprintf(checkerPath, sizeof checkerPath, "%s/Textures/checker.png", g->contentRoot);
    uint32_t pngWidth = 0, pngHeight = 0;
    PngSize(checkerPath, &pngWidth, &pngHeight);
    const CNA_Result missingLoaded = cna_content_manager_load_texture2d(content, cprobe_view("Textures/no-such-texture"), &missing);
    char exceptionType[128];
    uint64_t exceptionBytes = 0;
    const CNA_Result exceptionCopied = cna_error_copy_last_exception_type_ext(exceptionType, sizeof exceptionType - 1, &exceptionBytes);
    cprobe_terminate(exceptionType, sizeof exceptionType, exceptionCopied, &exceptionBytes);
    char key[256];
    uint64_t keyBytes = 0;
    const CNA_Result keyCopied = cna_content_manager_copy_normalized_key(content, cprobe_view("Textures/checker"), key, sizeof key - 1, &keyBytes);
    cprobe_terminate(key, sizeof key, keyCopied, &keyBytes);
    uint64_t manifest = 0;
    cna_content_manager_get_manifest_entry_count(content, &manifest);
    CNA_Handle borrowed = CNA_INVALID_HANDLE;
    cna_game_get_content_manager_ext(game, &borrowed);
    const CNA_Result borrowedDestroy = cna_content_manager_destroy(borrowed);
    CNA_Bool provider = CNA_TRUE;
    cna_content_manager_get_has_service_provider(content, &provider);
    if (picture != CNA_INVALID_HANDLE) cna_texture2d_destroy(picture);
    if (checker != CNA_INVALID_HANDLE) cna_texture2d_destroy(checker);
    cprobe_got(p, "create %u, root \"%s\"; picture.xnb %u: %ux%u; Textures/checker %u: %ux%u (PNG header %ux%u); missing -> %u (%s); key \"%s\"; %llu manifest entries; the game's own manager destroy -> %u; service provider %u",
               (unsigned)created, strcmp(root, g->contentRoot) == 0 ? "as given" : root, (unsigned)pictureLoaded, (unsigned)pictureInfo.width, (unsigned)pictureInfo.height, (unsigned)checkerLoaded,
               (unsigned)checkerInfo.width, (unsigned)checkerInfo.height, (unsigned)pngWidth, (unsigned)pngHeight, (unsigned)missingLoaded, exceptionType, key, (unsigned long long)manifest,
               (unsigned)borrowedDestroy, provider);
    CPROBE_CHECK(p, "cna_content_manager_create/copy_root_directory/load_texture2d/copy_normalized_key/get_manifest_entry_count/get_has_service_provider/destroy, cna_game_get_content_manager_ext",
                 "the root as given; the XNA-compiled picture.xnb and the loose Textures/checker.png load, the PNG at the size its header states; a missing asset -> CNA_RESULT_IO; a manifest; the game's borrowed manager refuses destroy (INVALID_STATE); no service provider crosses the ABI",
                 created == CNA_RESULT_SUCCESS && strcmp(root, g->contentRoot) == 0 && pictureLoaded == CNA_RESULT_SUCCESS && pictureInfo.width > 0u && checkerLoaded == CNA_RESULT_SUCCESS &&
                     pngWidth > 0u && checkerInfo.width == pngWidth && checkerInfo.height == pngHeight && missingLoaded == CNA_RESULT_IO && keyCopied == CNA_RESULT_SUCCESS && keyBytes > 0u &&
                     manifest > 0u && borrowedDestroy == CNA_RESULT_INVALID_STATE && provider == CNA_FALSE);
    cprobe_got(p, "result %u, exception type \"%s\" (%llu bytes)", (unsigned)missingLoaded, exceptionType, (unsigned long long)exceptionBytes);
    CPROBE_CHECK(p, "cna_error_copy_last_exception_type_ext after cna_content_manager_load_texture2d of a missing asset",
                 "\"Microsoft.Xna.Framework.Content.ContentLoadException\", the canonical exception core.h names as its own example",
                 missingLoaded == CNA_RESULT_IO && strcmp(exceptionType, "Microsoft.Xna.Framework.Content.ContentLoadException") == 0);

    /* With a content manager alive in this process, XNA's built-in readers are in the registry. */
    CNA_Bool int32Registered = CNA_FALSE;
    cna_content_type_reader_manager_get_is_registered(cprobe_view("Microsoft.Xna.Framework.Content.Int32Reader"), &int32Registered);
    CNA_ContentTypeReaderHandle reader = CNA_INVALID_HANDLE;
    const CNA_Result readerCreated = cna_content_type_reader_manager_create_reader(cprobe_view("Microsoft.Xna.Framework.Content.Int32Reader"), &reader);
    char target[64];
    uint64_t targetBytes = 0;
    const CNA_Result targetCopied = cna_content_type_reader_copy_target_type_name(reader, target, sizeof target - 1, &targetBytes);
    cprobe_terminate(target, sizeof target, targetCopied, &targetBytes);
    if (reader != CNA_INVALID_HANDLE) cna_content_type_reader_destroy(reader);
    cprobe_got(p, "Int32Reader registered %u, create %u, target \"%s\"", int32Registered, (unsigned)readerCreated, target);
    CPROBE_CHECK(p, "cna_content_type_reader_manager_get_is_registered/create_reader for XNA's Int32Reader, cna_content_type_reader_copy_target_type_name",
                 "registered once a content manager exists; it reads System.Int32", int32Registered == CNA_TRUE && readerCreated == CNA_RESULT_SUCCESS && strcmp(target, "System.Int32") == 0);
    return content;
}

void gp_effects(GameProbe* g, CNA_Handle game, CNA_Handle device)
{
    StockEffects(g, device);
    Fonts(g, device);
    const CNA_Handle content = Content(g, game, device);
    Models(g, device, content);
    cna_content_manager_destroy(content);
}
