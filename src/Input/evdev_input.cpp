#include "evdev_input.h"

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "libretro.h"

namespace {
uint64_t monotonic_ms()
{
    using clock = std::chrono::steady_clock;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count());
}

int parse_env_key_code(const char *name)
{
    const char *value = std::getenv(name);
    if (!value || !value[0]) {
        return -1;
    }
    char *end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || parsed < 0 || parsed > KEY_MAX) {
        return -1;
    }
    return static_cast<int>(parsed);
}

unsigned evdev_digit_to_retro_key(int code)
{
    switch (code) {
    case KEY_0: return RETROK_0;
    case KEY_1: return RETROK_1;
    case KEY_2: return RETROK_2;
    case KEY_3: return RETROK_3;
    case KEY_4: return RETROK_4;
    case KEY_5: return RETROK_5;
    case KEY_6: return RETROK_6;
    case KEY_7: return RETROK_7;
    case KEY_8: return RETROK_8;
    case KEY_9: return RETROK_9;
    default: return RETROK_UNKNOWN;
    }
}

}

void EvdevInput::init_key_bindings()
{
    key_to_joypad_.fill(-1);

    auto bind_default = [&](int key, int joypad_id) {
        if (key >= 0 && static_cast<size_t>(key) < key_to_joypad_.size()) {
            key_to_joypad_[static_cast<size_t>(key)] = joypad_id;
        }
    };

    bind_default(KEY_Z, RETRO_DEVICE_ID_JOYPAD_LEFT);
    bind_default(KEY_C, RETRO_DEVICE_ID_JOYPAD_RIGHT);
    bind_default(KEY_F, RETRO_DEVICE_ID_JOYPAD_UP);
    bind_default(KEY_X, RETRO_DEVICE_ID_JOYPAD_DOWN);
    bind_default(KEY_J, RETRO_DEVICE_ID_JOYPAD_L);
    bind_default(KEY_K, RETRO_DEVICE_ID_JOYPAD_B);
    bind_default(KEY_L, RETRO_DEVICE_ID_JOYPAD_A);
    bind_default(KEY_I, RETRO_DEVICE_ID_JOYPAD_Y);
    bind_default(KEY_O, RETRO_DEVICE_ID_JOYPAD_X);
    bind_default(KEY_U, RETRO_DEVICE_ID_JOYPAD_R);
    bind_default(KEY_V, RETRO_DEVICE_ID_JOYPAD_START);
    bind_default(KEY_B, RETRO_DEVICE_ID_JOYPAD_SELECT);
    bind_default(KEY_N, RETRO_DEVICE_ID_JOYPAD_L2);
    bind_default(KEY_M, RETRO_DEVICE_ID_JOYPAD_R2);
    // Inversion pour arcade 6 boutons :
    // 1. J <-> U
    // 2. U <-> O, J <-> L
    int id_J = RETRO_DEVICE_ID_JOYPAD_L;
    int id_L = RETRO_DEVICE_ID_JOYPAD_A;
    int id_O = RETRO_DEVICE_ID_JOYPAD_X;
    int id_U = RETRO_DEVICE_ID_JOYPAD_R;

    // 1. J <-> U
    std::swap(id_J, id_U);
    // 2. U <-> O
    std::swap(id_U, id_O);
    // 2. J <-> L
    std::swap(id_J, id_L);

    bind_default(KEY_J, id_J);
    bind_default(KEY_L, id_L);
    bind_default(KEY_O, id_O);
    bind_default(KEY_U, id_U);

    const struct {
        const char *env;
        int id;
    } remaps[] = {
        {"CP0_BIND_UP", RETRO_DEVICE_ID_JOYPAD_UP},
        {"CP0_BIND_DOWN", RETRO_DEVICE_ID_JOYPAD_DOWN},
        {"CP0_BIND_LEFT", RETRO_DEVICE_ID_JOYPAD_LEFT},
        {"CP0_BIND_RIGHT", RETRO_DEVICE_ID_JOYPAD_RIGHT},
        {"CP0_BIND_A", RETRO_DEVICE_ID_JOYPAD_A},
        {"CP0_BIND_B", RETRO_DEVICE_ID_JOYPAD_B},
        {"CP0_BIND_X", RETRO_DEVICE_ID_JOYPAD_X},
        {"CP0_BIND_Y", RETRO_DEVICE_ID_JOYPAD_Y},
        {"CP0_BIND_L", RETRO_DEVICE_ID_JOYPAD_L},
        {"CP0_BIND_R", RETRO_DEVICE_ID_JOYPAD_R},
        {"CP0_BIND_L2", RETRO_DEVICE_ID_JOYPAD_L2},
        {"CP0_BIND_R2", RETRO_DEVICE_ID_JOYPAD_R2},
        {"CP0_BIND_START", RETRO_DEVICE_ID_JOYPAD_START},
        {"CP0_BIND_SELECT", RETRO_DEVICE_ID_JOYPAD_SELECT},
    };

    for (const auto &r : remaps) {
        int code = parse_env_key_code(r.env);
        // Interdire le rebinding de E/R (volume)
        if (code == KEY_E || code == KEY_R) {
            std::cerr << "[WARN] Rebinding E/R is not allowed (reserved for volume control). Ignored.\n";
            continue;
        }
        if (code >= 0 && static_cast<size_t>(code) < key_to_joypad_.size()) {
            key_to_joypad_[static_cast<size_t>(code)] = r.id;
        }
    }
}

