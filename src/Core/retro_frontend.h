#ifndef CP0_RETRO_FRONTEND_H
#define CP0_RETRO_FRONTEND_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "libretro.h"
#include "../Audio/alsa_audio.h"
#include "../Input/evdev_input.h"
#include "../Video/eglfbdev_video.h"
#include "../Video/fbdev_video.h"

// Runtime frontend state shared by Libretro callbacks and the main emulation loop.
struct RetroFrontendContext {
    FbdevVideo video;
    EglFbdevVideo egl_video;
    EvdevInput input;
    AlsaAudio audio;

    bool quit = false;
    unsigned video_rotation = 0;

    bool hw_render_requested = false;
    bool hw_render_ready = false;
    bool hw_context_reset_done = false;
    unsigned hw_frame_width = 320;
    unsigned hw_frame_height = 240;
    retro_hw_render_callback hw_render {};

    retro_perf_callback perf_callback {};
    retro_rumble_interface rumble_interface {};
    retro_keyboard_event_t keyboard_event_cb = nullptr;

    std::string active_env_core;
    std::string system_dir;
    std::string content_dir;
    std::string save_dir;

    float target_refresh_hz = 60.0f;
};

// Returns the process-wide frontend context used by Libretro callbacks.
RetroFrontendContext &retro_frontend_context();

// Initializes static callback interfaces exposed through RETRO_ENVIRONMENT_*.
void init_frontend_interfaces();

// Clears per-content frontend state before launching a new ROM/core.
void reset_frontend_state_for_content(const std::string &active_env_core);

// Performs the deferred GL context reset requested by hardware-rendered cores.
void reset_hw_render_context_if_needed();

// Bridges Libretro keyboard callbacks to the evdev input layer.
void keyboard_event_bridge(
    bool down,
    unsigned keycode,
    uint32_t character,
    uint16_t key_modifiers
);

// Libretro environment/video/audio/input callbacks.
bool environment_cb(unsigned cmd, void *data);
void video_cb(const void *data, unsigned width, unsigned height, size_t pitch);
void audio_sample_cb(int16_t left, int16_t right);
size_t audio_batch_cb(const int16_t *data, size_t frames);
void input_poll_cb();
int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id);

#endif // CP0_RETRO_FRONTEND_H
