#ifndef CP0_BIOS_UTILS_H
#define CP0_BIOS_UTILS_H

#include <string>
#include <vector>

struct NeoGeoBiosCheck {
    bool ok = false;
    std::string bios_dir;
    std::string neogeo_zip_path;
    std::vector<std::string> searched_dirs;
    std::string error_body;
};

// Prints the BIOS files expected by the selected core, when relevant.
void log_runtime_bios_state_for_core(const char *env_core, const char *label);

// Prepares the system directory and bundled BIOS files required by a core.
bool prepare_runtime_bios_for_core(
    const char *env_core,
    const std::string &content_dir,
    bool system_dir_overridden,
    std::string &system_dir,
    std::string &error_message
);

// Searches for neogeo.zip near the selected ROM and configured ROM directory.
NeoGeoBiosCheck check_neogeo_bios_for_rom(
    const std::string &rom_path,
    const std::string &current_rom_dir,
    const std::string &configured_rom_dir
);

#endif // CP0_BIOS_UTILS_H
