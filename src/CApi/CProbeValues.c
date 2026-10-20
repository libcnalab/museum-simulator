/* Value families of CNA's C ABI, used from C17: colours (color.h, named_colors.h), the bounding volumes, planes and
 * rays (geometry.h over math_values.h's structs), and the built-in vertex values (vertex_values.h, graphics3d.h).
 * Expectations come from the XNA 4.0 originals (Color, Plane, Ray, BoundingBox, BoundingSphere, BoundingFrustum,
 * VertexElement, VertexPositionColor): their ToString formats, hash codes, packing and corner order. */
#include <math.h>

#include "CApi/CProbe.h"
#include "CNA/C/color.h"
#include "CNA/C/geometry.h"
#include "CNA/C/math.h"
#include "CNA/C/graphics3d.h"
#include "CNA/C/math_values.h"
#include "CNA/C/matrix.h"
#include "CNA/C/named_colors.h"
#include "CNA/C/vertex_values.h"

static int SameColor(CNA_Color c, int r, int g, int b, int a)
{
    return c.r == r && c.g == g && c.b == b && c.a == a;
}

static int Near(float value, float expected, float tolerance)
{
    return fabsf(value - expected) <= tolerance;
}

static int NearV3(CNA_Vector3 v, float x, float y, float z, float tolerance)
{
    return Near(v.x, x, tolerance) && Near(v.y, y, tolerance) && Near(v.z, z, tolerance);
}

