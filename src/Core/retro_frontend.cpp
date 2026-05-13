#include "retro_frontend.h"

namespace {
RetroFrontendContext g_frontend;
}

// Provides the singleton state shared between the runtime and Libretro callbacks.
RetroFrontendContext &retro_frontend_context()
{
    return g_frontend;
}

// Clears per-content flags while preserving opened frontend backends.
void reset_frontend_state_for_content(const std::string &active_env_core)
{
    RetroFrontendContext &ctx = retro_frontend_context();

    ctx.active_env_core = active_env_core;
    ctx.video_rotation = 0;
    ctx.hw_render_requested = false;
    ctx.hw_render_ready = false;
    ctx.hw_context_reset_done = false;
    ctx.hw_render = {};
    ctx.hw_frame_width = 320;
    ctx.hw_frame_height = 240;
    ctx.quit = false;
}
