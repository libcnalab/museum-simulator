#pragma once

#include <cstdarg>
#include <cstdio>

namespace Museum {

// Warnings and notes go to stderr so they never interleave with CNA's own
// stdout logging. Printf-style on purpose: no allocation, no formatting library.
inline void Log(const char* tag, const char* format, ...)
{
    std::fprintf(stderr, "[museum][%s] ", tag);
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
}

// An exhibit caught an exception it did not expect (not a refusal it demonstrates on purpose) and carried on:
// logged as an exhibit error, so tours, validators and execution evidence treat it as a failure, and counted into
// the "exhibit errors" of the stats line next to the exceptions MuseumWorld catches from exhibit hooks.
inline int& ReportedExhibitErrorCount()
{
    static int count = 0;
    return count;
}

inline void ReportExhibitError(const char* room, const char* what)
{
    ++ReportedExhibitErrorCount();
    Log("exhibit-error", "%s caught: %s", room, what);
}

}  // namespace Museum
