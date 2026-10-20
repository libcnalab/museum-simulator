/* Shared plumbing of the museum's C17 clients of CNA's C ABI; see CProbe.h. */
#include "CApi/CProbe.h"

#include <stdarg.h>

void cprobe_check(CProbe* probe, const char* name, const char* expected, int pass)
{
    probe->checks += 1;
    probe->failures += pass ? 0 : 1;
    probe->report(probe->user, name, expected, probe->got, pass);
    probe->got[0] = '\0';
}

void cprobe_got(CProbe* probe, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(probe->got, sizeof probe->got, format, arguments);
    va_end(arguments);
}

const char* cprobe_terminate(char* buffer, size_t capacity, CNA_Result result, const uint64_t* bytes)
{
    if (capacity == 0) return "";
    if (result != CNA_RESULT_SUCCESS) {
        buffer[0] = '\0';
        return buffer;
    }
    buffer[*bytes < capacity - 1 ? (size_t)*bytes : capacity - 1] = '\0';
    return buffer;
}

const char* cprobe_last_message(char* buffer, size_t capacity)
{
    uint64_t bytes = 0;
    const CNA_Result result = cna_error_copy_last_message(buffer, (uint64_t)(capacity - 1), &bytes);
    if (result != CNA_RESULT_SUCCESS) {
        snprintf(buffer, capacity, "(no message: %u)", (unsigned)result);
        return buffer;
    }
    return cprobe_terminate(buffer, capacity, result, &bytes);
}

CNA_StringView cprobe_view(const char* text)
{
    CNA_StringView view;
    view.data = text;
    view.byte_length = text != NULL ? (uint64_t)strlen(text) : 0u;
    return view;
}

int32_t cprobe_float_hash(float value)
{
    if (value == 0.0f) return 0;
    int32_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

int32_t cprobe_hash_add(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a + (uint32_t)b);
}
