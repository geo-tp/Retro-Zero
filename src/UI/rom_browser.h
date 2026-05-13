#ifndef CP0_ROM_BROWSER_H
#define CP0_ROM_BROWSER_H

#include "../Core/core_config.h"

#include <string>
#include <vector>

struct FileEntry {
    std::string name;
    std::string path;
    bool dir = false;
};

// Lowercases text for extension/filter matching.
std::string to_lower_copy(std::string value);

// Resolves the ROM directory for one console using env overrides when set.
std::string find_rom_dir_for_console(const CoreConfig &console);

// Identifies special launcher entries that are not emulation cores.
bool is_rom_upload_console(const CoreConfig &console);
bool is_settings_console(const CoreConfig &console);

// Lists directories and ROM files accepted by the selected console profile.
std::vector<FileEntry> load_rom_entries(
    const std::string &rom_dir,
    const CoreConfig &console,
    const std::string &filter
);

#endif // CP0_ROM_BROWSER_H
