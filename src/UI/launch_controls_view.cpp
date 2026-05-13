#include "launch_controls_view.h"

#include "../Utils/file_utils.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

static lv_obj_t *make_rect(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t bg_hex, lv_opa_t opa, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(o, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg_hex), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(o, opa, LV_PART_MAIN | LV_STATE_DEFAULT);
    return o;
}


struct LaunchPadHints {
    const char *title;
    const char *j_label;
    const char *k_label;
    const char *u_label;
    const char *i_label;
    const char *a_label;
    const char *s_label;
    bool show_u;
    bool show_i;
    bool show_shoulders;
};

LaunchPadHints launch_hints_for_console(const CoreConfig &console)
{
    if (std::strcmp(console.envCore, "CP0_CORE_NES") == 0 ||
        std::strcmp(console.envCore, "CP0_CORE_SMS") == 0 ||
        std::strcmp(console.envCore, "CP0_CORE_GG") == 0 ||
        std::strcmp(console.envCore, "CP0_CORE_A2600") == 0 ||
        std::strcmp(console.envCore, "CP0_CORE_A7800") == 0 ||
        std::strcmp(console.envCore, "CP0_CORE_PCE") == 0) {
        return {"2-button pad", "B", "A", "", "", "", "", false, false, false};
    }
    if (std::strcmp(console.envCore, "CP0_CORE_GB") == 0 ||
        std::strcmp(console.envCore, "CP0_CORE_GBC") == 0) {
        return {"Game Boy", "B", "A", "", "", "", "", false, false, false};
    }
    if (std::strcmp(console.envCore, "CP0_CORE_GBA") == 0) {
        return {"Game Boy Adv", "B", "A", "", "", "L", "R", false, false, true};
    }
    if (std::strcmp(console.envCore, "CP0_CORE_MD") == 0) {
        return {"Mega Drive", "B", "C", "A", "", "", "", true, false, false};
    }
    if (std::strcmp(console.envCore, "CP0_CORE_SNES") == 0 ||
        std::strcmp(console.envCore, "CP0_CORE_WS") == 0) {
        return {"4-button pad", "Y", "B", "X", "A", "L", "R", true, true, true};
    }
    if (std::strcmp(console.envCore, "CP0_CORE_PS1") == 0) {
        return {"PlayStation", "Square", "Cross", "Triangle", "Circle", "L1", "R1", true, true, true};
    }
    if (std::strcmp(console.envCore, "CP0_CORE_NEOGEO") == 0 ||
        std::strcmp(console.envCore, "CP0_CORE_FBN") == 0 || 
        std::strcmp(console.envCore, "CP0_CORE_MAME") == 0) {
        return {"Arcade", "B", "A", "D", "C", "", "", true, true, false};
    }
    if (std::strcmp(console.envCore, "CP0_CORE_NGP") == 0) {
        return {"Neo Geo Pocket", "B", "A", "", "", "", "", false, false, false};
    }
    if (std::strcmp(console.envCore, "CP0_CORE_LYNX") == 0) {
        return {"Atari Lynx", "B", "A", "Opt1", "Opt2", "", "", true, true, false};
    }
    return {console.shortName, "B", "A", "Y", "X", "L", "R", true, true, true};
}