static void ColorChecks(CProbe* p)
{
    char text[128];
    uint64_t bytes = 0, counted = 0;

    /* Integer channels clamp to 0..255; the packed value is AABBGGRR; ToString is "{R:.. G:.. B:.. A:..}". */
    CNA_Color clamped = CNA_COLOR_TRANSPARENT;
    uint32_t packed = 0;
    const CNA_Result clampResult = cna_color_init_int_rgb(300, -5, 128, &clamped);
    cna_color_get_packed_value(clamped, &packed);
    cna_color_get_string_byte_count(clamped, &counted);
    const CNA_Result copied = cna_color_copy_string(clamped, text, sizeof text - 1, &bytes);
    cprobe_terminate(text, sizeof text, copied, &bytes);
    cprobe_got(p, "%u: (%u, %u, %u, %u), packed 0x%08X, %llu bytes \"%s\"", (unsigned)clampResult, clamped.r, clamped.g, clamped.b, clamped.a, (unsigned)packed,
               (unsigned long long)counted, text);
    CPROBE_CHECK(p, "cna_color_init_int_rgb(300, -5, 128): clamp, packed value, string", "(255, 0, 128, 255), 0xFF8000FF, 23 bytes \"{R:255 G:0 B:128 A:255}\"",
                 clampResult == CNA_RESULT_SUCCESS && SameColor(clamped, 255, 0, 128, 255) && packed == 0xFF8000FFu && counted == 23u && strcmp(text, "{R:255 G:0 B:128 A:255}") == 0);

    /* A too-small buffer is refused with the needed count and no partial write. */
    char small[8];
    memset(small, '#', sizeof small);
    uint64_t needed = 0;
    const CNA_Result tooSmall = cna_color_copy_string(clamped, small, sizeof small, &needed);
    int untouched = 1;
    for (size_t i = 0; i < sizeof small; ++i) untouched = untouched && small[i] == '#';
    cprobe_got(p, "result %u, needed %llu, buffer %s", (unsigned)tooSmall, (unsigned long long)needed, untouched ? "untouched" : "written");
    CPROBE_CHECK(p, "cna_color_copy_string into 8 bytes", "CNA_RESULT_BUFFER_TOO_SMALL, needed 23, nothing written",
                 tooSmall == CNA_RESULT_BUFFER_TOO_SMALL && needed == 23u && untouched);

    /* set_packed_value splits AABBGGRR; equality against a named colour expression. */
    CNA_Color fromPacked = CNA_COLOR_BLACK;
    cna_color_set_packed_value(&fromPacked, 0x80402010u);
    CNA_Bool same = CNA_FALSE, different = CNA_FALSE;
    cna_color_equals(fromPacked, CNA_COLOR_RGBA(16, 32, 64, 128), &same);
    cna_color_not_equals(fromPacked, CNA_COLOR_BLACK, &different);
    cprobe_got(p, "(%u, %u, %u, %u), equals %u, not_equals(Black) %u", fromPacked.r, fromPacked.g, fromPacked.b, fromPacked.a, (unsigned)same, (unsigned)different);
    CPROBE_CHECK(p, "cna_color_set_packed_value(0x80402010), equals and not_equals", "(16, 32, 64, 128), equal to CNA_COLOR_RGBA(16, 32, 64, 128), not equal to Black",
                 SameColor(fromPacked, 16, 32, 64, 128) && same == CNA_TRUE && different == CNA_TRUE);

    /* XNA: Color.GetHashCode is packedValue.GetHashCode, i.e. the packed bits as an int. */
    int32_t redHash = 0, cornflowerHash = 0;
    cna_color_get_hash_code(CNA_COLOR_RED, &redHash);
    cna_color_get_hash_code(CNA_COLOR_CORNFLOWER_BLUE, &cornflowerHash);
    const CNA_Color cornflower = CNA_COLOR_CORNFLOWER_BLUE;
    cprobe_got(p, "Red %d, CornflowerBlue (%u, %u, %u, %u) %d", (int)redHash, cornflower.r, cornflower.g, cornflower.b, cornflower.a, (int)cornflowerHash);
    CPROBE_CHECK(p, "cna_color_get_hash_code(CNA_COLOR_RED, CNA_COLOR_CORNFLOWER_BLUE)", "-16776961 (0xFF0000FF) and (100, 149, 237, 255) -> -1206940 (0xFFED9564)",
                 redHash == -16776961 && SameColor(cornflower, 100, 149, 237, 255) && cornflowerHash == -1206940);

    /* Lerp in XNA's 16.16 fixed point truncates: 255 * 0.25 -> 63, not 64. */
    CNA_Color lerped = CNA_COLOR_TRANSPARENT;
    cna_color_lerp(CNA_COLOR_BLACK, CNA_COLOR_WHITE, 0.25f, &lerped);
    CNA_Color premultiplied = CNA_COLOR_TRANSPARENT;
    cna_color_from_non_premultiplied_int(200, 100, 50, 128, &premultiplied);
    CNA_Color scaled = CNA_COLOR_TRANSPARENT;
    cna_color_multiply(CNA_COLOR_RGBA(200, 100, 50, 255), 0.5f, &scaled);
    cprobe_got(p, "lerp (%u, %u, %u, %u), premultiplied (%u, %u, %u, %u), multiply (%u, %u, %u, %u)", lerped.r, lerped.g, lerped.b, lerped.a, premultiplied.r, premultiplied.g,
               premultiplied.b, premultiplied.a, scaled.r, scaled.g, scaled.b, scaled.a);
    CPROBE_CHECK(p, "cna_color_lerp(Black, White, 0.25), from_non_premultiplied_int(200, 100, 50, 128), multiply(.., 0.5)",
                 "(63, 63, 63, 255), (100, 50, 25, 128), (100, 50, 25, 127)",
                 SameColor(lerped, 63, 63, 63, 255) && SameColor(premultiplied, 100, 50, 25, 128) && SameColor(scaled, 100, 50, 25, 127));

    /* Float channels round to the nearest byte; to_vector4 divides by 255. */
    CNA_Color fromFloats = CNA_COLOR_TRANSPARENT;
    cna_color_init_float_rgba(1.0f, 0.25f, 0.0f, 0.75f, &fromFloats);
    CNA_Vector4 unit = {0, 0, 0, 0};
    cna_color_to_vector4(CNA_COLOR_RGBA(255, 0, 51, 255), &unit);
    CNA_Color fromVector = CNA_COLOR_TRANSPARENT;
    const CNA_Vector4 quarter = {0.2f, 0.4f, 0.6f, 1.0f};
    cna_color_init_vector4(quarter, &fromVector);
    cprobe_got(p, "float (%u, %u, %u, %u), to_vector4 (%g, %g, %g, %g), init_vector4 (%u, %u, %u, %u)", fromFloats.r, fromFloats.g, fromFloats.b, fromFloats.a, (double)unit.x,
               (double)unit.y, (double)unit.z, (double)unit.w, fromVector.r, fromVector.g, fromVector.b, fromVector.a);
    CPROBE_CHECK(p, "cna_color_init_float_rgba(1, 0.25, 0, 0.75), to_vector4(255, 0, 51, 255), init_vector4(0.2, 0.4, 0.6, 1)",
                 "(255, 64, 0, 191), (1, 0, 0.2, 1), (51, 102, 153, 255)",
                 SameColor(fromFloats, 255, 64, 0, 191) && Near(unit.x, 1.0f, 1e-6f) && unit.y == 0.0f && Near(unit.z, 0.2f, 1e-6f) && Near(unit.w, 1.0f, 1e-6f) &&
                     SameColor(fromVector, 51, 102, 153, 255));
}

