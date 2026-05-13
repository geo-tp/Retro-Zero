#ifndef CP0_ROM_LOADER_H
#define CP0_ROM_LOADER_H

#include "libretro.h"

#include <cstdint>
#include <vector>

// Owns ROM bytes when a core accepts in-memory loading.
struct LoadedRetroGame {
    retro_game_info info {};
    std::vector<uint8_t> rom_data;
};

// Prepares retro_game_info using either path-only or in-memory ROM loading.
bool prepare_loaded_retro_game(
    const char *rom_path,
    const char *selected_env_core,
    const retro_system_info &system_info,
    LoadedRetroGame &loaded_game
);

#endif // CP0_ROM_LOADER_H
