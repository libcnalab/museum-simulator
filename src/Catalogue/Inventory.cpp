#include "Catalogue/Inventory.hpp"

#include <filesystem>
#include <iomanip>
#include <set>
#include <sstream>
#include <string_view>

#include "Exhibits/ExhibitRegistry.hpp"

namespace {

std::string JsonString(std::string_view value)
{
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<unsigned int>(c) << std::dec;
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    out << '"';
    return out.str();
}

const char* OriginName(ApiOrigin origin)
{
    switch (origin) {
        case ApiOrigin::Xna: return "xna";
        case ApiOrigin::CnaExtension: return "cna-extension";
        case ApiOrigin::Application: return "application";
    }
    return "unknown";
}

}  // namespace

MuseumInventory ComputeInventory(const MuseumPlan& m)
{
    MuseumInventory inv;
    inv.floors = static_cast<int>(m.floors.size());
    inv.wings = static_cast<int>(m.wings.size());
    inv.cells = static_cast<int>(m.rooms.size());
    std::set<std::string> kinds;
    for (const RoomDef& r : m.rooms) {
        if (r.kind != CellKind::Room) continue;
        ++inv.rooms;
        ++inv.roomsPerFloor[r.floor];
        if (r.openRoof) ++inv.outdoorRooms;
        if (!r.exhibit.empty()) kinds.insert(r.exhibit);
        for (const std::string& api : r.apis) ++inv.apiCounts[api];
        switch (r.origin) {
            case ApiOrigin::Xna: ++inv.xnaRooms; break;
            case ApiOrigin::CnaExtension: ++inv.extensionRooms; break;
            default: ++inv.applicationRooms; break;
        }
    }
    inv.exhibitKinds = static_cast<int>(kinds.size());
    return inv;
}

std::string InventoryAsText(const MuseumPlan& m)
{
    const MuseumInventory inv = ComputeInventory(m);
    std::ostringstream out;
    out << "floors:          " << inv.floors << "\n"
        << "wings:           " << inv.wings << "\n"
        << "exhibit rooms:   " << inv.rooms << "  (" << inv.outdoorRooms << " outdoors)\n"
        << "walkable cells:  " << inv.cells << "  (rooms, galleries, concourses, halls, elevators, stairs)\n"
        << "exhibit kinds:   " << inv.exhibitKinds << "\n"
        << "origin:          " << inv.xnaRooms << " XNA 4.0, " << inv.extensionRooms << " CNA extension, " << inv.applicationRooms << " application\n"
        << "API names:       " << inv.apiCounts.size() << "\n";
    for (const FloorDef& f : m.floors) {
        const auto it = inv.roomsPerFloor.find(f.index);
        out << "  floor " << f.index << "  " << (it == inv.roomsPerFloor.end() ? 0 : it->second) << " rooms  " << f.name << "\n";
    }
    return out.str();
}

std::string InventoryAsJson(const MuseumPlan& m)
{
    const MuseumInventory inv = ComputeInventory(m);
    std::ostringstream out;
    out << "{\n  \"floors\": " << inv.floors << ",\n  \"wings\": " << inv.wings << ",\n  \"rooms\": " << inv.rooms
        << ",\n  \"outdoorRooms\": " << inv.outdoorRooms << ",\n  \"cells\": " << inv.cells << ",\n  \"exhibitKinds\": "
        << inv.exhibitKinds << ",\n  \"xnaRooms\": " << inv.xnaRooms << ",\n  \"extensionRooms\": " << inv.extensionRooms
        << ",\n  \"applicationRooms\": " << inv.applicationRooms << ",\n  \"apis\": {";
    bool first = true;
    for (const auto& [api, n] : inv.apiCounts) {
        out << (first ? "\n    \"" : ",\n    \"") << api << "\": " << n;
        first = false;
    }
    out << "\n  }\n}\n";
    return out.str();
}

