#pragma once

#include <map>
#include <string>
#include <vector>

#include "World/Catalogue.hpp"

/** Counts derived from the catalogue, so documentation never carries hand-maintained statistics. */
struct MuseumInventory {
    int floors = 0;
    int wings = 0;
    int cells = 0;        // every walkable space
    int rooms = 0;        // exhibit rooms
    int exhibitKinds = 0; // distinct exhibit factories used by rooms
    int outdoorRooms = 0;
    int interactables = 0;   // filled in only when the museum has been built (see MuseumWorld); 0 from the catalogue alone
    std::map<std::string, int> apiCounts;   // API name -> number of rooms that demonstrate it
    std::map<int, int> roomsPerFloor;
    int xnaRooms = 0, extensionRooms = 0, applicationRooms = 0;
};

MuseumInventory ComputeInventory(const MuseumPlan& museum);
std::string InventoryAsText(const MuseumPlan& museum);
std::string InventoryAsJson(const MuseumPlan& museum);
/** The museum map as Markdown: every floor, wing and room with its number, origin and APIs (docs/MUSEUM_MAP.md). */
std::string MapAsMarkdown(const MuseumPlan& museum);
/** CNA API names with the rooms that demonstrate them (the generated part of docs/CNA_COVERAGE.md). */
std::string CoverageAsMarkdown(const MuseumPlan& museum);
/** Machine-readable catalogue claims. A claim is only a lead for the separate evidence audit. */
std::string CoverageAsJson(const MuseumPlan& museum);

/** Assets the catalogue refers to that do not exist under `contentRoot`; empty when all are present. */
std::vector<std::string> FindMissingAssets(const MuseumPlan& museum, const std::string& contentRoot);
