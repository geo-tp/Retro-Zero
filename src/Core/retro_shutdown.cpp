#include "retro_shutdown.h"

#include "retro_frontend.h"
#include "../Utils/bios_utils.h"

#include <iostream>
#include <string>
#include <unistd.h>

// Shuts down the current Libretro session while preserving known core-specific workarounds.
void shutdown_retro_session(
    LibretroCore &core,
    const char *selected_env_core,
    const char *core_path
)
{
    RetroFrontendContext &frontend = retro_frontend_context();

    const bool flycast_shutdown = is_flycast_env_core(selected_env_core);
    const std::string core_path_str = core_path ? core_path : "";
    const bool is_dreamcast_old_flycast =
        flycast_shutdown && core_path_str.find("flycast") != std::string::npos;

    if (is_dreamcast_old_flycast) {
        std::cout << "[SHUTDOWN] old Flycast: skip retro_unload_game/retro_deinit/dlclose\n";
        std::cout.flush();
        std::cerr.flush();
        frontend.audio.shutdown();
        frontend.input.shutdown();
        sync();
        _Exit(0);
    }

    std::cout << "[SHUTDOWN] === BEGIN ===\n";

    std::cout << "[SHUTDOWN] retro_unload_game: begin\n";
    core.retro_unload_game();
    std::cout << "[SHUTDOWN] retro_unload_game: end\n";

    if (frontend.hw_render.context_destroy) {
        std::cout << "[SHUTDOWN] context_destroy: begin\n";
        frontend.egl_video.make_current();
        frontend.hw_render.context_destroy();
        frontend.hw_render.context_destroy = nullptr;
        std::cout << "[SHUTDOWN] context_destroy: end\n";
    }

    std::cout << "[SHUTDOWN] retro_deinit: begin\n";
    core.retro_deinit();
    std::cout << "[SHUTDOWN] retro_deinit: end\n";

    std::cout << "[SHUTDOWN] audio.shutdown: begin\n";
    frontend.audio.shutdown();
    std::cout << "[SHUTDOWN] audio.shutdown: end\n";

    std::cout << "[SHUTDOWN] input.shutdown: begin\n";
    frontend.input.shutdown();
    std::cout << "[SHUTDOWN] input.shutdown: end\n";

    std::cout << "[SHUTDOWN] egl.shutdown: begin\n";
    frontend.egl_video.shutdown();
    std::cout << "[SHUTDOWN] egl.shutdown: end\n";

    std::cout << "[SHUTDOWN] fbdev.shutdown: begin\n";
    frontend.video.shutdown();
    std::cout << "[SHUTDOWN] fbdev.shutdown: end\n";

    std::cout << "[SHUTDOWN] core.close: begin\n";
    core.close();
    std::cout << "[SHUTDOWN] core.close: end\n";

    std::cout << "[SHUTDOWN] === END ===\n";
}
