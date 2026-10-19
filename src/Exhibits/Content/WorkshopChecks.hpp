#pragma once

// The Pipeline Workshop's check list: every room of the wing runs one mode, and every result of a mode is one line
//     [museum][check] <room> <name>: PASS|FAIL|INFO (expected ..., got ...)
// The modes live in their own files (WorkshopTypeWriterChecks.cpp, ...) and only see this list and the museum's
// services, so the room (PipelineChecksLab) stays one class with one table of modes.
#include <cstdio>
#include <exception>
#include <functional>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "Diagnostics/Log.hpp"

struct Services;

namespace Workshop {

struct Check {
    std::string name, expected, got;
    bool pass = false, informational = false;
};

class Checks {
public:
    explicit Checks(std::string room) : room_(std::move(room)) {}

    void Add(const std::string& name, const std::string& expected, const std::string& got, bool pass)
    {
        Record({name, expected, got, pass, false});
    }

    // Something the room reports but does not judge: an unavailable codec, what a context recorded, an API that does not exist.
    void Info(const std::string& name, const std::string& expected, const std::string& got) { Record({name, expected, got, false, true}); }

    // A refusal the contract requires: PASS only when the named exception type is what comes out of the call.
    template <typename TException, typename F>
    void ExpectThrow(const std::string& name, const std::string& expected, F&& call)
    {
        if constexpr (std::is_same_v<TException, std::exception>) {
            try {
                call();
                Add(name, expected, "no exception", false);
            } catch (const std::exception& e) {
                Add(name, expected, Short(std::string("thrown: ") + e.what()), true);
            }
        } else {
            try {
                call();
                Add(name, expected, "no exception", false);
            } catch (const TException& e) {
                Add(name, expected, Short(std::string("thrown: ") + e.what()), true);
            } catch (const std::exception& e) {
                Add(name, expected, Short(std::string("other exception (") + typeid(e).name() + "): " + e.what()), false);
            }
        }
    }

    // Runs one group of checks; an exception that escapes it is itself a failed check, and the next group still runs.
    template <typename F>
    void Group(const std::string& name, F&& body)
    {
        try {
            body();
        } catch (const std::exception& e) {
            Add(name + ": the group itself", "no exception", Short(std::string(typeid(e).name()) + ": " + e.what()), false);
        }
    }

    [[nodiscard]] const std::vector<Check>& List() const { return list_; }

    static std::string Short(const std::string& s, std::size_t limit = 220)
    {
        std::string out = s.size() > limit ? s.substr(0, limit - 3) + "..." : s;
        for (char& c : out)
            if (c == '\n' || c == '\r') c = ' ';
        return out;
    }

private:
    void Record(Check c)
    {
        Museum::Log("check", "%s %s: %s (expected %s, got %s)", room_.c_str(), c.name.c_str(), c.informational ? "INFO" : (c.pass ? "PASS" : "FAIL"), c.expected.c_str(), c.got.c_str());
        list_.push_back(std::move(c));
    }

    std::string room_;
    std::vector<Check> list_;
};

inline std::string F(double v, const char* format = "%.6g")
{
    char buf[64];
    std::snprintf(buf, sizeof buf, format, v);
    return buf;
}

inline std::string Yes(bool b) { return b ? "yes" : "no"; }

// One function per mode. Each builds its own input, runs CNA's public API and records exact checks. Pipeline modes exist only with
// MUSEUM_PIPELINE; the "xml" mode needs no pipeline.
#ifdef MUSEUM_PIPELINE
void ProcessorContextChecks(Checks& checks, Services& services);
void TypeWriterChecks(Checks& checks, Services& services);
void MaterialProcessorChecks(Checks& checks, Services& services);
void ImporterChecks(Checks& checks, Services& services);
void CnbLoaderChecks(Checks& checks, Services& services);
void CnbLoaderLeave();
void GltfImportChecks(Checks& checks, Services& services);
#endif
void XmlSerializerChecks(Checks& checks, Services& services);

}  // namespace Workshop
