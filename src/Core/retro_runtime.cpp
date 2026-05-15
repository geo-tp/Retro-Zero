#include "retro_runtime.h"

#include "core_registry.h"
#include "libretro_core.h"
#include "frame_pacer.h"
#include "retro_frontend.h"
#include "rom_loader.h"
#include "retro_save.h"
#include "retro_shutdown.h"

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "../Audio/alsa_audio.h"
#include "../Utils/bios_utils.h"
#include "../Utils/file_utils.h"
#if CP0_WITH_LVGL
#include "../UI/app_ui.h"
#endif

// Runs the frontend: select content, configure Libretro, execute frames, and return to UI when requested.
int run_retro_runtime(int argc, char **argv)
{
    const bool ui_mode = (argc == 1);
    init_frontend_interfaces();
    apply_boot_audio_mixer();

    while (true) {
        RetroFrontendContext &frontend = retro_frontend_context();

        std::string ui_core_path;
        std::string ui_rom_path;
        const char *core_path = nullptr;
        const char *rom_path = nullptr;

        if (ui_mode) {
#if CP0_WITH_LVGL
            if (!run_app_ui(ui_core_path, ui_rom_path)) {
                std::cerr << "No ROM selected\n";
                return 1;
            }
            core_path = ui_core_path.c_str();
            rom_path = ui_rom_path.c_str();
#else
            std::cerr << "UI was not built. Rebuild with WITH_LVGL=1, or run:\n"
                      << "  " << argv[0] << " <core.so> <game.rom>\n";
            return 1;
#endif
        } else if (argc == 3) {
            core_path = argv[1];
            rom_path = argv[2];
        } else {
            std::cerr << "Usage: " << argv[0] << " [<core.so> <game.rom>]\n";
            return 1;
        }

        const char *selected_env_core = std::getenv("CP0_SELECTED_ENV_CORE");
        std::string selected_env_core_str = selected_env_core ? selected_env_core : "";
        configure_core_option_overrides(selected_env_core, rom_path);
        reset_frontend_state_for_content(selected_env_core_str);

        frontend.content_dir = parent_dir(rom_path ? rom_path : "");
        frontend.system_dir = frontend.content_dir;
        frontend.save_dir.clear();

        bool system_dir_overridden = false;
        if (const char *system_override = std::getenv("CP0_SYSTEM_DIR_OVERRIDE")) {
            if (system_override[0]) {
                frontend.system_dir = system_override;
                system_dir_overridden = true;
                std::cout << "system dir override: " << frontend.system_dir << "\n";
            }
        }

        std::string bios_error;
        if (!prepare_runtime_bios_for_core(
                selected_env_core,
                frontend.content_dir,
                system_dir_overridden,
                frontend.system_dir,
                bios_error)) {
            if (!bios_error.empty()) {
                std::cerr << bios_error << "\n";
            }
            return 1;
        }
        if (!bios_error.empty()) {
            std::cerr << bios_error << "\n";
        }

        if (const char *save_override = std::getenv("CP0_SAVE_DIR_OVERRIDE")) {
            if (save_override[0]) {
                frontend.save_dir = save_override;
            }
        }
        if (frontend.save_dir.empty()) {
            frontend.save_dir = default_save_dir_for_core(selected_env_core, rom_path);
        }
        if (!frontend.save_dir.empty() && ensure_dir_exists(frontend.save_dir)) {
            std::cout << "save dir: " << frontend.save_dir << "\n";
        } else {
            std::cerr << "save dir: fallback to content directory\n";
            frontend.save_dir = frontend.content_dir;
        }

        if (const char *refresh_env = std::getenv("CP0_TARGET_REFRESH_HZ")) {
            float parsed = std::strtof(refresh_env, nullptr);
            if (parsed > 1.0f && parsed < 1000.0f) {
                frontend.target_refresh_hz = parsed;
            }
        }

        if (!frontend.video.init("/dev/fb0")) {
            return 1;
        }
        if (std::getenv("CP0_FBDEV_CPU_MIRE")) {
            frontend.video.render_test_pattern("startup");
        }

        if (const char *default_view_mode = std::getenv("CP0_DEFAULT_VIEW_MODE")) {
            std::string mode = default_view_mode;
            for (char &c : mode) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            const bool want_4x3 =
                mode.find("4:3") != std::string::npos ||
                mode == "43" ||
                mode.find("aspect") != std::string::npos;

            if (want_4x3 != frontend.video.aspect_4x3_enabled()) {
                frontend.video.toggle_view_mode();
            }
        }

        frontend.input.init();
        const bool two_button_mode =
            CoreRegistry::isTwoButtonMode(selected_env_core, rom_path ? rom_path : "");
        frontend.input.set_two_button_mode(two_button_mode);
        frontend.input.set_trigger_alias_mode(
            is_flycast_env_core(selected_env_core));
        frontend.input.set_keyboard_enabled(
            selected_env_core && std::strcmp(selected_env_core, "CP0_CORE_MSX") == 0);
        frontend.input.set_keyboard_event_callback(keyboard_event_bridge);

        LibretroCore core;
        core.skip_dlclose = is_flycast_env_core(selected_env_core);

        if (!core.load(core_path)) {
            frontend.input.shutdown();
            frontend.video.shutdown();
            return 1;
        }

        core.retro_set_environment(environment_cb);
        core.retro_set_video_refresh(video_cb);
        core.retro_set_audio_sample(audio_sample_cb);
        core.retro_set_audio_sample_batch(audio_batch_cb);
        core.retro_set_input_poll(input_poll_cb);
        core.retro_set_input_state(input_state_cb);

        core.retro_init();

        retro_system_info system_info {};
        core.retro_get_system_info(&system_info);
        std::cout << "core: "
                  << (system_info.library_name ? system_info.library_name : "unknown")
                  << " "
                  << (system_info.library_version ? system_info.library_version : "")
                  << "\n";
        std::cout << "core need_fullpath: "
                  << (system_info.need_fullpath ? "true" : "false")
                  << " valid_extensions="
                  << (system_info.valid_extensions ? system_info.valid_extensions : "")
                  << "\n";

        LoadedRetroGame loaded_game;
        if (!prepare_loaded_retro_game(rom_path, selected_env_core, system_info, loaded_game)) {
            core.retro_deinit();
            core.close();
            frontend.input.shutdown();
            frontend.video.shutdown();
            return 1;
        }

        if (is_flycast_env_core(selected_env_core)) {
            log_runtime_bios_state_for_core(selected_env_core, "before retro_load_game");
        }

        if (!core.retro_load_game(&loaded_game.info)) {
            std::cerr << "retro_load_game failed\n";
            core.retro_deinit();
            core.close();
            frontend.input.shutdown();
            frontend.video.shutdown();
            return 1;
        }

        if (selected_env_core &&
            (is_flycast_env_core(selected_env_core) ||
             std::strcmp(selected_env_core, "CP0_CORE_N64") == 0)) {

            std::cout << "input: forcing controller ports joypad,none,none,none for "
                      << selected_env_core << "\n";

            core.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
            core.retro_set_controller_port_device(1, RETRO_DEVICE_NONE);
            core.retro_set_controller_port_device(2, RETRO_DEVICE_NONE);
            core.retro_set_controller_port_device(3, RETRO_DEVICE_NONE);
        }

        RetroSave save_sync;
        save_sync.init(&core, frontend.save_dir, rom_path ? rom_path : "game");
        save_sync.load_from_disk_if_available();

        retro_system_av_info av {};
        core.retro_get_system_av_info(&av);
        std::cout << "game geometry: " << av.geometry.base_width << "x"
                  << av.geometry.base_height << " max " << av.geometry.max_width
                  << "x" << av.geometry.max_height << "\n";
        std::cout << "timing: fps=" << av.timing.fps
                  << " sample_rate=" << av.timing.sample_rate << "\n";
        reset_hw_render_context_if_needed();

        const bool is_neogeo_core = CoreRegistry::isNeoGeoCore(selected_env_core);

        FramePacer frame_pacer;
        frame_pacer.configure(
            av.timing.fps,
            !is_neogeo_core,
            selected_env_core,
            frontend.target_refresh_hz
        );

        const bool use_ring_buffer = !is_neogeo_core;

        if (!frontend.audio.init(av.timing.sample_rate, use_ring_buffer)) {
            std::cerr << "audio: disabled; video/input will continue\n";
        } else if (std::getenv("CP0_AUDIO_TEST")) {
            frontend.audio.play_test_tone();
        }

        auto profile_last = std::chrono::steady_clock::now();
        uint64_t profile_frames = 0;
        bool return_to_file_picker = false;

        frontend.quit = false;

        while (!frontend.quit) {
            if (frontend.hw_render_ready && !std::getenv("CP0_HW_CONTEXT_CORE_THREAD")) {
                frontend.egl_video.make_current();
            }

            core.retro_run();
            ++profile_frames;

            frame_pacer.wait_for_next_frame();

            auto profile_now = std::chrono::steady_clock::now();
            std::chrono::duration<double> profile_elapsed = profile_now - profile_last;
            if (profile_elapsed.count() >= 1.0) {
                std::cout << "profile: fps="
                          << (static_cast<double>(profile_frames) / profile_elapsed.count())
                          << " hw=" << frontend.hw_frame_width << "x" << frontend.hw_frame_height
                          << " audio=" << (frontend.audio.enabled() ? "on" : "off")
                          << "\n";
                profile_last = profile_now;
                profile_frames = 0;
            }

            int volume_steps = frontend.input.consume_volume_steps();
            if (volume_steps != 0 && frontend.audio.enabled()) {
                frontend.audio.adjust_gain(0.03 * static_cast<double>(volume_steps));
            }

            int brightness_steps = frontend.input.consume_brightness_steps();
            if (brightness_steps != 0) {
                frontend.video.adjust_brightness_steps(brightness_steps);
            }

            int return_requests = frontend.input.consume_return_to_ui_requests();
            if (return_requests > 0) {
                return_to_file_picker = true;
                frontend.quit = true;
            }

            save_sync.tick();
        }

        save_sync.flush_if_dirty();

        shutdown_retro_session(core, selected_env_core, core_path);

        if (ui_mode && return_to_file_picker) {
            if (!selected_env_core_str.empty()) {
                setenv("CP0_UI_START_ENV_CORE", selected_env_core_str.c_str(), 1);
            }
            continue;
        }

        return 0;
    }
}
