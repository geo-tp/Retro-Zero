#include "retro_frontend.h"

#include "../Utils/bios_utils.h"
#include "../Utils/env_utils.h"

#include <cstdlib>
#include <iostream>

// Forwards Libretro keyboard events to the input backend when a core requests them.
void keyboard_event_bridge(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
{
    if (retro_frontend_context().keyboard_event_cb) {
        retro_frontend_context().keyboard_event_cb(down, keycode, character, key_modifiers);
    }
}

// Presents either software frames or completed hardware-rendered frames.
void video_cb(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        if (width) retro_frontend_context().hw_frame_width = width;
        if (height) retro_frontend_context().hw_frame_height = height;
        if (retro_frontend_context().hw_render_ready) {
            retro_frontend_context().egl_video.present(retro_frontend_context().hw_frame_width, retro_frontend_context().hw_frame_height);
        }
        return;
    }
    retro_frontend_context().video.render(data, width, height, pitch);
}

// Handles single-frame audio callbacks from older/simple cores.
void audio_sample_cb(int16_t left, int16_t right)
{
    retro_frontend_context().audio.write_sample(left, right);
}

// Handles batched audio callbacks from most Libretro cores.
size_t audio_batch_cb(const int16_t *data, size_t frames)
{
    return retro_frontend_context().audio.write_batch(data, frames);
}

unsigned rotated_dpad_id(unsigned id)
{
    switch (retro_frontend_context().video_rotation & 3u) {
    case 1: // 90 degrees
        switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_UP: return RETRO_DEVICE_ID_JOYPAD_RIGHT;
        case RETRO_DEVICE_ID_JOYPAD_DOWN: return RETRO_DEVICE_ID_JOYPAD_LEFT;
        case RETRO_DEVICE_ID_JOYPAD_LEFT: return RETRO_DEVICE_ID_JOYPAD_UP;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT: return RETRO_DEVICE_ID_JOYPAD_DOWN;
        default: return id;
        }
    case 2: // 180 degrees
        switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_UP: return RETRO_DEVICE_ID_JOYPAD_DOWN;
        case RETRO_DEVICE_ID_JOYPAD_DOWN: return RETRO_DEVICE_ID_JOYPAD_UP;
        case RETRO_DEVICE_ID_JOYPAD_LEFT: return RETRO_DEVICE_ID_JOYPAD_RIGHT;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT: return RETRO_DEVICE_ID_JOYPAD_LEFT;
        default: return id;
        }
    case 3: // 270 degrees
        switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_UP: return RETRO_DEVICE_ID_JOYPAD_LEFT;
        case RETRO_DEVICE_ID_JOYPAD_DOWN: return RETRO_DEVICE_ID_JOYPAD_RIGHT;
        case RETRO_DEVICE_ID_JOYPAD_LEFT: return RETRO_DEVICE_ID_JOYPAD_DOWN;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT: return RETRO_DEVICE_ID_JOYPAD_UP;
        default: return id;
        }
    default:
        return id;
    }
}


int16_t rotated_joypad_state(unsigned id)
{
    return retro_frontend_context().input.joypad_state(rotated_dpad_id(id));
}

// --- Keyboard → left analog stick, activé par défaut pour N64/DC ---
bool keyboard_to_left_analog_enabled()
{
    const char *env = std::getenv("CP0_KEYBOARD_ANALOG");
    if (env && env[0]) {
        return env_enabled_value(env);
    }

    // Compat ancien nom.
    env = std::getenv("CP0_N64_DPAD_TO_ANALOG");
    if (env && env[0]) {
        return env_enabled_value(env);
    }

    // Activé par défaut, sans build flag ni env var.
    return retro_frontend_context().active_env_core == "CP0_CORE_N64" ||
           is_flycast_env_core(retro_frontend_context().active_env_core.c_str());
}

int16_t keyboard_analog_strength()
{
    const char *env = std::getenv("CP0_ANALOG_STRENGTH");
    if (!env || !env[0]) {
        env = std::getenv("CP0_N64_ANALOG_STRENGTH");
    }

    if (env && env[0]) {
        char *end = nullptr;
        long value = std::strtol(env, &end, 10);

        if (end != env) {
            if (value < 1) {
                value = 1;
            }
            if (value > 32767) {
                value = 32767;
            }

            return static_cast<int16_t>(value);
        }
    }

    return 32767;
}