// Opens Linux evdev devices used by the Cardputer Zero keyboard/buttons.
bool EvdevInput::init()
{
    init_key_bindings();

    for (int i = 0; i < 32; ++i) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            fds_.push_back(fd);
            std::cout << "input: opened " << path << "\n";
        }
    }

    if (fds_.empty()) {
        std::cerr << "input: no /dev/input/event* devices opened\n";
        return false;
    }
    return true;
}

// Closes all opened evdev descriptors.
void EvdevInput::shutdown()
{
    for (int fd : fds_) {
        close(fd);
    }
    fds_.clear();
}

void EvdevInput::handle_keyboard_event(int code, int value)
{
    if (!keyboard_enabled_) return;
    if (value != 0 && value != 1) return;

    const unsigned retro_key = evdev_digit_to_retro_key(code);
    if (retro_key == RETROK_UNKNOWN) return;

    const bool down = value == 1;
    if (retro_key < keyboard_.size()) {
        keyboard_[retro_key] = down;
    }

    if (!keyboard_event_callback_) return;
    const uint32_t character = static_cast<uint32_t>(retro_key);
    keyboard_event_callback_(down, retro_key, character, 0);
}

// Reads pending input events and updates frontend/core state.
void EvdevInput::poll()
{
    constexpr uint64_t kVolumeInitialDelayMs = 260;
    constexpr uint64_t kVolumeRepeatMs = 60;
    constexpr uint64_t kBrightnessInitialDelayMs = 260;
    constexpr uint64_t kBrightnessRepeatMs = 60;

    input_event ev {};
    for (int fd : fds_) {
        while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type != EV_KEY) {
                continue;
            }

            const uint64_t now_ms = monotonic_ms();
            handle_keyboard_event(ev.code, ev.value);

            if ((ev.code == KEY_Q || ev.code == KEY_ESC) && ev.value != 0) {
                quit_ = true;
            }

            // Cardputer FN+F/FN+X are exposed as KEY_UP/KEY_DOWN.
            // E = volume up, R = volume down (in plus of FN+arrows)
            if (ev.code == KEY_UP || ev.code == KEY_E) {
                if (ev.value == 1 || ev.value == 2) {
                    ++volume_steps_;
                    volume_up_held_ = true;
                    volume_up_next_repeat_ms_ = now_ms +
                        (ev.value == 1 ? kVolumeInitialDelayMs : kVolumeRepeatMs);
                } else if (ev.value == 0) {
                    volume_up_held_ = false;
                }
                continue;
            }
            if (ev.code == KEY_DOWN || ev.code == KEY_R) {
                if (ev.value == 1 || ev.value == 2) {
                    --volume_steps_;
                    volume_down_held_ = true;
                    volume_down_next_repeat_ms_ = now_ms +
                        (ev.value == 1 ? kVolumeInitialDelayMs : kVolumeRepeatMs);
                } else if (ev.value == 0) {
                    volume_down_held_ = false;
                }
                continue;
            }
            if (ev.code == KEY_RIGHT) {
                if (ev.value == 1 || ev.value == 2) {
                    ++brightness_steps_;
                    brightness_right_held_ = true;
                    brightness_right_next_repeat_ms_ = now_ms +
                        (ev.value == 1 ? kBrightnessInitialDelayMs : kBrightnessRepeatMs);
                } else if (ev.value == 0) {
                    brightness_right_held_ = false;
                }
                continue;
            }
            if (ev.code == KEY_LEFT) {
                if (ev.value == 1 || ev.value == 2) {
                    --brightness_steps_;
                    brightness_left_held_ = true;
                    brightness_left_next_repeat_ms_ = now_ms +
                        (ev.value == 1 ? kBrightnessInitialDelayMs : kBrightnessRepeatMs);
                } else if (ev.value == 0) {
                    brightness_left_held_ = false;
                }
                continue;
            }
            if (ev.code == KEY_TAB) {
                if (ev.value == 1) {
                    ++return_to_ui_requests_;
                }
                continue;
            }

            int id = -1;
            if (static_cast<size_t>(ev.code) < key_to_joypad_.size()) {
                id = key_to_joypad_[static_cast<size_t>(ev.code)];
            }
            if (id == RETRO_DEVICE_ID_JOYPAD_L && two_button_mode_) {
                id = RETRO_DEVICE_ID_JOYPAD_A;
            }
            if (id >= 0 && static_cast<size_t>(id) < joypad_.size()) {
                joypad_[id] = ev.value != 0;
            }
        }
    }

    const uint64_t now_ms = monotonic_ms();
    while (volume_up_held_ && now_ms >= volume_up_next_repeat_ms_) {
        ++volume_steps_;
        volume_up_next_repeat_ms_ += kVolumeRepeatMs;
    }
    while (volume_down_held_ && now_ms >= volume_down_next_repeat_ms_) {
        --volume_steps_;
        volume_down_next_repeat_ms_ += kVolumeRepeatMs;
    }
    while (brightness_right_held_ && now_ms >= brightness_right_next_repeat_ms_) {
        ++brightness_steps_;
        brightness_right_next_repeat_ms_ += kBrightnessRepeatMs;
    }
    while (brightness_left_held_ && now_ms >= brightness_left_next_repeat_ms_) {
        --brightness_steps_;
        brightness_left_next_repeat_ms_ += kBrightnessRepeatMs;
    }

}

int EvdevInput::consume_volume_steps()
{
    int steps = volume_steps_;
    volume_steps_ = 0;
    return steps;
}

int EvdevInput::consume_brightness_steps()
{
    int steps = brightness_steps_;
    brightness_steps_ = 0;
    return steps;
}

int EvdevInput::consume_return_to_ui_requests()
{
    int n = return_to_ui_requests_;
    return_to_ui_requests_ = 0;
    return n;
}

int16_t EvdevInput::joypad_state(unsigned id) const
{
    if (id >= joypad_.size()) {
        return 0;
    }
    if (trigger_alias_mode_) {
        if (id == RETRO_DEVICE_ID_JOYPAD_R2 && joypad_[RETRO_DEVICE_ID_JOYPAD_R]) {
            return 1;
        }
        if (id == RETRO_DEVICE_ID_JOYPAD_L2 && joypad_[RETRO_DEVICE_ID_JOYPAD_L]) {
            return 1;
        }
    }
    return joypad_[id] ? 1 : 0;
}

int16_t EvdevInput::keyboard_state(unsigned keycode) const
{
    if (!keyboard_enabled_ || keycode >= keyboard_.size()) {
        return 0;
    }
    return keyboard_[keycode] ? 1 : 0;
}
