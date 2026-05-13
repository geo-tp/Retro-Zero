

#include "lvgl/lvgl.h"
#include "lvgl/src/font/lv_font.h"
LV_FONT_DECLARE(lv_font_montserrat_16)

#include "app_ui.h"
#include "app_platform.h"
#include "app_settings.h"
#include "launch_controls_view.h"
#include "rom_browser.h"
#include "../Core/core_registry.h"
#include "../Core/core_downloader.h"
#include "../Upload/rom_upload_server.h"
#include "console_art_renderer.h"
#include "../Utils/file_utils.h"
#include "../Utils/bios_utils.h"

#include <linux/input.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
const std::vector<CoreConfig>& g_consoles = CoreRegistry::all();

enum class View {
    ConsolePicker,
    FilePicker,
    Settings,
};

enum class SettingsPage {
    Main,
    InputRemap,
};

enum class PromptKind {
    None,
    CoreDownload,
    ResetDefaults,
};

struct SlotGeom {
    int x;
    int y;
    int w;
    int h;
};

struct CarouselCard {
    lv_obj_t *panel = nullptr;
    lv_obj_t *code = nullptr;
    lv_obj_t *name = nullptr;
    lv_obj_t *logo_badge = nullptr;
    lv_obj_t *logo_text = nullptr;
    lv_obj_t *art = nullptr;
};

constexpr std::array<SlotGeom, 5> kSlots = {{
    {-177, 18, 61, 61},
    {-99, 10, 81, 81},
    {0, 2, 101, 101},
    {99, 10, 81, 81},
    {177, 18, 61, 61},
}};

View g_view = View::ConsolePicker;
int g_console_index = 0;
int g_file_index = 0;
bool g_running = true;
bool g_selected = false;
bool g_animating = false;
bool g_launching = false;
int g_settings_index = 0;
SettingsPage g_settings_page = SettingsPage::Main;
int g_capture_bind_index = -1;

std::string g_rom_dir;
std::string g_filter;
std::string g_selected_core;
std::string g_selected_rom;
std::vector<FileEntry> g_files;
std::vector<int> g_input_fds;

lv_obj_t *g_root = nullptr;
lv_obj_t *g_time_label = nullptr;
lv_obj_t *g_power_bar = nullptr;
lv_obj_t *g_power_label = nullptr;
lv_obj_t *g_title = nullptr;
lv_obj_t *g_hint = nullptr;
lv_obj_t *g_path_label = nullptr;
lv_obj_t *g_filter_label = nullptr;
lv_obj_t *g_count_label = nullptr;
lv_obj_t *g_file_list = nullptr;
lv_obj_t *g_no_results_box = nullptr;
lv_obj_t *g_error_overlay = nullptr;
lv_obj_t *g_core_prompt_overlay = nullptr;
lv_obj_t *g_core_prompt_left = nullptr;
lv_obj_t *g_core_prompt_right = nullptr;
lv_obj_t *g_core_prompt_left_label = nullptr;
lv_obj_t *g_core_prompt_right_label = nullptr;
lv_obj_t *g_core_prompt_msg = nullptr;
bool g_core_prompt_ok_selected = true;
bool g_core_prompt_busy = false;
int g_core_prompt_console = -1;
std::string g_core_prompt_rom;
PromptKind g_core_prompt_kind = PromptKind::None;
std::array<CarouselCard, 5> g_cards {};
std::array<lv_obj_t *, 24> g_dots {};
AppConfig g_app_config;

constexpr uint32_t kNavHoldDelayMs = 220;
constexpr uint32_t kNavHoldRepeatMs = 65;
std::array<uint8_t, KEY_MAX + 1> g_nav_key_down {};
std::array<uint32_t, KEY_MAX + 1> g_nav_key_next_ms {};

constexpr std::array<unsigned, 10> kRepeatNavKeys = {{
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_Z, KEY_C, KEY_F, KEY_X,
    KEY_DELETE, KEY_BACKSPACE,
}};

std::string getenv_or(const char *name, const char *fallback)
{
    const char *value = std::getenv(name);
    return value && value[0] ? value : fallback;
}

int wrap_console(int index)
{
    int n = static_cast<int>(g_consoles.size());
    while (index < 0) index += n;
    return index % n;
}

void render_settings_rows();
void clear_screen();
void build_header();
void update_time();

