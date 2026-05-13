#include "app_settings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <string>
#include <sys/stat.h>

const std::array<BindAction, kBindActionCount> kBindActions = {{
    {"Up", "CP0_BIND_UP", "bind_up", KEY_F},
    {"Down", "CP0_BIND_DOWN", "bind_down", KEY_X},
    {"Left", "CP0_BIND_LEFT", "bind_left", KEY_Z},
    {"Right", "CP0_BIND_RIGHT", "bind_right", KEY_C},
    {"A", "CP0_BIND_A", "bind_a", KEY_L},
    {"B", "CP0_BIND_B", "bind_b", KEY_K},
    {"X", "CP0_BIND_X", "bind_x", KEY_O},
    {"Y", "CP0_BIND_Y", "bind_y", KEY_I},
    {"L", "CP0_BIND_L", "bind_l", KEY_J},
    {"R", "CP0_BIND_R", "bind_r", KEY_U},
    {"Start", "CP0_BIND_START", "bind_start", KEY_V},
    {"Select", "CP0_BIND_SELECT", "bind_select", KEY_B},
    {"L2", "CP0_BIND_L2", "bind_l2", KEY_N},
    {"R2", "CP0_BIND_R2", "bind_r2", KEY_M},
}};

namespace {

const char *kAppConfigPath = "/home/pi/.cp0-libretro.cfg";

bool read_int_file(const std::string &path, int &out)
{
    FILE *fp = fopen(path.c_str(), "r");
    if (!fp) return false;
    int value = 0;
    const bool ok = fscanf(fp, "%d", &value) == 1;
    fclose(fp);
    if (!ok) return false;
    out = value;
    return true;
}

bool write_int_file(const std::string &path, int value)
{
    FILE *fp = fopen(path.c_str(), "w");
    if (!fp) return false;
    const bool ok = fprintf(fp, "%d\n", value) > 0;
    fclose(fp);
    return ok;
}

} // namespace

// Fills the app config with built-in defaults.
void reset_app_config_defaults(AppConfig &config)
{
    config.default_4x3 = false;
    config.volume_percent = 30;
    config.brightness_percent = 70;
    for (size_t i = 0; i < config.binds.size(); ++i) {
        config.binds[i] = kBindActions[i].default_key;
    }
}

std::string key_name_for_code(int code)
{
    switch (code) {
    case KEY_A: return "A";
    case KEY_B: return "B";
    case KEY_C: return "C";
    case KEY_D: return "D";
    case KEY_E: return "E";
    case KEY_F: return "F";
    case KEY_G: return "G";
    case KEY_H: return "H";
    case KEY_I: return "I";
    case KEY_J: return "J";
    case KEY_K: return "K";
    case KEY_L: return "L";
    case KEY_M: return "M";
    case KEY_N: return "N";
    case KEY_O: return "O";
    case KEY_P: return "P";
    case KEY_Q: return "Q";
    case KEY_R: return "R";
    case KEY_S: return "S";
    case KEY_T: return "T";
    case KEY_U: return "U";
    case KEY_V: return "V";
    case KEY_W: return "W";
    case KEY_X: return "X";
    case KEY_Y: return "Y";
    case KEY_Z: return "Z";
    case KEY_0: return "0";
    case KEY_1: return "1";
    case KEY_2: return "2";
    case KEY_3: return "3";
    case KEY_4: return "4";
    case KEY_5: return "5";
    case KEY_6: return "6";
    case KEY_7: return "7";
    case KEY_8: return "8";
    case KEY_9: return "9";
    case KEY_SPACE: return "SPACE";
    case KEY_ENTER: return "ENTER";
    case KEY_BACKSPACE: return "BACK";
    case KEY_UP: return "UP";
    case KEY_DOWN: return "DOWN";
    case KEY_LEFT: return "LEFT";
    case KEY_RIGHT: return "RIGHT";
    case KEY_TAB: return "TAB";
    default: return "#" + std::to_string(code);
    }
}