static void PlaneAndRayChecks(CProbe* p)
{
    char text[160];
    uint64_t bytes = 0;

    /* XNA Plane(a, b, c): normal = normalize((b - a) x (c - a)), D = -normal . a. */
    const CNA_Vector3 a = {0, 1, 0}, b = {1, 1, 0}, c = {0, 1, 1};
    CNA_Plane through;
    memset(&through, 0, sizeof through);
    cna_plane_init_points(a, b, c, &through);
    float coordinate = 0.0f;
    const CNA_Vector3 point = {5, 3, 7};
    cna_plane_dot_coordinate(through, point, &coordinate);
    const CNA_Result planeCopied = cna_plane_copy_string(through, text, sizeof text - 1, &bytes);
    cprobe_terminate(text, sizeof text, planeCopied, &bytes);
    int32_t hash = 0, twinHash = 1, movedHash = 0;
    cna_plane_get_hash_code(through, &hash);
    CNA_Plane twin, moved;
    const CNA_Vector3 minusY = {0, -1, 0};
    cna_plane_init_normal_d(minusY, 1.0f, &twin);
    cna_plane_init_normal_d(minusY, 2.0f, &moved);
    cna_plane_get_hash_code(twin, &twinHash);
    cna_plane_get_hash_code(moved, &movedHash);
    CNA_Bool planesEqual = CNA_FALSE, planesDiffer = CNA_FALSE;
    cna_plane_equals(through, twin, &planesEqual);
    cna_plane_not_equals(through, moved, &planesDiffer);
    cprobe_got(p, "\"%s\", dot_coordinate(5, 3, 7) %g; equals (0,-1,0) D 1 %u with hash %d = %d; D 2 not_equals %u, hash %d", text, (double)coordinate, planesEqual, (int)hash, (int)twinHash,
               planesDiffer, (int)movedHash);
    CPROBE_CHECK(p, "cna_plane_init_points((0,1,0), (1,1,0), (0,1,1)): copy_string, dot_coordinate, equals/not_equals, get_hash_code",
                 "\"{Normal:{X:0 Y:-1 Z:0} D:1}\" (XNA's normalize((b-a) x (c-a)), D = -n.a), -2; equal to (0,-1,0) D 1 with the same hash; D 2 differs and hashes differently (CNA's own size_t hash, not XNA's float-bit sum)",
                 strcmp(text, "{Normal:{X:0 Y:-1 Z:0} D:1}") == 0 && coordinate == -2.0f && planesEqual == CNA_TRUE && hash == twinHash && planesDiffer == CNA_TRUE && movedHash != hash);

    /* Normalising (0, 2, 0, 4) divides D by the normal's length; the classification of volumes against y = 2. */
    CNA_Plane raw, normalized, y2;
    cna_plane_init_abcd(0.0f, 2.0f, 0.0f, 4.0f, &raw);
    cna_plane_normalize(raw, &normalized);
    const CNA_Vector3 up = {0, 1, 0};
    cna_plane_init_normal_d(up, -2.0f, &y2);
    CNA_BoundingBox unitBox;
    const CNA_Vector3 minusOne = {-1, -1, -1}, plusOne = {1, 1, 1};
    cna_bounding_box_init_min_max(minusOne, plusOne, &unitBox);
    CNA_BoundingSphere high, onPlane;
    const CNA_Vector3 highCenter = {0, 5, 0}, planeCenter = {0, 2, 0};
    cna_bounding_sphere_init_center_radius(highCenter, 1.0f, &high);
    cna_bounding_sphere_init_center_radius(planeCenter, 1.0f, &onPlane);
    CNA_PlaneIntersectionType boxSide = 99, highSide = 99, onSide = 99;
    cna_plane_intersects_box(y2, unitBox, &boxSide);
    cna_plane_intersects_sphere(y2, high, &highSide);
    cna_plane_intersects_sphere(y2, onPlane, &onSide);
    cprobe_got(p, "normalize -> (%g, %g, %g) D %g; box %u, sphere at y=5 %u, sphere at y=2 %u", (double)normalized.normal.x, (double)normalized.normal.y, (double)normalized.normal.z,
               (double)normalized.d, (unsigned)boxSide, (unsigned)highSide, (unsigned)onSide);
    CPROBE_CHECK(p, "cna_plane_normalize(0, 2, 0, 4); intersects_box/sphere against y = 2", "(0, 1, 0) D 2; BACK(1), FRONT(0), INTERSECTING(2)",
                 NearV3(normalized.normal, 0, 1, 0, 1e-6f) && Near(normalized.d, 2.0f, 1e-6f) && boxSide == CNA_PLANE_INTERSECTION_BACK && highSide == CNA_PLANE_INTERSECTION_FRONT &&
                     onSide == CNA_PLANE_INTERSECTION_INTERSECTING);

    /* Rays: distances to a box, a sphere and a plane along +Z from z = -10, and a miss. */
    CNA_Ray ray, miss;
    const CNA_Vector3 origin = {0, 0, -10}, forward = {0, 0, 1}, sideways = {0, 1, 0};
    cna_ray_init_position_direction(origin, forward, &ray);
    cna_ray_init_position_direction(origin, sideways, &miss);
    CNA_BoundingSphere radius2;
    const CNA_Vector3 zero = {0, 0, 0};
    cna_bounding_sphere_init_center_radius(zero, 2.0f, &radius2);
    CNA_Plane z0;
    cna_plane_init_normal_d(forward, 0.0f, &z0);
    CNA_Bool hitBox = CNA_FALSE, hitSphere = CNA_FALSE, hitPlane = CNA_FALSE, hitMiss = CNA_TRUE;
    float dBox = -1, dSphere = -1, dPlane = -1, dMiss = -1;
    cna_ray_intersects_box(ray, unitBox, &hitBox, &dBox);
    cna_ray_intersects_sphere(ray, radius2, &hitSphere, &dSphere);
    cna_ray_intersects_plane(ray, z0, &hitPlane, &dPlane);
    cna_ray_intersects_box(miss, unitBox, &hitMiss, &dMiss);
    const CNA_Result rayCopied = cna_ray_copy_string(ray, text, sizeof text - 1, &bytes);
    cprobe_terminate(text, sizeof text, rayCopied, &bytes);
    cprobe_got(p, "box %u/%g, sphere %u/%g, plane %u/%g, sideways %u; \"%s\"", hitBox, (double)dBox, hitSphere, (double)dSphere, hitPlane, (double)dPlane, hitMiss, text);
    CPROBE_CHECK(p, "cna_ray_intersects_box/sphere/plane from (0,0,-10) along +Z", "hits at 9, 8 and 10, the sideways ray misses; \"{Position:{X:0 Y:0 Z:-10} Direction:{X:0 Y:0 Z:1}}\"",
                 hitBox == CNA_TRUE && Near(dBox, 9.0f, 1e-5f) && hitSphere == CNA_TRUE && Near(dSphere, 8.0f, 1e-5f) && hitPlane == CNA_TRUE && Near(dPlane, 10.0f, 1e-5f) &&
                     hitMiss == CNA_FALSE && strcmp(text, "{Position:{X:0 Y:0 Z:-10} Direction:{X:0 Y:0 Z:1}}") == 0);
}

