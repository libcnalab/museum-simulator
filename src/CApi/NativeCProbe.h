/* The museum's C17 client of CNA's native C ABI (see NativeCProbe.c). Usable from C and C++. */
#ifndef MUSEUM_NATIVE_C_PROBE_H
#define MUSEUM_NATIVE_C_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/** Receives one check: its name, the expected result, what the C API returned, and 1 for a pass. */
typedef void (*MuseumCCheck)(void* user, const char* name, const char* expected, const char* got, int pass);

/** Runs every check through the C ABI and returns the number that failed. */
int museum_c_api_run(MuseumCCheck report, void* user);

#ifdef __cplusplus
}
#endif

#endif
