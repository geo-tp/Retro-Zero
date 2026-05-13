#ifndef CP0_CONSOLE_ART_RENDERER_H
#define CP0_CONSOLE_ART_RENDERER_H

#include "lvgl/lvgl.h"
#include "../Core/core_config.h"

namespace ConsoleArtRenderer {

// Rebuilds the decorative console artwork shown on one carousel card.
bool rebuild(lv_obj_t *art, lv_obj_t *logo_badge, lv_obj_t *name_lbl, const CoreConfig &console, int slot);

} // namespace ConsoleArtRenderer

#endif // CP0_CONSOLE_ART_RENDERER_H