static void BoxAndSphereChecks(CProbe* p)
{
    char text[160];
    uint64_t bytes = 0;

    const CNA_Vector3 points[3] = {{1, 2, 3}, {-1, 5, 0}, {4, -2, 1}};
    CNA_BoundingBox fromPoints;
    memset(&fromPoints, 0, sizeof fromPoints);
    const CNA_Result created = cna_bounding_box_create_from_points(points, 3u, &fromPoints);
    CNA_ContainmentType inside = 99, outside = 99, partial = 99;
    const CNA_Vector3 origin = {0, 0, 0}, far = {10, 0, 0};
    cna_bounding_box_contains_point(fromPoints, origin, &inside);
    cna_bounding_box_contains_point(fromPoints, far, &outside);
    CNA_BoundingBox straddling;
    const CNA_Vector3 sMin = {3, 0, 0}, sMax = {6, 1, 1};
    cna_bounding_box_init_min_max(sMin, sMax, &straddling);
    cna_bounding_box_contains_box(fromPoints, straddling, &partial);
    cprobe_got(p, "%u: min (%g, %g, %g) max (%g, %g, %g); origin %u, (10,0,0) %u, straddling box %u", (unsigned)created, (double)fromPoints.min.x, (double)fromPoints.min.y,
               (double)fromPoints.min.z, (double)fromPoints.max.x, (double)fromPoints.max.y, (double)fromPoints.max.z, (unsigned)inside, (unsigned)outside, (unsigned)partial);
    CPROBE_CHECK(p, "cna_bounding_box_create_from_points and contains_point/contains_box", "min (-1, -2, 0) max (4, 5, 3); CONTAINS(1), DISJOINT(0), INTERSECTS(2)",
                 created == CNA_RESULT_SUCCESS && NearV3(fromPoints.min, -1, -2, 0, 0) && NearV3(fromPoints.max, 4, 5, 3, 0) && inside == CNA_CONTAINMENT_CONTAINS &&
                     outside == CNA_CONTAINMENT_DISJOINT && partial == CNA_CONTAINMENT_INTERSECTS);

    /* Corners in XNA's order (front face top-left first, z = max), and a buffer one corner short. */
    CNA_BoundingBox box;
    const CNA_Vector3 bMin = {0, 0, 0}, bMax = {1, 2, 3};
    cna_bounding_box_init_min_max(bMin, bMax, &box);
    CNA_Vector3 corners[8];
    memset(corners, 0, sizeof corners);
    uint64_t count = 0, shortCount = 0;
    const CNA_Result cornerResult = cna_bounding_box_copy_corners(box, corners, 8u, &count);
    CNA_Vector3 shortBuffer[7];
    memset(shortBuffer, 0x7F, sizeof shortBuffer);
    const CNA_Result shortResult = cna_bounding_box_copy_corners(box, shortBuffer, 7u, &shortCount);
    const unsigned char firstByte = ((const unsigned char*)shortBuffer)[0];
    cprobe_got(p, "%u, %llu corners, [0] (%g, %g, %g), [2] (%g, %g, %g), [7] (%g, %g, %g); 7 slots -> %u, needed %llu, first byte 0x%02X", (unsigned)cornerResult,
               (unsigned long long)count, (double)corners[0].x, (double)corners[0].y, (double)corners[0].z, (double)corners[2].x, (double)corners[2].y, (double)corners[2].z,
               (double)corners[7].x, (double)corners[7].y, (double)corners[7].z, (unsigned)shortResult, (unsigned long long)shortCount, firstByte);
    CPROBE_CHECK(p, "cna_bounding_box_copy_corners of (0,0,0)-(1,2,3)", "8 corners: [0] (0, 2, 3), [2] (1, 0, 3), [7] (0, 0, 0); 7 slots refused, nothing written",
                 cornerResult == CNA_RESULT_SUCCESS && count == 8u && NearV3(corners[0], 0, 2, 3, 0) && NearV3(corners[2], 1, 0, 3, 0) && NearV3(corners[7], 0, 0, 0, 0) &&
                     shortResult == CNA_RESULT_BUFFER_TOO_SMALL && shortCount == 8u && firstByte == 0x7Fu);

    CNA_BoundingBox merged;
    cna_bounding_box_create_merged(box, straddling, &merged);
    const CNA_Result boxCopied = cna_bounding_box_copy_string(box, text, sizeof text - 1, &bytes);
    cprobe_terminate(text, sizeof text, boxCopied, &bytes);
    uint64_t sized = 0;
    cna_bounding_box_get_string_size(box, &sized);
    cprobe_got(p, "merged (%g, %g, %g)-(%g, %g, %g); \"%s\" (%llu bytes)", (double)merged.min.x, (double)merged.min.y, (double)merged.min.z, (double)merged.max.x, (double)merged.max.y,
               (double)merged.max.z, text, (unsigned long long)sized);
    CPROBE_CHECK(p, "cna_bounding_box_create_merged and copy_string", "(0, 0, 0)-(6, 2, 3); \"{Min:{X:0 Y:0 Z:0} Max:{X:1 Y:2 Z:3}}\" (37 bytes)",
                 NearV3(merged.min, 0, 0, 0, 0) && NearV3(merged.max, 6, 2, 3, 0) && strcmp(text, "{Min:{X:0 Y:0 Z:0} Max:{X:1 Y:2 Z:3}}") == 0 && sized == 37u);

    /* Spheres: from a box (centre = midpoint, radius = half the diagonal), transform (radius scales), containment. */
    CNA_BoundingBox cube;
    const CNA_Vector3 cMax = {2, 2, 2};
    cna_bounding_box_init_min_max(bMin, cMax, &cube);
    CNA_BoundingSphere fromBox, moved;
    cna_bounding_sphere_create_from_box(cube, &fromBox);
    CNA_Matrix scale, translation, world;
    cna_matrix_create_scale_scalar(2.0f, &scale);
    cna_matrix_create_translation_xyz(1.0f, 0.0f, 0.0f, &translation);
    cna_matrix_multiply(scale, translation, &world);
    CNA_BoundingSphere unit;
    cna_bounding_sphere_init_center_radius(origin, 1.0f, &unit);
    cna_bounding_sphere_transform(unit, world, &moved);
    CNA_ContainmentType small = 99, overlapping = 99;
    CNA_BoundingSphere tiny, offset;
    const CNA_Vector3 tinyCenter = {1, 1, 1}, offsetCenter = {3, 1, 1};
    cna_bounding_sphere_init_center_radius(tinyCenter, 0.5f, &tiny);
    cna_bounding_sphere_init_center_radius(offsetCenter, 1.0f, &offset);
    cna_bounding_sphere_contains_sphere(fromBox, tiny, &small);
    cna_bounding_sphere_contains_sphere(fromBox, offset, &overlapping);
    cprobe_got(p, "from box centre (%g, %g, %g) radius %.6f; transformed centre (%g, %g, %g) radius %g; contains %u, %u", (double)fromBox.center.x, (double)fromBox.center.y,
               (double)fromBox.center.z, (double)fromBox.radius, (double)moved.center.x, (double)moved.center.y, (double)moved.center.z, (double)moved.radius, (unsigned)small,
               (unsigned)overlapping);
    CPROBE_CHECK(p, "cna_bounding_sphere_create_from_box, transform and contains_sphere", "(1, 1, 1) r 1.732051; (1, 0, 0) r 2; CONTAINS(1), INTERSECTS(2)",
                 NearV3(fromBox.center, 1, 1, 1, 1e-6f) && Near(fromBox.radius, 1.7320508f, 1e-5f) && NearV3(moved.center, 1, 0, 0, 1e-6f) && Near(moved.radius, 2.0f, 1e-5f) &&
                     small == CNA_CONTAINMENT_CONTAINS && overlapping == CNA_CONTAINMENT_INTERSECTS);
}

