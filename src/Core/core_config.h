#ifndef CP0_CORE_CONFIG_H
#define CP0_CORE_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

// Describes a single emulation core / console configuration.
// This is the single source of truth for core-specific metadata.
struct CoreConfig {
    const char* id;              // "nes", "snes", "gba", "" for tools
    const char* name;            // "Nintendo", "Super Nintendo"
    const char* shortName;       // "NES", "SNES"
    const char* envCore;         // "CP0_CORE_NES", "CP0_TOOL_SETTINGS"
    const char* corePath;        // default .so path on disk
    const char* coreZipUrl;      // download URL for the core archive
    std::vector<std::string> extensions; // accepted ROM extensions, e.g. {".nes", ".zip"}
    uint32_t accent;             // UI accent colour (0xRRGGBB)
    bool needsBios;              // true if a BIOS file must be present (e.g. Neo Geo)
    const char* romDirEnv;       // env var that overrides the ROM directory
    const char* romDirDefault;   // default ROM directory path
    const char* saveSubdir;      // subdirectory under saves root, e.g. "nes"
    bool loadByPathOnly;         // pass ROM path only (no in-memory load), for disc images / arcade ZIPs
    bool twoButtonMode;          // restrict input to 2-button layout (B+A)
};

#endif // CP0_CORE_CONFIG_H