std::string MapAsMarkdown(const MuseumPlan& m)
{
    const MuseumInventory inv = ComputeInventory(m);
    std::ostringstream out;
    out << inv.floors << " floors, " << inv.wings << " wings, " << inv.rooms << " exhibit rooms (" << inv.outdoorRooms << " open to the sky), " << inv.cells << " walkable cells in all.\n\n";
    for (const FloorDef& f : m.floors) {
        out << "### Floor " << f.index << ": " << f.name << "\n\n" << f.tagline << "\n\n";
        for (const WingDef& w : m.wings) {
            if (w.floor != f.index) continue;
            out << "**" << w.name << "** - " << w.tagline << "\n\n| Room | Key | Title | Origin | APIs |\n|---|---|---|---|---|\n";
            for (const RoomDef& r : m.rooms) {
                if (r.kind != CellKind::Room || r.floor != f.index || r.wing != w.index) continue;
                out << "| " << r.number << " | `" << r.key << "` | " << r.title << " | " << (r.origin == ApiOrigin::Xna ? "XNA 4.0" : "CNA extension") << " | ";
                for (std::size_t i = 0; i < r.apis.size(); ++i) out << (i ? ", " : "") << r.apis[i];
                out << " |\n";
            }
            out << "\n";
        }
    }
    return out.str();
}

std::string CoverageAsMarkdown(const MuseumPlan& m)
{
    std::map<std::string, std::vector<std::string>> rooms;
    for (const RoomDef& r : m.rooms) {
        if (r.kind != CellKind::Room) continue;
        for (const std::string& api : r.apis) rooms[api].push_back(r.key);
    }
    std::ostringstream out;
    out << rooms.size() << " distinct API names are claimed by at least one room.\n\n| API | Rooms |\n|---|---|\n";
    for (const auto& [api, keys] : rooms) {
        out << "| " << api << " | ";
        for (std::size_t i = 0; i < keys.size(); ++i) out << (i ? ", " : "") << "`" << keys[i] << "`";
        out << " |\n";
    }
    return out.str();
}

std::string CoverageAsJson(const MuseumPlan& m)
{
    std::map<std::string, std::vector<std::string>> claims;
    std::ostringstream out;
    out << "{\n  \"schema\": \"museum-api-claims/1\",\n  \"rooms\": [";
    bool firstRoom = true;
    for (const RoomDef& r : m.rooms) {
        if (r.kind != CellKind::Room) continue;
        out << (firstRoom ? "\n" : ",\n")
            << "    {\"key\": " << JsonString(r.key)
            << ", \"title\": " << JsonString(r.title)
            << ", \"exhibit\": " << JsonString(r.exhibit)
            << ", \"origin\": " << JsonString(OriginName(r.origin))
            << ", \"apis\": [";
        for (std::size_t i = 0; i < r.apis.size(); ++i) {
            out << (i == 0 ? "" : ", ") << JsonString(r.apis[i]);
            claims[r.apis[i]].push_back(r.key);
        }
        out << "]}";
        firstRoom = false;
    }
    out << "\n  ],\n  \"claims\": [";
    bool firstClaim = true;
    for (const auto& [api, rooms] : claims) {
        out << (firstClaim ? "\n" : ",\n")
            << "    {\"api\": " << JsonString(api) << ", \"rooms\": [";
        for (std::size_t i = 0; i < rooms.size(); ++i)
            out << (i == 0 ? "" : ", ") << JsonString(rooms[i]);
        out << "]}";
        firstClaim = false;
    }
    out << "\n  ]\n}\n";
    return out.str();
}

std::vector<std::string> FindMissingAssets(const MuseumPlan& m, const std::string& contentRoot)
{
    namespace fs = std::filesystem;
    std::vector<std::string> missing;
    std::set<std::string> checked;
    const auto texture = [&](const std::string& name) {
        if (name.empty() || name == "white" || !checked.insert("tex:" + name).second) return;
        if (!fs::exists(fs::path(contentRoot) / "Textures" / (name + ".png"))) missing.push_back("texture '" + name + "'");
    };
    for (const RoomDef& r : m.rooms) {
        texture(r.theme.floorTex);
        texture(r.theme.wallTex);
        texture(r.theme.ceilTex);
        if (!IsKnownExhibit(r.exhibit)) missing.push_back("exhibit '" + r.exhibit + "' (room " + r.key + ")");
    }
    texture("wood");
    texture("carpet");
    texture("glass");
    texture("metal");
    texture("grass");
    texture("cobble");
    texture("logo");
    return missing;
}