static void FrustumChecks(CProbe* p)
{
    /* A camera at the origin looking down -Z, 90 degree field of view, square, near 1, far 100. */
    CNA_Matrix view, projection, viewProjection;
    const CNA_Vector3 eye = {0, 0, 0}, target = {0, 0, -1}, up = {0, 1, 0};
    cna_matrix_create_look_at(eye, target, up, &view);
    cna_matrix_create_perspective_field_of_view(1.5707963f, 1.0f, 1.0f, 100.0f, &projection);
    cna_matrix_multiply(view, projection, &viewProjection);
    CNA_BoundingFrustum frustum;
    memset(&frustum, 0, sizeof frustum);
    const CNA_Result created = cna_bounding_frustum_init_matrix(viewProjection, &frustum);
    CNA_Plane nearPlane, farPlane;
    memset(&nearPlane, 0, sizeof nearPlane);
    memset(&farPlane, 0, sizeof farPlane);
    cna_bounding_frustum_get_near(frustum, &nearPlane);
    cna_bounding_frustum_get_far(frustum, &farPlane);
    cprobe_got(p, "%u: near (%g, %g, %g) D %g, far (%g, %g, %g) D %g", (unsigned)created, (double)nearPlane.normal.x, (double)nearPlane.normal.y, (double)nearPlane.normal.z,
               (double)nearPlane.d, (double)farPlane.normal.x, (double)farPlane.normal.y, (double)farPlane.normal.z, (double)farPlane.d);
    CPROBE_CHECK(p, "cna_bounding_frustum_init_matrix (fov 90, near 1, far 100): get_near, get_far", "near (0, 0, 1) D 1, far (0, 0, -1) D -100 (outward normals)",
                 created == CNA_RESULT_SUCCESS && NearV3(nearPlane.normal, 0, 0, 1, 1e-5f) && Near(nearPlane.d, 1.0f, 1e-4f) && NearV3(farPlane.normal, 0, 0, -1, 1e-5f) &&
                     Near(farPlane.d, -100.0f, 1e-2f));

    CNA_Vector3 corners[8];
    memset(corners, 0, sizeof corners);
    uint64_t count = 0;
    cna_bounding_frustum_copy_corners(frustum, corners, 8u, &count);
    CNA_ContainmentType ahead = 99, behind = 99, straddle = 99;
    const CNA_Vector3 pointAhead = {0, 0, -50}, pointBehind = {0, 0, -150};
    cna_bounding_frustum_contains_point(frustum, pointAhead, &ahead);
    cna_bounding_frustum_contains_point(frustum, pointBehind, &behind);
    CNA_BoundingBox acrossNear;
    const CNA_Vector3 nMin = {-0.25f, -0.25f, -2.0f}, nMax = {0.25f, 0.25f, 0.0f};
    cna_bounding_box_init_min_max(nMin, nMax, &acrossNear);
    cna_bounding_frustum_contains_box(frustum, acrossNear, &straddle);
    cprobe_got(p, "%llu corners, [0] (%.3f, %.3f, %.3f), [6] (%.2f, %.2f, %.2f); (0,0,-50) %u, (0,0,-150) %u, box across the near plane %u", (unsigned long long)count,
               (double)corners[0].x, (double)corners[0].y, (double)corners[0].z, (double)corners[6].x, (double)corners[6].y, (double)corners[6].z, (unsigned)ahead,
               (unsigned)behind, (unsigned)straddle);
    CPROBE_CHECK(p, "cna_bounding_frustum_copy_corners, contains_point, contains_box",
                 "8 corners, [0] near-left-top (-1, 1, -1), [6] far-right-bottom (100, -100, -100); CONTAINS(1), DISJOINT(0), INTERSECTS(2)",
                 count == 8u && NearV3(corners[0], -1, 1, -1, 1e-3f) && NearV3(corners[6], 100, -100, -100, 0.05f) && ahead == CNA_CONTAINMENT_CONTAINS &&
                     behind == CNA_CONTAINMENT_DISJOINT && straddle == CNA_CONTAINMENT_INTERSECTS);
}

