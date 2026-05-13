#ifndef CP0_APP_PLATFORM_H
#define CP0_APP_PLATFORM_H

#include "lvgl/lvgl.h"

#include <vector>

struct UiPowerStatus {
    bool available = false;
    int percentage = -1;
    bool charging = false;
};

// Creates the LVGL display backend bound to the Cardputer Zero framebuffer.
lv_display_t *init_ui_display();

// Registers Linux evdev input devices with LVGL.
void init_ui_input_devices(std::vector<int> &input_fds);

// Reads battery/charging status from sysfs when available.
bool read_ui_power_status(UiPowerStatus &status);

#endif // CP0_APP_PLATFORM_H