void build_settings_menu(SettingsPage page)
{
    g_view = View::Settings;
    g_settings_page = page;
    g_capture_bind_index = -1;
    g_settings_index = 0;

    clear_screen();
    build_header();

    lv_obj_t *title_bar = lv_obj_create(g_root);
    lv_obj_set_size(title_bar, 320, 16);
    lv_obj_set_pos(title_bar, 0, 28);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *badge = lv_obj_create(title_bar);
    lv_obj_set_size(badge, 34, 14);
    lv_obj_set_pos(badge, 6, 1);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(badge, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x3D5A80), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(badge, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(badge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(badge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *badge_txt = lv_label_create(badge);
    lv_label_set_text(badge_txt, "CFG");
    lv_obj_center(badge_txt);
    lv_obj_set_style_text_color(badge_txt, lv_color_hex(0xEAF0FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(badge_txt, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_title = lv_label_create(title_bar);
    lv_label_set_text(g_title, page == SettingsPage::Main ? "General / Video / Sound / Input" : "Input Remap");
    lv_obj_set_pos(g_title, 46, 2);
    lv_obj_set_style_text_color(g_title, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_title, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_filter_label = lv_label_create(g_root);
    lv_obj_set_width(g_filter_label, 308);
    lv_obj_set_pos(g_filter_label, 6, 48);
    lv_label_set_recolor(g_filter_label, true);
    lv_obj_set_style_text_font(g_filter_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_filter_label, lv_color_hex(0x666666), LV_PART_MAIN | LV_STATE_DEFAULT);

    g_file_list = lv_obj_create(g_root);
    lv_obj_set_size(g_file_list, 308, 108);
    lv_obj_set_pos(g_file_list, 6, 60);
    lv_obj_clear_flag(g_file_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_file_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_file_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_file_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(g_file_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    render_settings_rows();
    update_time();
}

void render_settings_rows()
{
    if (!g_file_list) return;
    lv_obj_clean(g_file_list);

    std::vector<std::string> rows;
    if (g_settings_page == SettingsPage::Main) {
        rows.push_back("General: Reset defaults");
        rows.push_back(std::string("Video: Default mode = ") + (g_app_config.default_4x3 ? "4:3" : "Fullscreen"));
        rows.push_back("Sound: Default volume = " + std::to_string(g_app_config.volume_percent) + "%");
        rows.push_back("Display: Brightness = " + std::to_string(g_app_config.brightness_percent) + "%");
        rows.push_back("Input: Remap buttons...");
        rows.push_back("Back");
    } else {
        for (size_t i = 0; i < g_app_config.binds.size(); ++i) {
            rows.push_back(std::string(kBindActions[i].label) + " = " + key_name_for_code(g_app_config.binds[i]));
        }
        rows.push_back("Back");
    }

    if (rows.empty()) return;
    if (g_settings_index < 0) g_settings_index = 0;
    if (g_settings_index >= static_cast<int>(rows.size())) g_settings_index = static_cast<int>(rows.size()) - 1;

    constexpr int visible_rows = 6;
    constexpr int row_h = 17;
    constexpr int row_step = 18;
    int start = std::max(0, g_settings_index - 2);
    int end = std::min(static_cast<int>(rows.size()), start + visible_rows);
    start = std::max(0, end - visible_rows);

    for (int i = start; i < end; ++i) {
        bool selected = i == g_settings_index;
        lv_obj_t *row = lv_obj_create(g_file_list);
        lv_obj_set_size(row, 308, row_h);
        lv_obj_set_pos(row, 0, (i - start) * row_step);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(row, selected ? 4 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(row, lv_color_hex(selected ? 0x3D5A80 : 0x141414), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(row, selected ? 255 : 140, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, rows[i].c_str());
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(label, 300);
        lv_obj_set_pos(label, 4, 2);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label,
                                    lv_color_hex(selected ? 0xEAF0FF : 0xCCCCCC),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (g_filter_label) {
        if (g_capture_bind_index >= 0) {
            lv_label_set_text(g_filter_label, "Press a key to bind, BACKSPACE to cancel");
        } else if (g_settings_page == SettingsPage::Main) {
            lv_label_set_text(g_filter_label, "Use Left/Right to adjust values");
        } else {
            lv_label_set_text(g_filter_label, "Enter to rebind selected action");
        }
    }
}


void show_error_overlay(const std::string &title, const std::string &body);
void show_success_overlay(const std::string &title, const std::string &body);

void launch_rom_upload_tool()
{
    UploadServer::StartResult result = UploadServer::start(g_consoles);
    if (!result.ok) {
        const std::string title = result.error.find("Wi-Fi") != std::string::npos ?
            "Wi-Fi not connected" : "Upload server failed";
        show_error_overlay(title, result.error);
        return;
    }

    show_success_overlay("ROM Upload Ready", result.message);
}


std::string ellipsize_text(const std::string &text, size_t max_chars)
{
    if (text.size() <= max_chars) return text;
    if (max_chars <= 3) return std::string(max_chars, '.');
    return text.substr(0, max_chars - 3) + "...";
}

std::string core_hint_for_file_picker()
{
    std::string core_path = CoreDownloader::resolveInstalledCore(g_consoles[g_console_index]);
    std::string core_name = file_name_from_path(core_path);
    if (core_name.empty()) core_name = core_path;
    if (core_name.empty()) core_name = "unknown";

    const std::string suffix = "_libretro.so";
    if (core_name.size() > suffix.size() &&
        core_name.compare(core_name.size() - suffix.size(), suffix.size(), suffix) == 0) {
        core_name.erase(core_name.size() - suffix.size());
    }

    return "core: " + ellipsize_text(core_name, 16);
}

lv_obj_t *make_label(lv_obj_t *parent, const char *text, int y, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, LV_SIZE_CONTENT);
    lv_obj_set_align(label, LV_ALIGN_CENTER);
    lv_obj_set_y(label, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return label;
}

const char *card_logo_text(const CoreConfig &console)
{
    return console.shortName;
}

void set_obj_geom(lv_obj_t *obj, const SlotGeom &slot)
{
    lv_obj_set_size(obj, slot.w, slot.h);
    lv_obj_set_x(obj, slot.x);
    lv_obj_set_y(obj, slot.y);
    lv_obj_set_align(obj, LV_ALIGN_CENTER);
}

void update_time()
{
    if (!g_time_label) return;

    time_t now = time(nullptr);
    struct tm tmv {};
    localtime_r(&now, &tmv);

    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    lv_label_set_text(g_time_label, buf);

    if (!g_power_label || !g_power_bar) return;

    UiPowerStatus power;
    if (!read_ui_power_status(power)) {
        lv_label_set_text(g_power_label, "--");
        lv_bar_set_value(g_power_bar, 0, LV_ANIM_OFF);
        return;
    }

    char power_text[16];
    snprintf(power_text, sizeof(power_text), power.charging ? "+%d%%" : "%d%%", power.percentage);
    lv_label_set_text(g_power_label, power_text);
    lv_bar_set_value(g_power_bar, power.percentage, LV_ANIM_OFF);
}

void dismiss_error_overlay()
{
    if (g_error_overlay) {
        lv_obj_del(g_error_overlay);
        g_error_overlay = nullptr;
    }
}

void show_error_overlay(const std::string &title, const std::string &body)
{
    dismiss_error_overlay();

    g_error_overlay = lv_obj_create(g_root);
    lv_obj_set_size(g_error_overlay, 308, 106);
    lv_obj_set_pos(g_error_overlay, 6, 58);
    lv_obj_clear_flag(g_error_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_error_overlay, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_error_overlay, lv_color_hex(0x1A0808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_error_overlay, 248, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_error_overlay, lv_color_hex(0xFF4444), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_error_overlay, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(g_error_overlay, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *etitle = lv_label_create(g_error_overlay);
    lv_label_set_text(etitle, title.c_str());
    lv_obj_align(etitle, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(etitle, lv_color_hex(0xFF5555), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(etitle, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *emsg = lv_label_create(g_error_overlay);
    lv_label_set_text(emsg, body.c_str());
    lv_label_set_long_mode(emsg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(emsg, 288);
    lv_obj_align(emsg, LV_ALIGN_TOP_LEFT, 0, 20);
    lv_obj_set_style_text_color(emsg, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(emsg, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *hint = lv_label_create(g_error_overlay);
    lv_label_set_text(hint, "Press any key");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_move_foreground(g_error_overlay);
}

void show_success_overlay(const std::string &title, const std::string &body)
{
    dismiss_error_overlay();

    const bool is_rom_upload_ready = (title == "ROM Upload Ready");

    g_error_overlay = lv_obj_create(g_root);
    lv_obj_set_size(g_error_overlay, 308, 106);
    lv_obj_set_pos(g_error_overlay, 6, 58);
    lv_obj_clear_flag(g_error_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_error_overlay, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_error_overlay, lv_color_hex(0x08180A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_error_overlay, 248, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_error_overlay, lv_color_hex(0x3AC65A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_error_overlay, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(g_error_overlay, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *etitle = lv_label_create(g_error_overlay);
    lv_label_set_text(etitle, title.c_str());
    lv_obj_align(etitle, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(etitle, lv_color_hex(0x7BE58A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(etitle, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (is_rom_upload_ready) {
        const size_t first_nl = body.find('\n');
        const size_t second_nl = (first_nl == std::string::npos) ? std::string::npos : body.find('\n', first_nl + 1);

        const std::string intro = (first_nl == std::string::npos) ? body : body.substr(0, first_nl);
        const std::string addr = (first_nl == std::string::npos)
            ? std::string()
            : body.substr(first_nl + 1, second_nl == std::string::npos ? std::string::npos : second_nl - first_nl - 1);
        const std::string rest = (second_nl == std::string::npos) ? std::string() : body.substr(second_nl + 1);

        lv_obj_t *intro_lbl = lv_label_create(g_error_overlay);
        lv_label_set_text(intro_lbl, intro.c_str());
        lv_obj_set_width(intro_lbl, 288);
        lv_obj_align(intro_lbl, LV_ALIGN_TOP_LEFT, 0, 24);
        lv_obj_set_style_text_color(intro_lbl, lv_color_hex(0xD0E8D2), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(intro_lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *addr_lbl = lv_label_create(g_error_overlay);
        lv_label_set_text(addr_lbl, addr.c_str());
        lv_obj_set_width(addr_lbl, 288);
        lv_obj_align(addr_lbl, LV_ALIGN_TOP_LEFT, 0, 38);
        lv_obj_set_style_text_color(addr_lbl, lv_color_hex(0xF0FFF2), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(addr_lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *rest_lbl = lv_label_create(g_error_overlay);
        lv_label_set_text(rest_lbl, rest.c_str());
        lv_label_set_long_mode(rest_lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(rest_lbl, 288);
        lv_obj_align(rest_lbl, LV_ALIGN_TOP_LEFT, 0, 46);
        lv_obj_set_style_text_color(rest_lbl, lv_color_hex(0xD0E8D2), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(rest_lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_t *emsg = lv_label_create(g_error_overlay);
        lv_label_set_text(emsg, body.c_str());
        lv_label_set_long_mode(emsg, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(emsg, 288);
        lv_obj_align(emsg, LV_ALIGN_TOP_LEFT, 0, 24);
        lv_obj_set_style_text_color(emsg, lv_color_hex(0xD0E8D2), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(emsg, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_obj_t *hint = lv_label_create(g_error_overlay);
    lv_label_set_text(hint, "Press any key");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_move_foreground(g_error_overlay);
}

void wait_for_any_key()
{
    const int step_us = 10000;
    input_event ev {};
    while (g_running) {
        for (int fd : g_input_fds) {
            while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_KEY && ev.code <= KEY_MAX && (ev.value == 1 || ev.value == 2)) {
                    return;
                }
            }
        }
        lv_timer_handler();
        usleep(step_us);
    }
}

void dismiss_core_prompt()
{
    if (g_core_prompt_overlay) {
        lv_obj_del(g_core_prompt_overlay);
        g_core_prompt_overlay = nullptr;
    }
    g_core_prompt_left = nullptr;
    g_core_prompt_right = nullptr;
    g_core_prompt_left_label = nullptr;
    g_core_prompt_right_label = nullptr;
    g_core_prompt_msg = nullptr;
    g_core_prompt_busy = false;
    g_core_prompt_console = -1;
    g_core_prompt_rom.clear();
    g_core_prompt_kind = PromptKind::None;
}

void update_core_prompt_choice()
{
    if (!g_core_prompt_left || !g_core_prompt_right) return;
    if (g_core_prompt_busy) {
        lv_obj_set_style_bg_color(g_core_prompt_left, lv_color_hex(0x1A1A1A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_core_prompt_right, lv_color_hex(0x1A1A1A), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (g_core_prompt_left_label) {
            lv_obj_set_style_text_color(g_core_prompt_left_label, lv_color_hex(0x777777), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (g_core_prompt_right_label) {
            lv_obj_set_style_text_color(g_core_prompt_right_label, lv_color_hex(0x777777), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        return;
    }
    lv_obj_set_style_bg_color(g_core_prompt_left,
                              lv_color_hex(g_core_prompt_ok_selected ? 0x1A1A1A : 0x7A3F12),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_core_prompt_right,
                              lv_color_hex(g_core_prompt_ok_selected ? 0xEE7500 : 0x1A1A1A),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    if (g_core_prompt_left_label) {
        lv_obj_set_style_text_color(g_core_prompt_left_label, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_core_prompt_right_label) {
        lv_obj_set_style_text_color(g_core_prompt_right_label, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void set_core_prompt_busy(bool busy)
{
    g_core_prompt_busy = busy;
    if (g_core_prompt_msg) {
        lv_label_set_text(g_core_prompt_msg, busy ? "Downloading core... Please wait" : "Do you want to download the core?");
    }
    update_core_prompt_choice();
}

void show_core_download_prompt(int console_index, const std::string &rom)
{
    dismiss_core_prompt();

    g_core_prompt_kind = PromptKind::CoreDownload;
    g_core_prompt_console = console_index;
    g_core_prompt_rom = rom;
    g_core_prompt_ok_selected = true;

    g_core_prompt_overlay = lv_obj_create(g_root);
    // Keep small side/bottom margins while covering the whole selection area.
    lv_obj_set_size(g_core_prompt_overlay, 308, 106);
    lv_obj_set_pos(g_core_prompt_overlay, 6, 58);
    lv_obj_clear_flag(g_core_prompt_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_core_prompt_overlay, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_core_prompt_overlay, lv_color_hex(0x1B1108), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_core_prompt_overlay, 248, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_core_prompt_overlay, lv_color_hex(0xEE7500), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_core_prompt_overlay, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(g_core_prompt_overlay, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *title = lv_label_create(g_core_prompt_overlay);
    lv_label_set_text(title, "Core missing");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFB066), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *msg = lv_label_create(g_core_prompt_overlay);
    g_core_prompt_msg = msg;
    lv_label_set_text(msg, "Do you want to download the core?");
    lv_obj_align(msg, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_obj_set_style_text_color(msg, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_core_prompt_left = lv_obj_create(g_core_prompt_overlay);
    lv_obj_set_size(g_core_prompt_left, 56, 20);
    lv_obj_align(g_core_prompt_left, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_radius(g_core_prompt_left, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_core_prompt_left, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(g_core_prompt_left, LV_OBJ_FLAG_SCROLLABLE);
    g_core_prompt_left_label = lv_label_create(g_core_prompt_left);
    lv_label_set_text(g_core_prompt_left_label, "<");
    lv_obj_center(g_core_prompt_left_label);
    lv_obj_set_style_text_color(g_core_prompt_left_label, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);

    g_core_prompt_right = lv_obj_create(g_core_prompt_overlay);
    lv_obj_set_size(g_core_prompt_right, 56, 20);
    lv_obj_align(g_core_prompt_right, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_radius(g_core_prompt_right, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_core_prompt_right, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(g_core_prompt_right, LV_OBJ_FLAG_SCROLLABLE);
    g_core_prompt_right_label = lv_label_create(g_core_prompt_right);
    lv_label_set_text(g_core_prompt_right_label, "OK");
    lv_obj_center(g_core_prompt_right_label);
    lv_obj_set_style_text_color(g_core_prompt_right_label, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);

    g_core_prompt_busy = false;
    update_core_prompt_choice();
    lv_obj_move_foreground(g_core_prompt_overlay);
}

void show_settings_reset_prompt()
{
    dismiss_core_prompt();

    g_core_prompt_kind = PromptKind::ResetDefaults;
    g_core_prompt_ok_selected = true;

    g_core_prompt_overlay = lv_obj_create(g_root);
    lv_obj_set_size(g_core_prompt_overlay, 308, 106);
    lv_obj_set_pos(g_core_prompt_overlay, 6, 58);
    lv_obj_clear_flag(g_core_prompt_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_core_prompt_overlay, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_core_prompt_overlay, lv_color_hex(0x0F141E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_core_prompt_overlay, 248, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_core_prompt_overlay, lv_color_hex(0xEE7500), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_core_prompt_overlay, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(g_core_prompt_overlay, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *title = lv_label_create(g_core_prompt_overlay);
    lv_label_set_text(title, "Reset settings");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xC9DBF5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *msg = lv_label_create(g_core_prompt_overlay);
    g_core_prompt_msg = msg;
    lv_label_set_text(msg, "Restore default config values?");
    lv_obj_align(msg, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_obj_set_style_text_color(msg, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_core_prompt_left = lv_obj_create(g_core_prompt_overlay);
    lv_obj_set_size(g_core_prompt_left, 56, 20);
    lv_obj_align(g_core_prompt_left, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_radius(g_core_prompt_left, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_core_prompt_left, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(g_core_prompt_left, LV_OBJ_FLAG_SCROLLABLE);
    g_core_prompt_left_label = lv_label_create(g_core_prompt_left);
    lv_label_set_text(g_core_prompt_left_label, "<");
    lv_obj_center(g_core_prompt_left_label);
    lv_obj_set_style_text_color(g_core_prompt_left_label, lv_color_hex(0xEAF0FF), LV_PART_MAIN | LV_STATE_DEFAULT);

    g_core_prompt_right = lv_obj_create(g_core_prompt_overlay);
    lv_obj_set_size(g_core_prompt_right, 56, 20);
    lv_obj_align(g_core_prompt_right, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_radius(g_core_prompt_right, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_core_prompt_right, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(g_core_prompt_right, LV_OBJ_FLAG_SCROLLABLE);
    g_core_prompt_right_label = lv_label_create(g_core_prompt_right);
    lv_label_set_text(g_core_prompt_right_label, "OK");
    lv_obj_center(g_core_prompt_right_label);
    lv_obj_set_style_text_color(g_core_prompt_right_label, lv_color_hex(0xEAF0FF), LV_PART_MAIN | LV_STATE_DEFAULT);

    g_core_prompt_busy = false;
    update_core_prompt_choice();
    lv_obj_move_foreground(g_core_prompt_overlay);
}

void clear_screen()
{
    if (g_root) lv_obj_del(g_root);
    g_no_results_box = nullptr;
    g_error_overlay = nullptr;
    g_core_prompt_overlay = nullptr;
    g_core_prompt_left = nullptr;
    g_core_prompt_right = nullptr;
    g_core_prompt_left_label = nullptr;
    g_core_prompt_right_label = nullptr;
    g_core_prompt_msg = nullptr;
    g_core_prompt_busy = false;
    g_core_prompt_console = -1;
    g_core_prompt_rom.clear();
    g_core_prompt_kind = PromptKind::None;
    g_root = lv_obj_create(nullptr);
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_root, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_root, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_disp_load_scr(g_root);
}

void build_header()
{
    lv_obj_t *header = lv_obj_create(g_root);
    lv_obj_set_size(header, 320, 27);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x0A0A0A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(header, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(header, lv_color_hex(0x2C2C2C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *brand = lv_label_create(header);
    lv_label_set_text(brand, "CARDPUTER RETRO ZERO");
    lv_obj_set_pos(brand, 8, 8);
    lv_obj_set_width(brand, 156);
    lv_obj_set_style_text_align(brand, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(brand, lv_color_hex(0x666666), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ver_badge = lv_obj_create(header);
    lv_obj_set_size(ver_badge, 44, 14);
    lv_obj_set_pos(ver_badge, 152, 7);
    lv_obj_clear_flag(ver_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ver_badge, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ver_badge, lv_color_hex(0x1F2F44), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ver_badge, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ver_badge, lv_color_hex(0x355474), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ver_badge, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ver_badge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ver_txt = lv_label_create(ver_badge);
    lv_label_set_text(ver_txt, "v0.1");
    lv_obj_center(ver_txt);
    lv_obj_set_style_text_color(ver_txt, lv_color_hex(0xBFD8F5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ver_txt, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_time_label = lv_label_create(header);
    lv_label_set_text(g_time_label, "--:--");
    lv_obj_set_pos(g_time_label, 220, 8);
    lv_obj_set_style_text_color(g_time_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_time_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *bat_box = lv_obj_create(header);
    lv_obj_set_size(bat_box, 56, 15);
    lv_obj_set_pos(bat_box, 258, 6);
    lv_obj_clear_flag(bat_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(bat_box, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(bat_box, lv_color_hex(0x1E1E1E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bat_box, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(bat_box, lv_color_hex(0x383838), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bat_box, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(bat_box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_power_bar = lv_bar_create(bat_box);
    lv_obj_set_size(g_power_bar, 56, 15);
    lv_obj_set_pos(g_power_bar, 0, 0);
    lv_bar_set_value(g_power_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(g_power_bar, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_power_bar, lv_color_hex(0x1E1E1E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_power_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_power_bar, 2, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_power_bar, lv_color_hex(0x555555), LV_PART_INDICATOR | LV_STATE_DEFAULT);

    g_power_label = lv_label_create(bat_box);
    lv_label_set_text(g_power_label, "--%");
    lv_obj_center(g_power_label);
    lv_obj_set_style_text_color(g_power_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_power_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void style_card(CarouselCard &card, int console_index, int slot)
{
    const CoreConfig &console = g_consoles[wrap_console(console_index)];
    bool center = slot == 2;

    lv_obj_clear_flag(card.panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(card.panel, center ? 18 : 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card.panel, lv_color_hex(0x121212), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card.panel, center ? 255 : 230, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card.panel, center ? 2 : 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(card.panel,
                                  lv_color_hex(center ? console.accent : 0x2A2A2A),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card.panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (center) {
        lv_obj_set_style_shadow_width(card.panel, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(card.panel, lv_color_hex(console.accent), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(card.panel, 160, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_spread(card.panel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_shadow_width(card.panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(card.panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_obj_set_style_text_color(card.code, lv_color_hex(center ? console.accent : 0xCCCCCC),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(card.code, center ? &lv_font_montserrat_28 : &lv_font_montserrat_20,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(card.name, lv_color_hex(center ? 0xDDDDDD : 0x777777),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(card.name, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_size(card.logo_badge, center ? 34 : 30, center ? 14 : 12);
    lv_obj_align(card.logo_badge, LV_ALIGN_TOP_LEFT, center ? 8 : 6, center ? 7 : 6);
    lv_obj_set_style_radius(card.logo_badge, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card.logo_badge, lv_color_hex(console.accent), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card.logo_badge, center ? 255 : 220, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card.logo_badge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card.logo_badge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(card.logo_text, lv_color_hex(0x0A0A0A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(card.logo_text, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
}


void fill_card_with_console(CarouselCard &card, int console_index, int slot)
{
    const CoreConfig &console = g_consoles[wrap_console(console_index)];
    lv_label_set_text(card.code, console.shortName);
    lv_label_set_text(card.name, console.name);
    lv_label_set_text(card.logo_text, card_logo_text(console));
    lv_obj_set_width(card.name, slot == 2 ? 92 : 70);
    lv_label_set_long_mode(card.name, LV_LABEL_LONG_CLIP);
    lv_obj_set_y(card.code, slot == 2 ? -15 : -11);
    lv_obj_set_y(card.name, slot == 2 ? 24 : 18);
    lv_obj_set_x(card.name, slot == 2 ? 4 : 0);
    style_card(card, console_index, slot);
    set_obj_geom(card.panel, kSlots[slot]);
    {
        bool has_art = ConsoleArtRenderer::rebuild(card.art, card.logo_badge, card.name, console, slot);
        if (has_art) lv_obj_add_flag(card.code, LV_OBJ_FLAG_HIDDEN);
        else         lv_obj_clear_flag(card.code, LV_OBJ_FLAG_HIDDEN);
    }
    if (slot == 0 || slot == 4) lv_obj_add_flag(card.panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(card.panel, LV_OBJ_FLAG_HIDDEN);
}

void fill_card(CarouselCard &card, int slot)
{
    fill_card_with_console(card, g_console_index + slot - 2, slot);
}

void update_dots()
{
    uint32_t accent = g_consoles[g_console_index].accent;
    int n = static_cast<int>(g_consoles.size());
    int total_w = (n - 1) * 12 + 10;
    int start_x = (320 - total_w) / 2;

    for (size_t i = 0; i < g_dots.size(); ++i) {
        if (!g_dots[i]) continue;
        bool active = static_cast<int>(i) == g_console_index;
        int size = active ? 10 : 5;
        lv_obj_set_size(g_dots[i], size, size);
        lv_obj_set_pos(g_dots[i], start_x + static_cast<int>(i) * 12, 148 + (active ? 0 : 2));
        lv_obj_set_style_bg_color(g_dots[i], lv_color_hex(active ? accent : 0x3A3A3A),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(g_dots[i], lv_color_hex(active ? accent : 0x3A3A3A),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(g_dots[i], active ? 3 : LV_RADIUS_CIRCLE,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void refresh_carousel()
{
    lv_label_set_text(g_title, g_consoles[g_console_index].name);
    lv_obj_set_style_text_color(g_title, lv_color_hex(g_consoles[g_console_index].accent),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    for (int i = 1; i <= 3; ++i) fill_card(g_cards[i], i);
    update_dots();
}

void anim_one(lv_obj_t *obj, int start, int end, lv_anim_exec_xcb_t cb, lv_anim_ready_cb_t ready = nullptr)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, start, end);
    lv_anim_set_duration(&a, 180);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, cb);
    if (ready) lv_anim_set_completed_cb(&a, ready);
    lv_anim_start(&a);
}

void anim_card_to_slot(CarouselCard &card, int from, int to, lv_anim_ready_cb_t ready = nullptr)
{
    if (!card.panel) return;
    if (to != 0 && to != 4) lv_obj_clear_flag(card.panel, LV_OBJ_FLAG_HIDDEN);
    anim_one(card.panel, kSlots[from].x, kSlots[to].x, [](void *o, int32_t v) { lv_obj_set_x((lv_obj_t *)o, v); });
    anim_one(card.panel, kSlots[from].y, kSlots[to].y, [](void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); });
    anim_one(card.panel, kSlots[from].w, kSlots[to].w, [](void *o, int32_t v) { lv_obj_set_width((lv_obj_t *)o, v); });
    anim_one(card.panel, kSlots[from].h, kSlots[to].h, [](void *o, int32_t v) { lv_obj_set_height((lv_obj_t *)o, v); }, ready);
}

void carousel_anim_done(lv_anim_t *)
{
    g_animating = false;
    refresh_carousel();
}

void move_console(int delta)
{
    if (g_animating) return;
    g_animating = true;
    if (delta > 0) {
        fill_card_with_console(g_cards[1], g_console_index + 2, 4);
        lv_obj_clear_flag(g_cards[1].panel, LV_OBJ_FLAG_HIDDEN);
        anim_card_to_slot(g_cards[1], 4, 3, carousel_anim_done);
        anim_card_to_slot(g_cards[2], 2, 1);
        anim_card_to_slot(g_cards[3], 3, 2);
        g_console_index = wrap_console(g_console_index + 1);
        std::rotate(g_cards.begin() + 1, g_cards.begin() + 2, g_cards.begin() + 4);
    } else {
        fill_card_with_console(g_cards[3], g_console_index - 2, 0);
        lv_obj_clear_flag(g_cards[3].panel, LV_OBJ_FLAG_HIDDEN);
        anim_card_to_slot(g_cards[3], 0, 1, carousel_anim_done);
        anim_card_to_slot(g_cards[2], 2, 3);
        anim_card_to_slot(g_cards[1], 1, 2);
        g_console_index = wrap_console(g_console_index - 1);
        std::rotate(g_cards.begin() + 1, g_cards.begin() + 3, g_cards.begin() + 4);
    }
}

void build_console_picker()
{
    g_view = View::ConsolePicker;
    clear_screen();
    build_header();

    g_title = make_label(g_root, "", 26, 0xFFFFFF);
    lv_obj_set_style_text_font(g_title, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    for (int i = 1; i <= 3; ++i) {
        g_cards[i].panel = lv_obj_create(g_root);
        g_cards[i].logo_badge = lv_obj_create(g_cards[i].panel);
        lv_obj_clear_flag(g_cards[i].logo_badge, LV_OBJ_FLAG_SCROLLABLE);
        g_cards[i].logo_text = lv_label_create(g_cards[i].logo_badge);
        lv_obj_center(g_cards[i].logo_text);
        g_cards[i].code = make_label(g_cards[i].panel, "", -10, 0xFFFFFF);
        g_cards[i].name = make_label(g_cards[i].panel, "", 18, 0xAAAAAA);
        // art container: transparent placeholder, sized on demand
        g_cards[i].art = lv_obj_create(g_cards[i].panel);
        lv_obj_set_size(g_cards[i].art, 0, 0);
        lv_obj_clear_flag(g_cards[i].art, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(g_cards[i].art, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_cards[i].art, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(g_cards[i].art, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        fill_card(g_cards[i], i);
    }

    int n = std::min(static_cast<int>(g_consoles.size()), static_cast<int>(g_dots.size()));
    for (int i = 0; i < n; ++i) {
        g_dots[i] = lv_obj_create(g_root);
        lv_obj_clear_flag(g_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(g_dots[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_dots[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    for (int i = n; i < static_cast<int>(g_dots.size()); ++i) g_dots[i] = nullptr;

    g_hint = nullptr;

    refresh_carousel();
    update_time();
}

void load_files()
{
    g_files = load_rom_entries(g_rom_dir, g_consoles[g_console_index], g_filter);
    if (g_file_index >= static_cast<int>(g_files.size())) {
        g_file_index = static_cast<int>(g_files.size()) - 1;
    }
    if (g_file_index < 0) {
        g_file_index = 0;
    }
}

void render_file_rows()
{
    lv_obj_clean(g_file_list);
    if (g_no_results_box) {
        lv_obj_del(g_no_results_box);
        g_no_results_box = nullptr;
    }

    constexpr int visible_rows = 5;
    constexpr int row_h = 17;
    constexpr int row_step = 18;
    int start = std::max(0, g_file_index - 2);
    int end = std::min(static_cast<int>(g_files.size()), start + visible_rows);
    start = std::max(0, end - visible_rows);
    uint32_t accent = g_consoles[g_console_index].accent;

    for (int i = start; i < end; ++i) {
        const FileEntry &entry = g_files[i];
        bool selected = i == g_file_index;
        lv_obj_t *row = lv_obj_create(g_file_list);
        lv_obj_set_size(row, 308, row_h);
        lv_obj_set_pos(row, 0, (i - start) * row_step);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(row, selected ? 4 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(row, lv_color_hex(selected ? accent : 0x141414), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(row, selected ? 255 : 140, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *label = lv_label_create(row);
        std::string text;
        if (entry.dir && entry.name == "..") text = "<  ..";
        else if (entry.dir) text = "/  " + entry.name;
        else text = "   " + entry.name;
        lv_label_set_text(label, text.c_str());
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(label, 300);
        lv_obj_set_pos(label, 4, 2);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label,
                                    lv_color_hex(selected ? 0x080808 : (entry.dir ? 0x88BBFF : 0xCCCCCC)),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    bool has_filtered_result = false;
    for (const FileEntry &entry : g_files) {
        if (!(entry.dir && entry.name == "..")) {
            has_filtered_result = true;
            break;
        }
    }
    if (!has_filtered_result) {
        g_no_results_box = lv_obj_create(g_root);
        lv_obj_set_size(g_no_results_box, 250, g_filter.empty() ? 56 : 34);
        int y_offset = g_filter.empty() ? -10 : -28;
        lv_obj_align(g_no_results_box, LV_ALIGN_BOTTOM_MID, 0, y_offset);
        lv_obj_clear_flag(g_no_results_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(g_no_results_box, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_no_results_box, lv_color_hex(0x121212), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(g_no_results_box, 242, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(g_no_results_box, lv_color_hex(0x3A3A3A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_no_results_box, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(g_no_results_box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *label = lv_label_create(g_no_results_box);
        std::string msg;
        if (g_filter.empty()) {
            msg = "Use upload or drop ROMs in:\n" + g_rom_dir;
        } else {
            std::string display_filter = g_filter;
            if (display_filter.size() > 20) {
                display_filter = display_filter.substr(0, 17) + "...";
            }
            msg = "No results for\n" + display_filter;
        }
        lv_label_set_text(label, msg.c_str());
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(label, 238);
        lv_obj_center(label);
        lv_obj_set_style_text_color(label, lv_color_hex(0xC7C7C7), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_move_foreground(g_no_results_box);
    }

    if (g_filter_label) {
        std::string text = g_filter.empty()
            ? "type to filter"
            : ("#55CC66 o# #666666filter: " + g_filter + "#");
        lv_label_set_text(g_filter_label, text.c_str());
    }
    if (g_count_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d/%d", g_files.empty() ? 0 : g_file_index + 1, (int)g_files.size());
        lv_label_set_text(g_count_label, buf);
    }
    if (g_path_label) {
        lv_label_set_text(g_path_label, g_rom_dir.c_str());
    }
}

void build_file_picker()
{
    g_view = View::FilePicker;
    clear_screen();
    build_header();

    const CoreConfig &con = g_consoles[g_console_index];

    lv_obj_t *title_bar = lv_obj_create(g_root);
    lv_obj_set_size(title_bar, 320, 16);
    lv_obj_set_pos(title_bar, 0, 28);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *badge = lv_obj_create(title_bar);
    lv_obj_set_size(badge, 34, 14);
    lv_obj_set_pos(badge, 6, 1);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(badge, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(badge, lv_color_hex(con.accent), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(badge, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(badge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(badge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *badge_txt = lv_label_create(badge);
    lv_label_set_text(badge_txt, con.shortName);
    lv_obj_center(badge_txt);
    lv_obj_set_style_text_color(badge_txt, lv_color_hex(0x080808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(badge_txt, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_title = lv_label_create(title_bar);
    lv_label_set_text(g_title, con.name);
    lv_obj_set_pos(g_title, 46, 2);
    lv_obj_set_style_text_color(g_title, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_title, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *back_hint = lv_label_create(title_bar);
    std::string core_hint = core_hint_for_file_picker();
    lv_label_set_text(back_hint, core_hint.c_str());
    lv_label_set_long_mode(back_hint, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(back_hint, 140);
    lv_obj_align(back_hint, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_text_align(back_hint, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(back_hint, lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(back_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_path_label = lv_label_create(g_root);
    lv_label_set_text(g_path_label, g_rom_dir.c_str());
    lv_label_set_long_mode(g_path_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(g_path_label, 308);
    lv_obj_set_pos(g_path_label, 6, 47);
    lv_obj_set_style_text_font(g_path_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_path_label, lv_color_hex(0x4A4A4A), LV_PART_MAIN | LV_STATE_DEFAULT);

    g_filter_label = lv_label_create(g_root);
    lv_obj_set_width(g_filter_label, 210);
    lv_obj_set_pos(g_filter_label, 6, 60);
    lv_label_set_recolor(g_filter_label, true);
    lv_obj_set_style_text_font(g_filter_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_filter_label, lv_color_hex(0x666666), LV_PART_MAIN | LV_STATE_DEFAULT);

    g_count_label = lv_label_create(g_root);
    lv_obj_set_width(g_count_label, 98);
    lv_obj_set_pos(g_count_label, 216, 60);
    lv_obj_set_style_text_align(g_count_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_count_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_count_label, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *sep = lv_obj_create(g_root);
    lv_obj_set_size(sep, 308, 1);
    lv_obj_set_pos(sep, 6, 73);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x232323), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(sep, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_file_list = lv_obj_create(g_root);
    lv_obj_set_size(g_file_list, 308, 89);
    lv_obj_set_pos(g_file_list, 6, 75);
    lv_obj_clear_flag(g_file_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_file_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_file_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_file_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(g_file_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    load_files();
    render_file_rows();
    update_time();
}

void blank_before_launch()
{
    const CoreConfig &console = g_consoles[g_console_index];
    draw_launch_controls_screen(g_root, console, g_selected_rom.empty() ? g_selected_core : g_selected_rom, g_app_config);
    lv_refr_now(nullptr);
    wait_for_any_key();
    if (g_root) {
        lv_obj_set_style_bg_color(g_root, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clean(g_root);
        g_time_label = nullptr;
        g_power_bar = nullptr;
        g_power_label = nullptr;
        g_title = nullptr;
        g_hint = nullptr;
        g_path_label = nullptr;
        g_filter_label = nullptr;
        g_count_label = nullptr;
        g_file_list = nullptr;
        g_no_results_box = nullptr;
        g_error_overlay = nullptr;
        g_core_prompt_overlay = nullptr;
        g_core_prompt_left = nullptr;
        g_core_prompt_right = nullptr;
        g_core_prompt_left_label = nullptr;
        g_core_prompt_right_label = nullptr;
        g_core_prompt_msg = nullptr;
        for (auto &c : g_cards) {
            c.panel = nullptr;
            c.code = nullptr;
            c.name = nullptr;
            c.logo_badge = nullptr;
            c.logo_text = nullptr;
            c.art = nullptr;
        }
        g_dots.fill(nullptr);
    }
    lv_refr_now(nullptr);
    usleep(30000);
}

void launch_rom(const std::string &rom)
{
    unsetenv("CP0_SYSTEM_DIR_OVERRIDE");
    const CoreConfig &console = g_consoles[g_console_index];
    setenv("CP0_SELECTED_ENV_CORE", console.envCore, 1);

    if (std::strcmp(console.envCore, "CP0_CORE_GBA") == 0 && lower_ext(rom) == ".zip") {
        show_error_overlay("GBA ZIP not supported", "Use .gba ROM files. Please unzip the archive first.");
        return;
    }

    if (console.needsBios && std::strcmp(console.envCore, "CP0_CORE_NEOGEO") == 0) {
        const NeoGeoBiosCheck bios = check_neogeo_bios_for_rom(
            rom,
            g_rom_dir,
            find_rom_dir_for_console(g_consoles[g_console_index])
        );
        if (!bios.ok) {
            show_error_overlay("Missing NeoGeo BIOS", bios.error_body);
            return;
        }
        if (!bios.bios_dir.empty()) {
            setenv("CP0_SYSTEM_DIR_OVERRIDE", bios.bios_dir.c_str(), 1);
        }
    }

    g_selected_core = CoreDownloader::resolveInstalledCore(g_consoles[g_console_index]);
    if (g_selected_core.empty() || !regular_file_exists(g_selected_core)) {
        show_core_download_prompt(g_console_index, rom);
        return;
    }

    g_selected_rom = rom;
    g_selected = true;
    g_launching = true;
    blank_before_launch();
    g_running = false;
}

int key_to_action(unsigned code)
{
    switch (code) {
    case KEY_Z: return KEY_LEFT;
    case KEY_C: return KEY_RIGHT;
    case KEY_F: return KEY_UP;
    case KEY_X: return KEY_DOWN;
    case KEY_J:
    case KEY_K:
    case KEY_ENTER:
    case KEY_1: return KEY_ENTER;
    case KEY_2:
    case KEY_BACKSPACE: return KEY_BACKSPACE;
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_UP:
    case KEY_DOWN: return -1;
    default: return static_cast<int>(code);
    }
}

char letter_for_key(unsigned code)
{
    switch (code) {
    case KEY_SPACE: return ' ';
    case KEY_A: return 'a';
    case KEY_B: return 'b';
    case KEY_D: return 'd';
    case KEY_E: return 'e';
    case KEY_G: return 'g';
    case KEY_H: return 'h';
    case KEY_I: return 'i';
    case KEY_J: return 'j';
    case KEY_K: return 'k';
    case KEY_L: return 'l';
    case KEY_M: return 'm';
    case KEY_N: return 'n';
    case KEY_O: return 'o';
    case KEY_P: return 'p';
    case KEY_Q: return 'q';
    case KEY_R: return 'r';
    case KEY_S: return 's';
    case KEY_T: return 't';
    case KEY_U: return 'u';
    case KEY_V: return 'v';
    case KEY_W: return 'w';
    case KEY_Y: return 'y';
    case KEY_Z:
    case KEY_C:
    case KEY_F:
    case KEY_X:
    case KEY_1:
    case KEY_2: return 0;
    default: break;
    }
    if (code >= KEY_3 && code <= KEY_9) return static_cast<char>('3' + (code - KEY_3));
    if (code == KEY_0) return '0';
    return 0;
}

void reload_files_keep_top()
{
    g_file_index = 0;
    load_files();
    render_file_rows();
}

void handle_key(unsigned raw_code)
{
    if (g_launching) {
        return;
    }

    if (g_view == View::Settings && g_capture_bind_index >= 0) {
        if (raw_code == KEY_BACKSPACE || raw_code == KEY_ESC) {
            g_capture_bind_index = -1;
            render_settings_rows();
            return;
        }

        bool forbidden_bind_key = (raw_code == KEY_TAB ||
                                   raw_code == KEY_ENTER ||
                                   raw_code == KEY_KPENTER ||
                                   raw_code == KEY_DELETE)  ||
                                   raw_code == KEY_R || raw_code == KEY_E;
#ifdef KEY_FN
        forbidden_bind_key = forbidden_bind_key || (raw_code == KEY_FN);
#endif
        if (forbidden_bind_key) {
            return;
        }

        if (raw_code <= KEY_MAX && raw_code != KEY_Q && raw_code != KEY_ESC) {
            g_app_config.binds[static_cast<size_t>(g_capture_bind_index)] = static_cast<int>(raw_code);
            g_capture_bind_index = -1;
            persist_and_apply_app_config(g_app_config);
            render_settings_rows();
            return;
        }
    }

    int code = key_to_action(raw_code);
    const bool fn_left = (raw_code == KEY_LEFT);
    const bool fn_right = (raw_code == KEY_RIGHT);
    const bool fn_arrow = fn_left || fn_right || raw_code == KEY_UP || raw_code == KEY_DOWN;
    if (code == -1 && fn_arrow) {
        code = static_cast<int>(raw_code);
    }
    if (g_core_prompt_overlay) {
        if (g_core_prompt_busy) return;
        if (code == KEY_LEFT) {
            g_core_prompt_ok_selected = false;
            update_core_prompt_choice();
            return;
        }
        if (code == KEY_RIGHT) {
            g_core_prompt_ok_selected = true;
            update_core_prompt_choice();
            return;
        }
        if (code == KEY_BACKSPACE) {
            dismiss_core_prompt();
            return;
        }
        if (code == KEY_ENTER) {
            if (g_core_prompt_kind == PromptKind::ResetDefaults) {
                bool do_reset = g_core_prompt_ok_selected;
                dismiss_core_prompt();
                if (!do_reset) {
                    return;
                }
                reset_app_config_defaults(g_app_config);
                persist_and_apply_app_config(g_app_config);
                show_success_overlay("Settings", "Defaults restored.");
                render_settings_rows();
                return;
            }
            if (g_core_prompt_kind != PromptKind::CoreDownload) {
                dismiss_core_prompt();
                return;
            }
            bool do_download = g_core_prompt_ok_selected;
            int console_index = g_core_prompt_console;
            std::string rom = g_core_prompt_rom;
            if (!do_download) {
                dismiss_core_prompt();
                return;
            }
            if (console_index < 0 || console_index >= static_cast<int>(g_consoles.size())) {
                dismiss_core_prompt();
                return;
            }

            set_core_prompt_busy(true);
            lv_refr_now(nullptr);

            std::string err;
            if (!CoreDownloader::downloadForConsole(g_consoles[console_index], err)) {
                dismiss_core_prompt();
                show_error_overlay("Download failed", err);
                return;
            }
            dismiss_core_prompt();
            show_success_overlay("Core installed", "Core downloaded and installed successfully.");
            lv_refr_now(nullptr);
            usleep(2000000);
            dismiss_error_overlay();
            launch_rom(rom);
            return;
        }
        return;
    }

    if (g_error_overlay) {
        dismiss_error_overlay();
        return;
    }

    if (raw_code == KEY_Q || raw_code == KEY_ESC) {
        g_running = false;
        return;
    }

    if (g_view == View::ConsolePicker) {
        int code = key_to_action(raw_code);
        if (code == KEY_LEFT) move_console(-1);
        else if (code == KEY_RIGHT) move_console(1);
        else if (code == KEY_ENTER) {
            const CoreConfig &console = g_consoles[g_console_index];
            if (is_rom_upload_console(console)) {
                launch_rom_upload_tool();
                return;
            }
            if (is_settings_console(console)) {
                build_settings_menu(SettingsPage::Main);
                return;
            }
            g_rom_dir = find_rom_dir_for_console(g_consoles[g_console_index]);
            g_filter.clear();
            g_file_index = 0;
            build_file_picker();
        }
        return;
    }

    if (g_view == View::Settings) {
        const int main_count = 6;
        const int remap_count = static_cast<int>(g_app_config.binds.size()) + 1;
        const int item_count = (g_settings_page == SettingsPage::Main) ? main_count : remap_count;

        if (code == KEY_UP && g_settings_index > 0) {
            --g_settings_index;
            render_settings_rows();
            return;
        }
        if (code == KEY_DOWN && g_settings_index + 1 < item_count) {
            ++g_settings_index;
            render_settings_rows();
            return;
        }

        // Allow exit from settings with TAB, like Z/<> in carousel
        if (code == KEY_TAB) {
            build_console_picker();
            return;
        }

        if (g_settings_page == SettingsPage::Main) {
            if (code == KEY_LEFT) {
                if (g_settings_index == 1) {
                    g_app_config.default_4x3 = !g_app_config.default_4x3;
                    persist_and_apply_app_config(g_app_config);
                    render_settings_rows();
                    return;
                }
                if (g_settings_index == 2) {
                    const int step = fn_left ? 10 : 1;
                    g_app_config.volume_percent = std::max(0, g_app_config.volume_percent - step);
                    persist_and_apply_app_config(g_app_config);
                    render_settings_rows();
                    return;
                }
                if (g_settings_index == 3) {
                    g_app_config.brightness_percent = std::max(0, g_app_config.brightness_percent - 1);
                    persist_and_apply_app_config(g_app_config);
                    render_settings_rows();
                    return;
                }
                build_console_picker();
                return;
            }

            if (code == KEY_RIGHT) {
                if (g_settings_index == 1) {
                    g_app_config.default_4x3 = !g_app_config.default_4x3;
                    persist_and_apply_app_config(g_app_config);
                    render_settings_rows();
                    return;
                }
                if (g_settings_index == 2) {
                    const int step = fn_right ? 10 : 1;
                    g_app_config.volume_percent = std::min(100, g_app_config.volume_percent + step);
                    persist_and_apply_app_config(g_app_config);
                    render_settings_rows();
                    return;
                }
                if (g_settings_index == 3) {
                    g_app_config.brightness_percent = std::min(100, g_app_config.brightness_percent + 1);
                    persist_and_apply_app_config(g_app_config);
                    render_settings_rows();
                    return;
                }
            }

            if (code == KEY_ENTER) {
                if (g_settings_index == 0) {
                    show_settings_reset_prompt();
                    return;
                }
                if (g_settings_index == 1) {
                    g_app_config.default_4x3 = !g_app_config.default_4x3;
                    persist_and_apply_app_config(g_app_config);
                    render_settings_rows();
                    return;
                }
                if (g_settings_index == 2) {
                    g_app_config.volume_percent = std::min(100, g_app_config.volume_percent + 1);
                    persist_and_apply_app_config(g_app_config);
                    render_settings_rows();
                    return;
                }
                if (g_settings_index == 3) {
                    g_app_config.brightness_percent = std::min(100, g_app_config.brightness_percent + 1);
                    persist_and_apply_app_config(g_app_config);
                    render_settings_rows();
                    return;
                }
                if (g_settings_index == 4) {
                    build_settings_menu(SettingsPage::InputRemap);
                    return;
                }
                if (g_settings_index == 5) {
                    build_console_picker();
                    return;
                }
            }
        } else {
            if (code == KEY_LEFT || code == KEY_BACKSPACE) {
                build_settings_menu(SettingsPage::Main);
                return;
            }
            if (code == KEY_ENTER) {
                if (g_settings_index == remap_count - 1) {
                    build_settings_menu(SettingsPage::Main);
                } else if (g_settings_index >= 0 && g_settings_index < static_cast<int>(g_app_config.binds.size())) {
                    g_capture_bind_index = g_settings_index;
                    render_settings_rows();
                }
                return;
            }
        }
        return;
    }

    // In file picker, navigation uses mapped actions from Z/C/F/X and J/K/1/2.
    // Exception requested: physical C must type 'c' in file picker (not OK/right).
    if (raw_code == KEY_C) {
        g_filter.push_back('c');
        reload_files_keep_top();
        return;
    }

    if (code == KEY_UP && g_file_index > 0) {
        --g_file_index;
        render_file_rows();
    } else if (code == KEY_DOWN && g_file_index + 1 < static_cast<int>(g_files.size())) {
        ++g_file_index;
        render_file_rows();
    } else if (code == KEY_LEFT || raw_code == KEY_TAB) {
        // Always return to console picker regardless of current directory
        build_console_picker();
    } else if (code == KEY_BACKSPACE || raw_code == KEY_DELETE) {
        if (!g_filter.empty()) {
            g_filter.pop_back();
            reload_files_keep_top();
        }
    } else if (code == KEY_ENTER && raw_code != KEY_J && raw_code != KEY_K) {
        if (g_files.empty()) return;
        FileEntry entry = g_files[g_file_index];
        if (entry.dir) {
            if (entry.path == "/") {
                build_console_picker();
            } else {
                g_rom_dir = entry.path;
                g_filter.clear();
                g_file_index = 0;
                build_file_picker();
            }
        } else {
            launch_rom(entry.path);
        }
    } else {
        char c = letter_for_key(raw_code);
        if (c) {
            g_filter.push_back(c);
            reload_files_keep_top();
        }
    }
}

void poll_input()
{
    if (g_launching) {
        return;
    }

    auto is_nav_repeat_key = [](unsigned code) {
        switch (code) {
        case KEY_LEFT:
        case KEY_RIGHT:
        case KEY_UP:
        case KEY_DOWN:
        case KEY_Z:
        case KEY_C:
        case KEY_F:
        case KEY_X:
        case KEY_DELETE:
        case KEY_BACKSPACE:
            return true;
        default:
            return false;
        }
    };

    input_event ev {};
    for (int fd : g_input_fds) {
        while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type != EV_KEY || ev.code > KEY_MAX) continue;

            if (ev.value == 0) {
                if (is_nav_repeat_key(ev.code)) g_nav_key_down[ev.code] = 0;
                continue;
            }

            if (ev.value == 1) {
                handle_key(ev.code);
                if (is_nav_repeat_key(ev.code)) {
                    g_nav_key_down[ev.code] = 1;
                    g_nav_key_next_ms[ev.code] = lv_tick_get() + kNavHoldDelayMs;
                }
                if (!g_running || g_launching) {
                    return;
                }
                continue;
            }

            if (ev.value == 2 && !is_nav_repeat_key(ev.code)) {
                handle_key(ev.code);
                if (!g_running || g_launching) {
                    return;
                }
            }
        }
    }

    uint32_t now = lv_tick_get();
    for (unsigned code : kRepeatNavKeys) {
        if (!g_nav_key_down[code]) continue;
        if (static_cast<int32_t>(now - g_nav_key_next_ms[code]) < 0) continue;

        handle_key(code);
        g_nav_key_next_ms[code] += kNavHoldRepeatMs;

        if (!g_running || g_launching) {
            return;
        }
    }
}

}

// Runs the LVGL launcher until a playable core/ROM selection is made.
bool run_app_ui(std::string &core_path, std::string &rom_path)
{
    load_app_config_file(g_app_config);
    apply_app_config_env(g_app_config);

    g_running = true;
    g_launching = false;
    g_selected = false;
    g_selected_core.clear();
    g_selected_rom.clear();
    g_filter.clear();
    g_input_fds.clear();
    g_error_overlay = nullptr;

    lv_init();
    lv_display_t *display = init_ui_display();
    if (!display) {
        return false;
    }

    init_ui_input_devices(g_input_fds);
    bool resumed_to_file_picker = false;
    if (const char *resume_env_core = std::getenv("CP0_UI_START_ENV_CORE")) {
        if (resume_env_core[0]) {
            for (size_t i = 0; i < g_consoles.size(); ++i) {
                if (std::strcmp(g_consoles[i].envCore, resume_env_core) == 0) {
                    if (!is_rom_upload_console(g_consoles[i]) && !is_settings_console(g_consoles[i])) {
                        g_console_index = static_cast<int>(i);
                        g_rom_dir = find_rom_dir_for_console(g_consoles[g_console_index]);
                        g_filter.clear();
                        g_file_index = 0;
                        build_file_picker();
                        resumed_to_file_picker = true;
                    }
                    break;
                }
            }
        }
    }
    unsetenv("CP0_UI_START_ENV_CORE");
    if (!resumed_to_file_picker) {
        build_console_picker();
    }

    uint32_t last_time_update = lv_tick_get();

    while (g_running) {
        poll_input();
        uint32_t now = lv_tick_get();
        if (now - last_time_update > 30000) {
            update_time();
            last_time_update = now;
        }
        lv_timer_handler();
        usleep(1000);
    }


    for (int fd : g_input_fds) close(fd);
    g_input_fds.clear();

    if (g_root) {
        lv_obj_del(g_root);
        g_root = nullptr;
    }
    lv_display_delete(display);
    lv_deinit();

    if (g_selected) {
        core_path = g_selected_core;
        rom_path = g_selected_rom;
    }
    return g_selected;
}
