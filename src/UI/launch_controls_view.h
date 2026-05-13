#ifndef CP0_LAUNCH_CONTROLS_VIEW_H
#define CP0_LAUNCH_CONTROLS_VIEW_H

#include "lvgl/lvgl.h"

#include "../Core/core_config.h"
#include "app_settings.h"

#include <string>

// Draws the short control summary displayed immediately before starting a ROM.
void draw_launch_controls_screen(
    lv_obj_t *root,
    const CoreConfig &console,
    const std::string &rom,
    const AppConfig &app_config
);

#endif // CP0_LAUNCH_CONTROLS_VIEW_H
