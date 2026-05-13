#include "app_platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Registers evdev descriptors as LVGL input devices.
void init_ui_input_devices(std::vector<int> &input_fds)
{
    for (int i = 0; i < 32; ++i) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            input_fds.push_back(fd);
            fprintf(stderr, "cp0-retro input: opened %s\n", path);
        }
    }
}

namespace {

std::string g_battery_supply_dir;

std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool read_first_line(const std::string &path, std::string &out)
{
    FILE *fp = fopen(path.c_str(), "r");
    if (!fp) return false;

    char line[64] {};
    const bool ok = fgets(line, sizeof(line), fp) != nullptr;
    fclose(fp);

    if (!ok) return false;

    out = line;
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
        out.pop_back();
    }

    return !out.empty();
}

bool ensure_battery_dir()
{
    if (!g_battery_supply_dir.empty()) {
        struct stat st {};
        if (stat(g_battery_supply_dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            return true;
        }
        g_battery_supply_dir.clear();
    }

    static const char *kKnown[] = {
        "/sys/class/power_supply/max170xx_battery",
        "/sys/class/power_supply/battery",
        "/sys/class/power_supply/BAT0",
        "/sys/class/power_supply/BAT1",
    };

    for (const char *dir : kKnown) {
        if (!dir || !dir[0]) continue;
        const std::string cap = std::string(dir) + "/capacity";
        struct stat st {};
        if (stat(cap.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            g_battery_supply_dir = dir;
            return true;
        }
    }

    DIR *dp = opendir("/sys/class/power_supply");
    if (!dp) return false;

    struct dirent *entry = nullptr;
    while ((entry = readdir(dp)) != nullptr) {
        const char *name = entry->d_name;
        if (!name || name[0] == '.' || name[0] == '\0') continue;

        const std::string dir = std::string("/sys/class/power_supply/") + name;
        const std::string cap = dir + "/capacity";

        struct stat st {};
        if (stat(cap.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            closedir(dp);
            g_battery_supply_dir = dir;
            return true;
        }
    }

    closedir(dp);
    return false;
}

int get_st7789v_fbdev(char *dev_path, size_t buf_size)
{
    FILE *fp = fopen("/proc/fb", "r");
    if (!fp) return -1;

    char line[256];
    int fb_num = -1;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "fb_st7789v") && sscanf(line, "%d", &fb_num) == 1) break;
    }
    fclose(fp);

    if (fb_num < 0) return -1;
    snprintf(dev_path, buf_size, "/dev/fb%d", fb_num);
    return 0;
}

} // namespace

// Creates the LVGL framebuffer display backend.
lv_display_t *init_ui_display()
{
    const char *device = std::getenv("LV_LINUX_FBDEV_DEVICE");
    char fbdev[64] {};
    if ((!device || !device[0]) && get_st7789v_fbdev(fbdev, sizeof(fbdev)) == 0) {
        device = fbdev;
    }
    if (!device || !device[0]) device = "/dev/fb0";

    lv_display_t *disp = lv_linux_fbdev_create();
    if (!disp) {
        fprintf(stderr, "cp0-retro: lv_linux_fbdev_create failed\n");
        return nullptr;
    }

    lv_linux_fbdev_set_file(disp, device);
    fprintf(stderr, "cp0-retro: framebuffer %s\n", device);
    return disp;
}


// Reads battery state from sysfs for the launcher status bar.
bool read_ui_power_status(UiPowerStatus &status)
{
    status = {};

    if (!ensure_battery_dir()) {
        return false;
    }

    std::string cap_text;
    if (read_first_line(g_battery_supply_dir + "/capacity", cap_text)) {
        status.percentage = std::atoi(cap_text.c_str());
        if (status.percentage < 0) status.percentage = 0;
        if (status.percentage > 100) status.percentage = 100;
        status.available = true;
    }

    std::string state;
    if (read_first_line(g_battery_supply_dir + "/status", state)) {
        const std::string lower = lower_copy(state);
        status.charging = lower.find("charging") != std::string::npos;
    }

    return status.available;
}
