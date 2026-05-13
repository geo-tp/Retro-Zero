#ifndef CP0_LIBRETRO_CORE_H
#define CP0_LIBRETRO_CORE_H

#include <cstddef>

#include "libretro.h"

// Prepares per-core option overrides before retro_init()/retro_load_game().
void configure_core_option_overrides(const char *env_core, const char *rom_path);

// Serves RETRO_ENVIRONMENT_GET_VARIABLE requests from the configured overrides.
bool get_core_option_override(retro_variable *var);

// Accepts Libretro option update notifications; overrides are static at runtime.
void mark_core_options_dirty();

// Dynamic Libretro core wrapper around dlopen/dlsym and the required API symbols.
struct LibretroCore {
    void *handle = nullptr;
    bool skip_dlclose = false;

    void (*retro_init)(void) = nullptr;
    void (*retro_deinit)(void) = nullptr;
    unsigned (*retro_api_version)(void) = nullptr;
    void (*retro_set_environment)(retro_environment_t) = nullptr;
    void (*retro_set_video_refresh)(retro_video_refresh_t) = nullptr;
    void (*retro_set_audio_sample)(retro_audio_sample_t) = nullptr;
    void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t) = nullptr;
    void (*retro_set_input_poll)(retro_input_poll_t) = nullptr;
    void (*retro_set_input_state)(retro_input_state_t) = nullptr;
    void (*retro_set_controller_port_device)(unsigned port, unsigned device) = nullptr;
    void (*retro_get_system_info)(retro_system_info *) = nullptr;
    void (*retro_get_system_av_info)(retro_system_av_info *) = nullptr;
    bool (*retro_load_game)(const retro_game_info *) = nullptr;
    void (*retro_unload_game)(void) = nullptr;
    void (*retro_run)(void) = nullptr;
    void *(*retro_get_memory_data)(unsigned id) = nullptr;
    size_t (*retro_get_memory_size)(unsigned id) = nullptr;

    // Loads a Libretro shared object and resolves the required entry points.
    bool load(const char *path);

    // Releases the dynamic library unless a core-specific shutdown workaround is active.
    void close();
};

#endif // CP0_LIBRETRO_CORE_H
