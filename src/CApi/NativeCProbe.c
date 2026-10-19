/*
 * The museum's native C client: a C17 translation unit that talks to CNA only through its exported C ABI
 * (CNA/C/*.h, the cna_c_api library), never through C++. It covers the value and handle families that need no
 * game of their own (the museum already runs one): the ABI version, result codes and thread-local error
 * diagnostics, vectors, matrices, quaternions, colours, half floats, and a curve handle's whole lifetime
 * including what a stale handle returns. Every check is reported through a callback; the museum's C++ room
 * draws and logs them.
 *
 * Compiled only when MUSEUM_C_API is ON (it needs CNA_BUILD_C_API).
 */
#include "CApi/NativeCProbe.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "CNA/C/abi.h"
#include "CNA/C/core.h"
#include "CNA/C/curve.h"
#include "CNA/C/math.h"
#include "CNA/C/matrix.h"
#include "CNA/C/packed_vectors.h"
#include "CNA/C/quaternion.h"
#include "CNA/C/vectors.h"

static void Report(MuseumCCheck report, void* user, const char* name, const char* expected, const char* got, int pass)
{
    report(user, name, expected, got, pass);
}

/* The C ABI copies the message's bytes without a terminator and reports their count; the terminator is ours. */
static void LastMessage(char* out, size_t capacity)
{
    uint64_t bytes = 0;
    out[0] = '\0';
    if (cna_error_copy_last_message(out, (uint64_t)(capacity - 1), &bytes) != CNA_RESULT_SUCCESS) {
        snprintf(out, capacity, "(no message)");
        return;
    }
    out[bytes < capacity - 1 ? (size_t)bytes : capacity - 1] = '\0';
}

