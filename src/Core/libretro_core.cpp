#include "libretro_core.h"
#include "../Utils/bios_utils.h"
#include "../Utils/file_utils.h"

#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<std::pair<std::string, std::string>> g_core_option_overrides;

template <typename T>
bool load_symbol(void *handle, const char *name, T &out)
{
    dlerror();
    out = reinterpret_cast<T>(dlsym(handle, name));

    const char *err = dlerror();
    if (err || !out) {
        std::cerr << "Missing libretro symbol " << name << ": "
                  << (err ? err : "null") << "\n";
        return false;
    }

    return true;
}

template <typename T>
void load_symbol_optional(void *handle, const char *name, T &out)
{
    dlerror();
    out = reinterpret_cast<T>(dlsym(handle, name));
    dlerror();
}

void set_core_option_override(const char *key, const char *value)
{
    if (!key || !key[0] || !value) {
        return;
    }

    for (auto &entry : g_core_option_overrides) {
        if (entry.first == key) {
            entry.second = value;
            return;
        }
    }

    g_core_option_overrides.emplace_back(key, value);
}

} // namespace

void mark_core_options_dirty()
{
    // Core option update notifications are accepted, but this frontend
    // currently serves static overrides and reports no runtime changes.
}

// Populates static Libretro core option overrides before the core queries them.
void configure_core_option_overrides(const char *env_core, const char *rom_path)
{
    g_core_option_overrides.clear();

    // Gearboy's "Auto" model can enable Super Game Boy presentation for
    // SGB-aware GB games. The GB launcher always wants plain DMG mode.
    if (env_core && std::strcmp(env_core, "CP0_CORE_GB") == 0) {
        set_core_option_override("gearboy_model", "Game Boy DMG");
        set_core_option_override("gearboy_palette", "B/W");
    }

    if (env_core && std::strcmp(env_core, "CP0_CORE_MSX") == 0) {
        const std::string ext = lower_ext(rom_path ? rom_path : "");

        set_core_option_override(
            "bluemsx_msxtype",
            ext == ".mx1" ? "MSX" : "MSX2"
        );

        // blueMSX en "Auto" semble choisir 50Hz sur notre setup.
        // On force 60Hz par défaut pour éviter les jeux trop lents / audio ralenti.
        // Override possible :
        //   CP0_MSX_VDP_SYNC=Auto
        //   CP0_MSX_VDP_SYNC=50Hz
        //   CP0_MSX_VDP_SYNC=60Hz
        const char *msx_vdp_sync = std::getenv("CP0_MSX_VDP_SYNC");
        set_core_option_override(
            "bluemsx_vdp_synctype",
            (msx_vdp_sync && msx_vdp_sync[0]) ? msx_vdp_sync : "60Hz"
        );

        set_core_option_override("bluemsx_ym2413_enable", "enabled");

        std::cout << "msx: type="
                  << (ext == ".mx1" ? "MSX" : "MSX2")
                  << " vdp_synctype="
                  << ((msx_vdp_sync && msx_vdp_sync[0]) ? msx_vdp_sync : "60Hz")
                  << " ym2413=enabled\n";
    }

    if (env_core && std::strcmp(env_core, "CP0_CORE_N64") == 0) {
        const char *cpu_core = std::getenv("CP0_N64_CPU_CORE");
        set_core_option_override(
            "mupen64plus-cpucore",
            (cpu_core && cpu_core[0]) ? cpu_core : "dynamic_recompiler"
        );
        set_core_option_override("mupen64plus-rspmode", "HLE");
        set_core_option_override("mupen64plus-43screensize", "320x240");
        set_core_option_override("mupen64plus-aspect", "4:3");
        set_core_option_override("mupen64plus-EnableFBEmulation", "False");
        set_core_option_override("mupen64plus-MultiSampling", "0");
        set_core_option_override("mupen64plus-BilinearMode", "standard");
        set_core_option_override("mupen64plus-astick-deadzone", "0");
        set_core_option_override("mupen64plus-astick-sensitivity", "100");
    }

    if (is_flycast_env_core(env_core)) {
        auto set_dc_option = [](const char *suffix, const char *value) {
            const std::string reicast_key = std::string("reicast_") + suffix;
            const std::string flycast_key = std::string("flycast_") + suffix;
            set_core_option_override(reicast_key.c_str(), value);
            set_core_option_override(flycast_key.c_str(), value);
        };

        set_dc_option("renderer", "OpenGL");
        set_dc_option("gles2", "enabled");
        set_dc_option("alpha_sorting", "per-strip");
        set_dc_option("cable_type", "VGA (RGB)");
        set_dc_option("cable", "VGA (RGB)");
        set_dc_option("hle_bios", "disabled");
        set_dc_option("boot_to_bios", "disabled");
        set_dc_option("enable_rttb", "disabled");
        set_dc_option("rttb", "disabled");
        set_dc_option("emulate_framebuffer", "disabled");
        set_dc_option("fog", "disabled");
        set_dc_option("pvr2_filtering", "disabled");
        set_dc_option("mipmapping", "disabled");
        set_dc_option("threaded_rendering", "enabled");
        set_dc_option("frame_skipping", "disabled");
        set_dc_option("frameskip", "disabled");
        set_dc_option("auto_skip_frame", "disabled");
        set_dc_option("framerate", "normal");
        set_dc_option("delay_frame_swapping", "disabled");
        set_dc_option("vmu_display", "disabled");
        set_dc_option("region", "Default");
        set_dc_option("broadcast", "NTSC");
        set_dc_option("language", "Default");
        set_dc_option("dsp", "disabled");
        set_dc_option("DSP", "disabled");
        set_dc_option("texupscale", "1");
        set_dc_option("anisotropic_filtering", "1");
        set_dc_option("render_to_texture_upscaling", "1x");
        set_dc_option("internal_resolution", "320x240");


        std::cout << "flycast diag: ultra-safe GLES2 profile"
                  << " alpha_sorting=per-strip"
                  << " cable_type=VGA(RGB)"
                  << " hle_bios=disabled"
                  << " boot_to_bios=disabled"
                  << " threaded_rendering=enabled"
                  << " internal_resolution=320x240\n";
    }
}