static void VertexChecks(CProbe* p)
{
    char text[200];
    uint64_t bytes = 0;

    uint32_t strideColor = 0, strideNormalTexture = 0, strideTexture = 0;
    cna_vertex_type_get_stride(CNA_VERTEX_TYPE_POSITION_COLOR, &strideColor);
    cna_vertex_type_get_stride(CNA_VERTEX_TYPE_POSITION_NORMAL_TEXTURE, &strideNormalTexture);
    cna_vertex_type_get_stride(CNA_VERTEX_TYPE_POSITION_TEXTURE, &strideTexture);
    CNA_VertexElement elements[4];
    memset(elements, 0, sizeof elements);
    uint64_t count = 0;
    const CNA_Result copied = cna_vertex_type_copy_elements(CNA_VERTEX_TYPE_POSITION_COLOR_TEXTURE, elements, 4u, &count);
    cprobe_got(p, "strides %u/%u/%u; %u: %llu elements (%d,%u,%u,%d) (%d,%u,%u,%d) (%d,%u,%u,%d)", (unsigned)strideColor, (unsigned)strideNormalTexture, (unsigned)strideTexture,
               (unsigned)copied, (unsigned long long)count, (int)elements[0].offset, (unsigned)elements[0].format, (unsigned)elements[0].usage, (int)elements[0].usage_index,
               (int)elements[1].offset, (unsigned)elements[1].format, (unsigned)elements[1].usage, (int)elements[1].usage_index, (int)elements[2].offset,
               (unsigned)elements[2].format, (unsigned)elements[2].usage, (int)elements[2].usage_index);
    CPROBE_CHECK(p, "cna_vertex_type_get_stride and copy_elements(POSITION_COLOR_TEXTURE)",
                 "strides 16/32/20; 3 elements (0,Vector3,Position,0) (12,Color,Color,0) (16,Vector2,TextureCoordinate,0)",
                 strideColor == 16u && strideNormalTexture == 32u && strideTexture == 20u && copied == CNA_RESULT_SUCCESS && count == 3u && elements[0].offset == 0 &&
                     elements[0].format == CNA_VERTEX_ELEMENT_FORMAT_VECTOR3 && elements[0].usage == CNA_VERTEX_ELEMENT_USAGE_POSITION && elements[1].offset == 12 &&
                     elements[1].format == CNA_VERTEX_ELEMENT_FORMAT_COLOR && elements[1].usage == CNA_VERTEX_ELEMENT_USAGE_COLOR && elements[2].offset == 16 &&
                     elements[2].format == CNA_VERTEX_ELEMENT_FORMAT_VECTOR2 && elements[2].usage == CNA_VERTEX_ELEMENT_USAGE_TEXTURE_COORDINATE && elements[2].usage_index == 0);

    const CNA_Result elementCopied = cna_vertex_element_copy_string(elements[1], text, sizeof text - 1, &bytes);
    cprobe_terminate(text, sizeof text, elementCopied, &bytes);
    CNA_Bool same = CNA_FALSE, different = CNA_FALSE;
    CNA_VertexElement twin = elements[1];
    CNA_VertexElement other = elements[1];
    other.usage_index = 1;
    cna_vertex_element_equals(elements[1], twin, &same);
    cna_vertex_element_not_equals(elements[1], other, &different);
    cprobe_got(p, "\"%s\", equals %u, not_equals(usage index 1) %u", text, (unsigned)same, (unsigned)different);
    CPROBE_CHECK(p, "cna_vertex_element_copy_string, equals, not_equals", "\"{Offset:12 Format:Color Usage:Color UsageIndex:0}\", 1, 1",
                 strcmp(text, "{Offset:12 Format:Color Usage:Color UsageIndex:0}") == 0 && same == CNA_TRUE && different == CNA_TRUE);

    CNA_VertexValue value, defaulted;
    memset(&value, 0, sizeof value);
    memset(&defaulted, 0xAB, sizeof defaulted);
    const CNA_Result defaultResult = cna_vertex_value_init_default(CNA_VERTEX_TYPE_POSITION_COLOR, &defaulted);
    value.position_color.position.x = 1.0f;
    value.position_color.position.y = 2.0f;
    value.position_color.position.z = 3.0f;
    value.position_color.color = CNA_COLOR_RED;
    const CNA_Result valueCopied = cna_vertex_value_copy_string(CNA_VERTEX_TYPE_POSITION_COLOR, &value, text, sizeof text - 1, &bytes);
    cprobe_terminate(text, sizeof text, valueCopied, &bytes);
    CNA_Bool equalsDefault = CNA_TRUE;
    cna_vertex_value_equals(CNA_VERTEX_TYPE_POSITION_COLOR, &value, &defaulted, &equalsDefault);
    const CNA_VertexPositionColor d = defaulted.position_color;
    cprobe_got(p, "default %u: (%g, %g, %g) (%u, %u, %u, %u); \"%s\"; equals default %u", (unsigned)defaultResult, (double)d.position.x, (double)d.position.y, (double)d.position.z, d.color.r,
               d.color.g, d.color.b, d.color.a, text, (unsigned)equalsDefault);
    CPROBE_CHECK(p, "cna_vertex_value_init_default, copy_string, equals (VertexPositionColor)",
                 "all-zero default; \"{Position:{X:1 Y:2 Z:3} Color:{R:255 G:0 B:0 A:255}}\"; not equal to the default",
                 defaultResult == CNA_RESULT_SUCCESS && NearV3(d.position, 0, 0, 0, 0) && SameColor(d.color, 0, 0, 0, 0) &&
                     strcmp(text, "{Position:{X:1 Y:2 Z:3} Color:{R:255 G:0 B:0 A:255}}") == 0 && equalsDefault == CNA_FALSE);
}

