#ifndef CP0_CORE_REGISTRY_H
#define CP0_CORE_REGISTRY_H

#include "core_config.h"

#include <string>
#include <vector>

// Static registry of all known emulation cores.
// Adding a new console/core requires only a new entry in CoreRegistry::all().
class CoreRegistry {
public:
    // Returns the full list of configured cores (including UI tools).
    static const std::vector<CoreConfig>& all();

    // Find a core by its id field (e.g. "nes", "snes").
    // Returns nullptr if not found.
    static const CoreConfig* findById(const std::string& id);

    // Find a core by its envCore field (e.g. "CP0_CORE_NES").
    // Returns nullptr if not found.
    static const CoreConfig* findByEnvCore(const std::string& envCore);

    // Find the best-matching core for a given ROM file path (by extension).
    // Returns nullptr if no core lists that extension.
    static const CoreConfig* findByRomPath(const std::string& romPath);

    // Returns true when the selected core should receive only the ROM path.
    static bool shouldLoadByPathOnly(const std::string& romPath, const char* envCore);

    // Returns true when the selected core should use the reduced two-button layout.
    static bool isTwoButtonMode(const char* envCore, const std::string& romPath);

    // Returns true when the selected core is the dedicated Neo Geo profile.
    static bool isNeoGeoCore(const char* envCore);

    // Returns true when the selected core is the PPSSPP / PSP profile.
    static bool isPspCore(const char* envCore);

    // Returns true when the selected core is the Nintendo 64 profile.
    static bool isN64Core(const char* envCore);
};

#endif // CP0_CORE_REGISTRY_H
