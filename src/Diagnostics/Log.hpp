#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <utility>

namespace Museum {

// The most recent log lines with a running sequence number, so a script can wait for one (await:TEXT).
struct RecentLog {
    std::mutex mutex;
    std::deque<std::pair<std::uint64_t, std::string>> lines;
    std::uint64_t sequence = 0;
};
inline RecentLog& Recent()
{
    static RecentLog log;
    return log;
}

// Warnings and notes go to stderr so they never interleave with CNA's own
// stdout logging. Printf-style on purpose: no formatting library.
inline void Log(const char* tag, const char* format, ...)
{
    va_list args, again;
    va_start(args, format);
    va_copy(again, args);
    const int size = std::vsnprintf(nullptr, 0, format, args);
    va_end(args);
    std::string text(size > 0 ? static_cast<std::size_t>(size) : 0u, '\0');
    if (size > 0) std::vsnprintf(text.data(), text.size() + 1, format, again);
    va_end(again);
    std::fprintf(stderr, "[museum][%s] %s\n", tag, text.c_str());
    RecentLog& recent = Recent();
    std::lock_guard<std::mutex> lock(recent.mutex);
    recent.lines.emplace_back(++recent.sequence, std::string("[museum][") + tag + "] " + text);
    while (recent.lines.size() > 512) recent.lines.pop_front();
}

// True when a line containing TEXT was logged with a sequence number above AFTER.
inline bool LoggedSince(std::uint64_t after, const std::string& text)
{
    RecentLog& recent = Recent();
    std::lock_guard<std::mutex> lock(recent.mutex);
    for (const auto& [sequence, line] : recent.lines)
        if (sequence > after && line.find(text) != std::string::npos) return true;
    return false;
}

inline std::uint64_t LogSequence()
{
    RecentLog& recent = Recent();
    std::lock_guard<std::mutex> lock(recent.mutex);
    return recent.sequence;
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
