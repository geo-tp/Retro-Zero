#include "rom_loader.h"

#include "core_registry.h"
#include "../Utils/file_utils.h"

#include <cstring>
#include <iostream>

// Builds the retro_game_info payload using the loading mode expected by the selected core.
bool prepare_loaded_retro_game(
    const char *rom_path,
    const char *selected_env_core,
    const retro_system_info &system_info,
    LoadedRetroGame &loaded_game
)
{
    loaded_game = {};
    loaded_game.info.path = rom_path;
    loaded_game.info.meta = nullptr;

    size_t rom_file_size = 0;
    if (stat_regular_file(rom_path, rom_file_size)) {
        std::cout << "ROM path: " << rom_path << " (" << rom_file_size << " bytes)\n";
    } else {
        std::cerr << "ROM path is not a readable regular file: "
                  << (rom_path ? rom_path : "(null)") << "\n";
    }

    const bool registry_path_only =
        CoreRegistry::shouldLoadByPathOnly(rom_path ? rom_path : "", selected_env_core);
    const bool n64_memory_load =
        selected_env_core && std::strcmp(selected_env_core, "CP0_CORE_N64") == 0 &&
        !system_info.need_fullpath;
    const bool load_by_path_only =
        system_info.need_fullpath || (registry_path_only && !n64_memory_load);

    if (load_by_path_only) {
        loaded_game.info.data = nullptr;
        loaded_game.info.size = 0;
        std::cout << "ROM load mode: path-only\n";
        return true;
    }

    if (!read_file(rom_path, loaded_game.rom_data)) {
        return false;
    }

    loaded_game.info.data = loaded_game.rom_data.data();
    loaded_game.info.size = loaded_game.rom_data.size();
    std::cout << "ROM load mode: memory (" << loaded_game.info.size << " bytes)\n";
    return true;
}
