/* Shared plumbing of the museum's C17 clients of CNA's C ABI (NativeCProbe.c and the CProbe*.c families, and the
 * separate C game probe under src/CApi/GameProbe/). C only: nothing here touches C++. */
#ifndef MUSEUM_C_PROBE_H
#define MUSEUM_C_PROBE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CApi/NativeCProbe.h"
#include "CNA/C/abi.h"
#include "CNA/C/core.h"

/** One run of checks: where to report them, how many failed, and the "got" text of the check being built. */
typedef struct CProbe {
    MuseumCCheck report;
    void* user;
    int failures;
    int checks;
    char got[640];
} CProbe;

/** Reports one check with the text currently in probe->got. */
void cprobe_check(CProbe* probe, const char* name, const char* expected, int pass);

/** Formats probe->got (printf style). */
void cprobe_got(CProbe* probe, const char* format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/** Terminates a count-then-copy string: the C ABI copies bytes without a terminator and reports their count. */
const char* cprobe_terminate(char* buffer, size_t capacity, CNA_Result result, const uint64_t* bytes);

/** Copies the calling thread's last C API error message into buffer (terminated). */
const char* cprobe_last_message(char* buffer, size_t capacity);

/** A string view over a C string. */
CNA_StringView cprobe_view(const char* text);

/** The bit pattern of a float as .NET's Single.GetHashCode reports it (0 for both zeros). */
int32_t cprobe_float_hash(float value);

/** Adds hash codes with .NET's wrapping int arithmetic. */
int32_t cprobe_hash_add(int32_t a, int32_t b);

/* The families. Each runs its checks and reports them through the probe. */
void museum_c_probe_values(CProbe* probe);    /* color, named_colors, math_values, geometry, graphics3d, vertex_values */
void museum_c_probe_core(CProbe* probe);      /* core.h extensions, core_ext.h */
void museum_c_probe_content(CProbe* probe);   /* cnb.h, content_readers.h */
void museum_c_probe_platform(CProbe* probe);  /* storage, media_library, sensors, devices */
void museum_c_probe_gamer(CProbe* probe);     /* gamer_services, net, net_gamers, net_sessions */
void museum_c_probe_input(CProbe* probe);     /* the input_*.h value helpers */

#define CPROBE_CHECK(probe, name, expected, condition) cprobe_check((probe), (name), (expected), (condition) ? 1 : 0)

#endif