int museum_c_api_run(MuseumCCheck report, void* user)
{
    char got[256];
    char message[160];
    int failures = 0;
#define CHECK(name, expected, condition)                    \
    do {                                                    \
        const int pass_ = (condition) ? 1 : 0;              \
        failures += pass_ ? 0 : 1;                          \
        Report(report, user, name, expected, got, pass_);   \
    } while (0)

    /* The library the process loaded must be the ABI these headers describe. */
    const uint32_t abi = cna_get_abi_version();
    snprintf(got, sizeof got, "%u.%u.%u", (unsigned)(abi >> 16), (unsigned)((abi >> 8) & 0xFFu), (unsigned)(abi & 0xFFu));
    CHECK("cna_get_abi_version() equals CNA_ABI_VERSION", "the header's version", abi == CNA_ABI_VERSION);

    /* Vectors: an exact cross and dot product. */
    CNA_Vector3 x, y, z;
    float dot = -1.0f;
    cna_vector3_init_xyz(1.0f, 0.0f, 0.0f, &x);
    cna_vector3_init_xyz(0.0f, 1.0f, 0.0f, &y);
    const CNA_Result crossResult = cna_vector3_cross(x, y, &z);
    cna_vector3_dot(x, y, &dot);
    snprintf(got, sizeof got, "cross (%g, %g, %g), dot %g", (double)z.x, (double)z.y, (double)z.z, (double)dot);
    CHECK("cna_vector3_cross(UnitX, UnitY) and dot", "cross (0, 0, 1), dot 0", crossResult == CNA_RESULT_SUCCESS && z.x == 0.0f && z.y == 0.0f && z.z == 1.0f && dot == 0.0f);

    /* Matrices: (rotation then translation) times its inverse is the identity; a point lands where expected. */
    CNA_Matrix rotation, translation, world, inverse, product;
    cna_matrix_create_rotation_y(1.5707963f, &rotation);
    cna_matrix_create_translation_xyz(1.0f, 2.0f, 3.0f, &translation);
    cna_matrix_multiply(rotation, translation, &world);
    const CNA_Result invertResult = cna_matrix_invert(world, &inverse);
    cna_matrix_multiply(world, inverse, &product);
    const float* p = &product.m11;
    float worst = 0.0f;
    for (int i = 0; i < 16; ++i) {
        const float expected = (i % 5 == 0) ? 1.0f : 0.0f;
        const float error = fabsf(p[i] - expected);
        if (error > worst) worst = error;
    }
    snprintf(got, sizeof got, "worst element error %.2e", (double)worst);
    CHECK("cna_matrix_invert: world * inverse is the identity", "every element within 1e-5", invertResult == CNA_RESULT_SUCCESS && worst < 1e-5f);
    CNA_Vector3 transformed;
    cna_vector3_transform_matrix(x, world, &transformed);
    snprintf(got, sizeof got, "(%.4f, %.4f, %.4f)", (double)transformed.x, (double)transformed.y, (double)transformed.z);
    CHECK("cna_vector3_transform_matrix(UnitX, rotY(90) * T(1,2,3))", "(1, 2, 2): UnitX turned to -Z, then moved",
          fabsf(transformed.x - 1.0f) < 1e-5f && fabsf(transformed.y - 2.0f) < 1e-5f && fabsf(transformed.z - 2.0f) < 1e-5f);

    /* Quaternions: a rotation from yaw, pitch and roll has unit length. */
    CNA_Quaternion q;
    float length = 0.0f;
    cna_quaternion_create_from_yaw_pitch_roll(0.4f, 0.2f, -0.1f, &q);
    cna_quaternion_length(q, &length);
    snprintf(got, sizeof got, "length %.6f", (double)length);
    CHECK("cna_quaternion_create_from_yaw_pitch_roll has unit length", "1 within 1e-6", fabsf(length - 1.0f) < 1e-6f);

    /* Half floats: exact IEEE 754 binary16 encodings. */
    uint16_t one = 0, minusTwo = 0;
    cna_half_from_single(1.0f, &one);
    cna_half_from_single(-2.0f, &minusTwo);
    snprintf(got, sizeof got, "0x%04X, 0x%04X", (unsigned)one, (unsigned)minusTwo);
    CHECK("cna_half_from_single(1.0) and (-2.0)", "0x3C00, 0xC000", one == 0x3C00u && minusTwo == 0xC000u);

    /* A handle's lifetime: create, use, destroy, and a stale handle afterwards. */
    CNA_CurveHandle curve = 0;
    CNA_CurveKeyCollectionHandle keys = 0;
    CNA_CurveKey key0, key1;
    float middle = -1.0f;
    const CNA_Result created = cna_curve_create(&curve);
    cna_curve_get_keys(curve, &keys);
    cna_curve_key_init_position_value(0.0f, 0.0f, &key0);
    cna_curve_key_init_position_value(1.0f, 10.0f, &key1);
    cna_curve_key_collection_add(keys, key0);
    cna_curve_key_collection_add(keys, key1);
    const CNA_Result evaluated = cna_curve_evaluate(curve, 0.5f, &middle);
    snprintf(got, sizeof got, "create %u, evaluate %u, value %g", (unsigned)created, (unsigned)evaluated, (double)middle);
    CHECK("cna_curve_create, two keys, cna_curve_evaluate(0.5)", "success and 5 (linear between 0 and 10)", created == CNA_RESULT_SUCCESS && evaluated == CNA_RESULT_SUCCESS && middle == 5.0f);
    const CNA_Result destroyed = cna_curve_destroy(curve);
    float after = -1.0f;
    const CNA_Result stale = cna_curve_evaluate(curve, 0.5f, &after);
    CNA_ErrorInfo info;
    memset(&info, 0, sizeof info);
    info.struct_size = (uint32_t)sizeof info;
    info.struct_version = 1u;
    const CNA_Result infoResult = cna_error_get_last_info(&info);
    LastMessage(message, sizeof message);
    snprintf(got, sizeof got, "destroy %u, stale evaluate %u, last info %u/category %u, \"%.70s\"", (unsigned)destroyed, (unsigned)stale, (unsigned)infoResult, (unsigned)info.category, message);
    CHECK("evaluating a destroyed curve handle", "CNA_RESULT_INVALID_HANDLE with a handle-category diagnostic",
          destroyed == CNA_RESULT_SUCCESS && stale == CNA_RESULT_INVALID_HANDLE && infoResult == CNA_RESULT_SUCCESS && info.result == CNA_RESULT_INVALID_HANDLE &&
              info.category == CNA_ERROR_CATEGORY_HANDLE && message[0] != '\0');

    /* A contract violation: a null output pointer is an argument error, never a crash. */
    const CNA_Result nullOut = cna_vector3_dot(x, y, NULL);
    LastMessage(message, sizeof message);
    snprintf(got, sizeof got, "result %u, \"%.80s\"", (unsigned)nullOut, message);
    CHECK("cna_vector3_dot(..., NULL)", "CNA_RESULT_INVALID_ARGUMENT", nullOut == CNA_RESULT_INVALID_ARGUMENT);
#undef CHECK
    return failures;
}