// Exports launcher settings to CP0_* variables consumed by the runtime.
void apply_app_config_env(const AppConfig &config)
{
    setenv("CP0_DEFAULT_VIEW_MODE", config.default_4x3 ? "4:3" : "fullscreen", 1);

    double gain = static_cast<double>(config.volume_percent) / 100.0;
    if (gain < 0.0) gain = 0.0;
    if (gain > 1.0) gain = 1.0;
    char gain_buf[32];
    snprintf(gain_buf, sizeof(gain_buf), "%.3f", gain);
    setenv("CP0_AUDIO_GAIN", gain_buf, 1);

    char brightness_buf[16];
    snprintf(brightness_buf, sizeof(brightness_buf), "%d",
             std::max(0, std::min(100, config.brightness_percent)));
    setenv("CP0_DEFAULT_BRIGHTNESS_PERCENT", brightness_buf, 1);

    for (size_t i = 0; i < config.binds.size(); ++i) {
        char key_buf[16];
        snprintf(key_buf, sizeof(key_buf), "%d", config.binds[i]);
        setenv(kBindActions[i].env_name, key_buf, 1);
    }
}

void apply_brightness_now_from_config(const AppConfig &config)
{
    const int brightness_pct = std::max(0, std::min(100, config.brightness_percent));

    std::string brightness_path;
    int max_brightness = 0;

    const char *forced = std::getenv("CP0_BACKLIGHT_BRIGHTNESS_PATH");
    if (forced && forced[0]) {
        const std::string forced_path = forced;
        const size_t slash = forced_path.find_last_of('/');
        if (slash != std::string::npos) {
            const std::string dir = forced_path.substr(0, slash);
            int cur = 0;
            if (read_int_file(dir + "/max_brightness", max_brightness) &&
                read_int_file(forced_path, cur)) {
                brightness_path = forced_path;
            }
        }
    }

    if (brightness_path.empty()) {
        DIR *dp = opendir("/sys/class/backlight");
        if (!dp) return;
        struct dirent *entry = nullptr;
        while ((entry = readdir(dp)) != nullptr) {
            const char *name = entry->d_name;
            if (!name || name[0] == '.') continue;
            const std::string dir = std::string("/sys/class/backlight/") + name;
            const std::string candidate = dir + "/brightness";
            int cur = 0;
            if (read_int_file(dir + "/max_brightness", max_brightness) &&
                read_int_file(candidate, cur)) {
                brightness_path = candidate;
                break;
            }
        }
        closedir(dp);
    }

    if (brightness_path.empty() || max_brightness <= 0) return;

    const int min_brightness = 1;
    const int range = std::max(0, max_brightness - min_brightness);
    const int target = min_brightness + (range * brightness_pct) / 100;

    int current = 0;
    if (!read_int_file(brightness_path, current)) return;
    if (current == target) return;
    write_int_file(brightness_path, target);
}

// Persists launcher settings to the user config file.
void save_app_config_file(const AppConfig &config)
{
    std::ofstream out(kAppConfigPath, std::ios::trunc);
    if (!out.is_open()) return;
    out << "default_view_mode=" << (config.default_4x3 ? "4:3" : "fullscreen") << "\n";
    out << "audio_volume=" << config.volume_percent << "\n";
    out << "display_brightness=" << config.brightness_percent << "\n";
    for (size_t i = 0; i < config.binds.size(); ++i) {
        out << kBindActions[i].cfg_key << "=" << config.binds[i] << "\n";
    }
}

// Loads launcher settings from the user config file when present.
void load_app_config_file(AppConfig &config)
{
    reset_app_config_defaults(config);
    std::ifstream in(kAppConfigPath);
    if (!in.is_open()) return;

    bool loaded_audio_volume = false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (key == "default_view_mode") {
            config.default_4x3 = (value == "4:3");
            continue;
        }
        if (key == "audio_volume") {
            int v = std::atoi(value.c_str());
            config.volume_percent = std::max(0, std::min(100, v));
            loaded_audio_volume = true;
            continue;
        }
        if (key == "display_brightness") {
            int v = std::atoi(value.c_str());
            config.brightness_percent = std::max(0, std::min(100, v));
            continue;
        }
        for (size_t i = 0; i < config.binds.size(); ++i) {
            if (key == kBindActions[i].cfg_key) {
                int code = std::atoi(value.c_str());
                if (code >= 0 && code <= KEY_MAX) config.binds[i] = code;
                break;
            }
        }
    }

    if (loaded_audio_volume && config.volume_percent == 1) {
        config.volume_percent = 30;
    }
}

void persist_and_apply_app_config(const AppConfig &config)
{
    save_app_config_file(config);
    apply_app_config_env(config);
    apply_brightness_now_from_config(config);
}