int16_t keyboard_left_analog_state(unsigned index, unsigned id)
{
    if (!keyboard_to_left_analog_enabled()) {
        return 0;
    }

    if (index != RETRO_DEVICE_INDEX_ANALOG_LEFT) {
        return 0;
    }

    const int16_t analog_max = keyboard_analog_strength();

    // Pour le moment on recycle les directions RetroPad.
    // Si tu ajoutes les binds dédiés dans EvdevInput, remplace cette partie
    // par retro_frontend_context().input.left_analog_state(index, id, analog_max).
    const bool left  = rotated_joypad_state(RETRO_DEVICE_ID_JOYPAD_LEFT)  != 0;
    const bool right = rotated_joypad_state(RETRO_DEVICE_ID_JOYPAD_RIGHT) != 0;
    const bool up    = rotated_joypad_state(RETRO_DEVICE_ID_JOYPAD_UP)    != 0;
    const bool down  = rotated_joypad_state(RETRO_DEVICE_ID_JOYPAD_DOWN)  != 0;

    switch (id) {
    case RETRO_DEVICE_ID_ANALOG_X:
        if (left && !right) {
            return static_cast<int16_t>(-analog_max);
        }
        if (right && !left) {
            return analog_max;
        }
        return 0;

    case RETRO_DEVICE_ID_ANALOG_Y:
        if (up && !down) {
            return static_cast<int16_t>(-analog_max);
        }
        if (down && !up) {
            return analog_max;
        }
        return 0;

    default:
        return 0;
    }
}

// Polls evdev input once per Libretro frame.
void input_poll_cb()
{
    retro_frontend_context().input.poll();
    if (retro_frontend_context().input.quit_requested()) {
        retro_frontend_context().quit = true;
    }
}

// Returns Libretro input state, including keyboard-to-analog helpers for N64.
int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id)
{
    const unsigned base_device = device & RETRO_DEVICE_MASK;

    if (std::getenv("CP0_INPUT_DEBUG")) {
        static bool seen[8][16][8][64] = {};
        static int seen_count = 0;

        if (port < 8 && base_device < 16 && index < 8 && id < 64) {
            if (!seen[port][base_device][index][id] && seen_count < 400) {
                seen[port][base_device][index][id] = true;
                ++seen_count;

                std::cout << "input poll seen:"
                          << " port=" << port
                          << " device=" << device
                          << " base_device=" << base_device
                          << " index=" << index
                          << " id=" << id
                          << "\n";
            }
        }
    }

    if (port != 0) {
        return 0;
    }

    if (base_device == RETRO_DEVICE_KEYBOARD) {
        return retro_frontend_context().input.keyboard_state(id);
    }

    if (base_device == RETRO_DEVICE_ANALOG) {
        const int16_t value = keyboard_left_analog_state(index, id);

        if (value != 0 && std::getenv("CP0_INPUT_DEBUG")) {
            static int analog_nonzero_log_count = 0;

            if (analog_nonzero_log_count < 200) {
                std::cout << "analog nonzero:"
                          << " core=" << retro_frontend_context().active_env_core
                          << " port=" << port
                          << " device=" << device
                          << " base_device=" << base_device
                          << " index=" << index
                          << " id=" << id
                          << " value=" << value
                          << "\n";
                ++analog_nonzero_log_count;
            }
        }

        return value;
    }

    if (base_device != RETRO_DEVICE_JOYPAD) {
        return 0;
    }

    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
        uint16_t mask = 0;

        for (unsigned b = RETRO_DEVICE_ID_JOYPAD_B;
             b <= RETRO_DEVICE_ID_JOYPAD_R3;
             ++b) {
            if (rotated_joypad_state(b)) {
                mask |= static_cast<uint16_t>(1u << b);
            }
        }

        return static_cast<int16_t>(mask);
    }

    return rotated_joypad_state(id);
}