static void MathChecks(CProbe* p)
{
    /* MathHelper, from XNA 4.0: exact interpolation results. */
    float lerp = 0, smooth = 0, catmull = 0, hermite = 0, barycentric = 0, degrees = 0, radians = 0, wrapped = 0, clamped = 0, distance = 0;
    cna_math_lerp(2.0f, 10.0f, 0.25f, &lerp);
    cna_math_smooth_step(0.0f, 1.0f, 0.5f, &smooth);
    cna_math_catmull_rom(0.0f, 1.0f, 2.0f, 3.0f, 0.5f, &catmull);
    cna_math_hermite(0.0f, 0.0f, 10.0f, 0.0f, 0.5f, &hermite);
    cna_math_barycentric(1.0f, 2.0f, 3.0f, 0.5f, 0.25f, &barycentric);
    cna_math_to_degrees(3.14159265f, &degrees);
    cna_math_to_radians(90.0f, &radians);
    cna_math_wrap_angle(4.71238898f, &wrapped);
    cna_math_clamp_float(5.0f, 0.0f, 3.0f, &clamped);
    cna_math_distance(-2.0f, 3.0f, &distance);
    int32_t clampedInt = 0, msaa6 = -1, msaa1 = -1;
    cna_math_clamp_int32(-7, -3, 3, &clampedInt);
    cna_math_closest_msaa_power(6, &msaa6);
    cna_math_closest_msaa_power(1, &msaa1);
    float minimum = 0, maximum = 0;
    cna_math_min(2.0f, -1.0f, &minimum);
    cna_math_max(2.0f, -1.0f, &maximum);
    cprobe_got(p, "lerp %g, smooth step %g, Catmull-Rom %g, Hermite %g, barycentric %g, degrees %.4f, radians %.6f, wrap(3pi/2) %.6f, clamp %g, distance %g, clamp int %d, MSAA 6 -> %d and 1 -> %d, min %g, max %g",
               (double)lerp, (double)smooth, (double)catmull, (double)hermite, (double)barycentric, (double)degrees, (double)radians, (double)wrapped, (double)clamped, (double)distance,
               (int)clampedInt, (int)msaa6, (int)msaa1, (double)minimum, (double)maximum);
    CPROBE_CHECK(p, "cna_math_lerp/smooth_step/catmull_rom/hermite/barycentric/to_degrees/to_radians/wrap_angle/clamp_float/distance/clamp_int32/closest_msaa_power/min/max",
                 "4, 0.5, 1.5, 5, 2 (XNA MathHelper), 180, pi/2, -pi/2 (wrapped into (-pi, pi]), 3, 5, -3, 4 and 0 (1x is not multisampling), -1, 2",
                 lerp == 4.0f && smooth == 0.5f && catmull == 1.5f && hermite == 5.0f && barycentric == 2.0f && fabsf(degrees - 180.0f) < 1e-3f && fabsf(radians - 1.5707963f) < 1e-6f &&
                     fabsf(wrapped + 1.5707963f) < 1e-5f && clamped == 3.0f && distance == 5.0f && clampedInt == -3 && msaa6 == 4 && msaa1 == 0 && minimum == -1.0f && maximum == 2.0f);

    /* Point and Rectangle: XNA's sums for hashes, its ToString formats and Rectangle's half-open containment. */
    CNA_Point point = {0, 0}, offset = {0, 0}, sum = {0, 0}, center = {0, 0};
    cna_point_init_xy(3, 4, &point);
    cna_point_init_xy(1, 2, &offset);
    cna_point_add(point, offset, &sum);
    int32_t pointHash = 0, rectangleHash = 0;
    cna_point_get_hash_code(point, &pointHash);
    char pointText[32], rectangleText[64];
    uint64_t pointBytes = 0, rectangleBytes = 0;
    const CNA_Result pointCopied = cna_point_copy_string(point, pointText, sizeof pointText - 1, &pointBytes);
    cprobe_terminate(pointText, sizeof pointText, pointCopied, &pointBytes);
    CNA_Rectangle rectangle, other, intersection, both;
    cna_rectangle_init_xywh(10, 20, 30, 40, &rectangle);
    cna_rectangle_init_xywh(30, 50, 20, 20, &other);
    int32_t right = 0, bottom = 0;
    cna_rectangle_get_right(rectangle, &right);
    cna_rectangle_get_bottom(rectangle, &bottom);
    cna_rectangle_get_center(rectangle, &center);
    CNA_Bool inside = CNA_FALSE, edge = CNA_TRUE, overlaps = CNA_FALSE;
    cna_rectangle_contains_xy(rectangle, 39, 59, &inside);
    cna_rectangle_contains_xy(rectangle, 40, 60, &edge);
    cna_rectangle_intersects(rectangle, other, &overlaps);
    cna_rectangle_intersect(rectangle, other, &intersection);
    cna_rectangle_union(rectangle, other, &both);
    cna_rectangle_get_hash_code(rectangle, &rectangleHash);
    const CNA_Result rectangleCopied = cna_rectangle_copy_string(rectangle, rectangleText, sizeof rectangleText - 1, &rectangleBytes);
    cprobe_terminate(rectangleText, sizeof rectangleText, rectangleCopied, &rectangleBytes);
    CNA_Rectangle inflated = rectangle;
    cna_rectangle_inflate(&inflated, 2, 3);
    cprobe_got(p, "point sum (%d, %d), hash %d, \"%s\"; rectangle right %d bottom %d centre (%d, %d), contains (39,59) %u (40,60) %u, intersects %u -> %d,%d %dx%d, union %d,%d %dx%d, hash %d, \"%s\", inflated %d,%d %dx%d",
               (int)sum.x, (int)sum.y, (int)pointHash, pointText, (int)right, (int)bottom, (int)center.x, (int)center.y, inside, edge, overlaps, (int)intersection.x, (int)intersection.y,
               (int)intersection.width, (int)intersection.height, (int)both.x, (int)both.y, (int)both.width, (int)both.height, (int)rectangleHash, rectangleText, (int)inflated.x, (int)inflated.y,
               (int)inflated.width, (int)inflated.height);
    CPROBE_CHECK(p, "cna_point_init_xy/add/get_hash_code/copy_string, cna_rectangle_init_xywh/get_right/get_bottom/get_center/contains_xy/intersects/intersect/union/get_hash_code/copy_string/inflate",
                 "(4, 6), hash 7 (X + Y), \"{X:3 Y:4}\"; 40, 60, (25, 40); (39,59) inside and (40,60) outside (half-open); intersection 30,50 10x10, union 10,20 40x50; hash 100 (the sum of the fields); \"{X:10 Y:20 Width:30 Height:40}\"; inflated 8,17 34x46",
                 sum.x == 4 && sum.y == 6 && pointHash == 7 && strcmp(pointText, "{X:3 Y:4}") == 0 && right == 40 && bottom == 60 && center.x == 25 && center.y == 40 && inside == CNA_TRUE &&
                     edge == CNA_FALSE && overlaps == CNA_TRUE && intersection.x == 30 && intersection.y == 50 && intersection.width == 10 && intersection.height == 10 && both.x == 10 &&
                     both.y == 20 && both.width == 40 && both.height == 50 && rectangleHash == 100 && strcmp(rectangleText, "{X:10 Y:20 Width:30 Height:40}") == 0 && inflated.x == 8 &&
                     inflated.y == 17 && inflated.width == 34 && inflated.height == 46);
}

void museum_c_probe_values(CProbe* probe)
{
    MathChecks(probe);
    ColorChecks(probe);
    PlaneAndRayChecks(probe);
    BoxAndSphereChecks(probe);
    FrustumChecks(probe);
    VertexChecks(probe);
}
