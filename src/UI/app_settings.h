#ifndef CP0_APP_SETTINGS_H
#define CP0_APP_SETTINGS_H

#include <array>
#include <cstddef>
#include <string>

#include <linux/input.h>

// Describes one configurable frontend input binding.
struct BindAction {
    const char *label;
    const char *env_name;
    const char *cfg_key;
    int default_key;
};

constexpr std::size_t kBindActionCount = 18;
extern const std::array<BindAction, kBindActionCount> kBindActions;

// Persistent launcher settings mirrored to env vars before starting a core.
struct AppConfig {
    bool default_4x3 = false;
    int volume_percent = 30;
    int brightness_percent = 70;
    std::array<int, kBindActionCount> binds {};
};

// Restores built-in defaults for settings and key bindings.
void reset_app_config_defaults(AppConfig &config);

// Loads ~/.cp0-libretro.cfg when present.
void load_app_config_file(AppConfig &config);

// Saves the current launcher settings to ~/.cp0-libretro.cfg.
void save_app_config_file(const AppConfig &config);

// Publishes settings as CP0_* environment variables for the runtime/backend.
void apply_app_config_env(const AppConfig &config);

// Applies the saved brightness immediately while still in the launcher UI.
void apply_brightness_now_from_config(const AppConfig &config);

// Saves settings and applies both environment variables and live brightness.
void persist_and_apply_app_config(const AppConfig &config);

// Converts Linux key codes to compact labels shown in the settings UI.
std::string key_name_for_code(int code);

#endif // CP0_APP_SETTINGS_H