// Handles RETRO_ENVIRONMENT_GET_VARIABLE by returning configured option values.
bool get_core_option_override(retro_variable *var)
{
    if (!var || !var->key) {
        std::cout << "core option GET_VARIABLE <null>\n";
        return false;
    }

    for (const auto &entry : g_core_option_overrides) {
        if (entry.first == var->key) {
            var->value = entry.second.c_str();
            std::cout << "core option GET_VARIABLE " << var->key
                      << " -> " << var->value << " (override)\n";
            return true;
        }
    }

    var->value = nullptr;
    std::cout << "core option GET_VARIABLE " << var->key << " -> <null>\n";
    return false;
}

// Opens a Libretro shared object and resolves the symbols required by the frontend.
bool LibretroCore::load(const char *path)
{
    handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "dlopen failed: " << dlerror() << "\n";
        return false;
    }

    const bool ok =
        load_symbol(handle, "retro_init", retro_init) &&
        load_symbol(handle, "retro_deinit", retro_deinit) &&
        load_symbol(handle, "retro_api_version", retro_api_version) &&
        load_symbol(handle, "retro_set_environment", retro_set_environment) &&
        load_symbol(handle, "retro_set_video_refresh", retro_set_video_refresh) &&
        load_symbol(handle, "retro_set_audio_sample", retro_set_audio_sample) &&
        load_symbol(handle, "retro_set_audio_sample_batch", retro_set_audio_sample_batch) &&
        load_symbol(handle, "retro_set_input_poll", retro_set_input_poll) &&
        load_symbol(handle, "retro_set_input_state", retro_set_input_state) &&
        load_symbol(handle, "retro_set_controller_port_device", retro_set_controller_port_device) &&
        load_symbol(handle, "retro_get_system_info", retro_get_system_info) &&
        load_symbol(handle, "retro_get_system_av_info", retro_get_system_av_info) &&
        load_symbol(handle, "retro_load_game", retro_load_game) &&
        load_symbol(handle, "retro_unload_game", retro_unload_game) &&
        load_symbol(handle, "retro_run", retro_run);

    load_symbol_optional(handle, "retro_get_memory_data", retro_get_memory_data);
    load_symbol_optional(handle, "retro_get_memory_size", retro_get_memory_size);

    if (!ok) {
        close();
        return false;
    }

    if (retro_api_version() != RETRO_API_VERSION) {
        std::cerr << "Unsupported libretro API version " << retro_api_version()
                  << ", expected " << RETRO_API_VERSION << "\n";
        close();
        return false;
    }

    return true;
}

// Closes the Libretro shared object unless a core-specific workaround disables dlclose.
void LibretroCore::close()
{
    if (!handle) {
        return;
    }

    if (!skip_dlclose) {
        dlclose(handle);
    } else {
        std::cerr << "[SHUTDOWN] dlclose skipped for Dreamcast core (old workaround)\n";
    }

    handle = nullptr;
}