lv_obj_t *make_launch_chip(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t bg, uint32_t border, uint32_t text, const char *label)
{
    lv_obj_t *chip = lv_obj_create(parent);
    lv_obj_set_pos(chip, x, y);
    lv_obj_set_size(chip, w, h);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(chip, h / 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(chip, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(chip, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(chip, lv_color_hex(border), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(chip, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(chip, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *txt = lv_label_create(chip);
    lv_label_set_text(txt, label);
    lv_obj_center(txt);
    lv_obj_set_style_text_color(txt, lv_color_hex(text), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    return chip;
}


} // namespace

// Draws the last confirmation/help screen before the selected ROM starts.
void draw_launch_controls_screen(
    lv_obj_t *root,
    const CoreConfig &console,
    const std::string &rom,
    const AppConfig &app_config
)
{
    int root_h = lv_disp_get_ver_res(lv_disp_get_default());
    if (root_h <= 0) root_h = lv_obj_get_height(root);
    if (root_h <= 0) root_h = 170;
    const bool compact = root_h < 200;
    const int panel_y = compact ? 6 : 28;
    const int panel_h = compact ? std::max(140, root_h - 12) : 184;
    const int pad_y = compact ? 46 : 52;
    const int pad_h = compact ? 82 : 104;
    const int by = compact ? 14 : 20;
    const int mid_btn_y = compact ? 53 : 66;
    const int dpad_hint_y = compact ? 65 : 75;
    const int selsta_y = compact ? 66 : 79;
    const int pad_dx = 4;

    lv_obj_t *panel = lv_obj_create(root);
    lv_obj_set_size(panel, 308, panel_h);
    lv_obj_set_pos(panel, 6, panel_y);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0A0A0C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(panel, lv_color_hex(console.accent), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    LaunchPadHints hints = launch_hints_for_console(console);

    auto bind_label = [&app_config](size_t idx) -> std::string {
        if (idx >= app_config.binds.size()) return "?";
        return key_name_for_code(app_config.binds[idx]);
    };

    const std::string up_key = bind_label(0);
    const std::string down_key = bind_label(1);
    const std::string left_key = bind_label(2);
    const std::string right_key = bind_label(3);
    const std::string a_key = bind_label(4);
    const std::string b_key = bind_label(5);
    const std::string x_key = bind_label(6);
    const std::string y_key = bind_label(7);
    const std::string l_key = bind_label(8);
    const std::string r_key = bind_label(9);
    const std::string start_key = bind_label(10);
    const std::string select_key = bind_label(11);

    lv_obj_t *title = lv_label_create(panel);
    std::string heading = std::string("Launching ") + console.shortName;
    lv_label_set_text(title, heading.c_str());
    lv_obj_set_pos(title, 12, 10);
    lv_obj_set_style_text_color(title, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *sub = lv_label_create(panel);
    lv_label_set_text(sub, hints.title);
    lv_obj_set_pos(sub, 12, 28);
    lv_obj_set_style_text_color(sub, lv_color_hex(console.accent), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *rom_lbl = lv_label_create(panel);
    lv_label_set_text(rom_lbl, file_name_from_path(rom).c_str());
    lv_label_set_long_mode(rom_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(rom_lbl, 180);
    lv_obj_set_pos(rom_lbl, 116, 12);
    lv_obj_set_style_text_align(rom_lbl, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(rom_lbl, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(rom_lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *fn_hint = lv_label_create(panel);
    lv_label_set_text(fn_hint, "FN+Arrows: Vol/Bright");
    lv_obj_set_pos(fn_hint, 10, compact ? 138 : 158);
    lv_obj_set_style_text_color(fn_hint, lv_color_hex(0x696974), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(fn_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *pad = lv_obj_create(panel);
    lv_obj_set_size(pad, 264, pad_h);
    lv_obj_set_pos(pad, 22, pad_y);
    lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(pad, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(pad, lv_color_hex(0x18181D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(pad, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(pad, lv_color_hex(0x32323A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(pad, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(pad, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Simple d-pad block on the left
    make_rect(pad, 22 + pad_dx, 29, 34, 8, 0x2A2A33, 255, 3);
    make_rect(pad, 35 + pad_dx, 16, 8, 34, 0x2A2A33, 255, 3);
    lv_obj_t *up = lv_label_create(pad);
    lv_label_set_text(up, up_key.c_str());
    lv_obj_set_pos(up, 37 + pad_dx, 5);
    lv_obj_set_style_text_color(up, lv_color_hex(0xD7D7E0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(up, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *left = lv_label_create(pad);
    lv_label_set_text(left, left_key.c_str());
    lv_obj_set_pos(left, 12 + pad_dx, 30);
    lv_obj_set_style_text_color(left, lv_color_hex(0xD7D7E0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(left, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *down = lv_label_create(pad);
    lv_label_set_text(down, down_key.c_str());
    lv_obj_set_pos(down, 37 + pad_dx, 51);
    lv_obj_set_style_text_color(down, lv_color_hex(0xD7D7E0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(down, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *right = lv_label_create(pad);
    lv_label_set_text(right, right_key.c_str());
    lv_obj_set_pos(right, 62 + pad_dx, 30);
    lv_obj_set_style_text_color(right, lv_color_hex(0xD7D7E0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(right, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *dpad_hint = lv_label_create(pad);
    lv_label_set_text(dpad_hint, "D-Pad");
    lv_obj_set_pos(dpad_hint, 24 + pad_dx, dpad_hint_y);
    lv_obj_set_style_text_color(dpad_hint, lv_color_hex(0x8B8B97), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(dpad_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Action buttons grid follows the physical Cardputer rows:
    // - default: J / K / L
    // - extended (6): U / I / O over J / K / L
    const bool six_button_layout = hints.show_u || hints.show_i || hints.show_shoulders;
    std::vector<std::string> action_keys = six_button_layout
        ? std::vector<std::string> {r_key, y_key, x_key, l_key, b_key, a_key}
        : std::vector<std::string> {l_key, b_key, a_key};

    const int chip_w = 28;
    const int chip_h = 12;
    const int chip_gap_x = 6;
    const int chip_gap_y = 4;
    const int cols = std::min(3, static_cast<int>(action_keys.size()));
    const int rows = (static_cast<int>(action_keys.size()) + 2) / 3;
    const int grid_w = cols * chip_w + (cols - 1) * chip_gap_x;
    const int grid_x = std::max(98, 264 - grid_w - 32) + pad_dx;
    const int grid_y = rows == 1 ? (by + (compact ? 8 : 10)) : by;

    for (size_t i = 0; i < action_keys.size(); ++i) {
        const int row = static_cast<int>(i) / 3;
        const int col = static_cast<int>(i) % 3;
        const int x = grid_x + col * (chip_w + chip_gap_x);
        const int y = grid_y + row * (chip_h + chip_gap_y);
        make_launch_chip(pad, x, y, chip_w, chip_h,
                         0x20191C,
                         console.accent,
                         0xF6EDEF,
                         action_keys[i].c_str());
    }

    make_launch_chip(pad, 99 + pad_dx, mid_btn_y, 24, 10, 0x23232B, 0x4A4A55, 0xE9E9EF, start_key.c_str());
    make_launch_chip(pad, 129 + pad_dx, mid_btn_y, 24, 10, 0x23232B, 0x4A4A55, 0xE9E9EF, select_key.c_str());
    lv_obj_t *selsta = lv_label_create(pad);
    std::string selsta_text = start_key + "=Start  " + select_key + "=Select";
    lv_label_set_text(selsta, selsta_text.c_str());
    lv_obj_set_pos(selsta, 86 + pad_dx, selsta_y);
    lv_obj_set_style_text_color(selsta, lv_color_hex(0x8C8C96), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(selsta, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *quit_hint = lv_label_create(pad);
    lv_label_set_text(quit_hint, "Quit=TAB");
    lv_obj_align(quit_hint, LV_ALIGN_BOTTOM_RIGHT, -8, -6);
    lv_obj_set_style_text_color(quit_hint, lv_color_hex(0x9A9AA5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(quit_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *foot = lv_label_create(panel);
    lv_label_set_text(foot, "Press any key to start");
    lv_obj_align(foot, LV_ALIGN_BOTTOM_RIGHT, -12, -6);
    lv_obj_set_style_text_color(foot, lv_color_hex(0x7A7A84), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(foot, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
}


