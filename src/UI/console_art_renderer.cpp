#include "console_art_renderer.h"

#include <cstring>

namespace ConsoleArtRenderer {

// ----- Console pixel-art drawing helpers -----

static lv_obj_t *make_rect(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t bg_hex, lv_opa_t opa, int radius) {
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

static lv_obj_t *make_bordered_rect(lv_obj_t *parent, int x, int y, int w, int h,
                                     uint32_t bg_hex, uint32_t border_hex, int radius) {
    lv_obj_t *o = make_rect(parent, x, y, w, h, bg_hex, 255, radius);
    lv_obj_set_style_border_width(o, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(o, lv_color_hex(border_hex), LV_PART_MAIN | LV_STATE_DEFAULT);
    return o;
}

// ---- Game Boy DMG / GBC (44x50, portrait) ----
void draw_gb_art_into(lv_obj_t *art, uint32_t accent) {
    const bool gbc_variant = accent == 0xFFB4B7u;
    const uint32_t body_bg = gbc_variant ? 0xF2D318 : 0xD7D8D1;
    const uint32_t body_border = gbc_variant ? 0xC6AA1A : 0xA8AAA2;
    const uint32_t bezel_bg = gbc_variant ? 0xDDBF24 : 0xB8B9B0;

    lv_obj_t *body = make_rect(art, 0, 0, 44, 50, body_bg, 255, 6);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(body_border), LV_PART_MAIN | LV_STATE_DEFAULT);
    make_rect(art, 7, 4, 30, 23, bezel_bg, 255, 3);
    make_rect(art, 10, 7, 24, 17, 0x8FA85B, 210, 1);
    make_rect(art, 4, 31, 14, 4, 0x2A2A2A, 255, 1);
    make_rect(art, 9, 26, 4, 13, 0x2A2A2A, 255, 1);
    make_rect(art, 31, 30, 6, 6, 0xA0195A, 255, 4);
    make_rect(art, 23, 33, 6, 6, 0xA0195A, 185, 4);
    make_rect(art, 10, 43, 8, 3, 0x72736E, 255, 2);
    make_rect(art, 21, 43, 8, 3, 0x72736E, 255, 2);
    for (int i = 0; i < 4; ++i)
        make_rect(art, 34, 36 + i * 3, 2, 2, 0x777777, 220, 1);
}

// ---- NES (50x44, landscape top-loader style) ----
void draw_nes_art_into(lv_obj_t *art, uint32_t accent) {
    // Simplified NES silhouette: light body, right black band, front panel and buttons
    lv_obj_t *body = make_rect(art, 1, 4, 48, 34, 0xDEDDD9, 255, 1);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0xB7B5AF), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Right dark vertical band
    make_rect(art, 36, 4, 12, 34, 0x1A1A1A, 255, 0);
    // Vent stripes
    for (int y = 8; y <= 28; y += 3)
        make_rect(art, 36, y, 12, 1, 0xD2D0C8, 255, 0);

    // Front flap/panel
    make_rect(art, 2, 24, 34, 12, 0xE7E5DF, 255, 0);
    make_rect(art, 3, 25, 32, 1, 0xC5C3BD, 255, 0);

    // Red logo hint on the front
    make_rect(art, 8, 28, 12, 2, 0xB82020, 255, 1);

    // Power / reset buttons
    make_rect(art, 26, 28, 6, 3, 0x8E8C86, 255, 1);
    make_rect(art, 26, 32, 6, 3, 0x8E8C86, 255, 1);
}

// ---- SNES (50x44, landscape) ----
void draw_snes_art_into(lv_obj_t *art, uint32_t accent) {
    lv_obj_t *body = make_rect(art, 5, 0, 40, 44, 0xA7A294, 255, 4);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x868174), LV_PART_MAIN | LV_STATE_DEFAULT);
    // Softer frame style: light gray, subtle, thin
    make_rect(art, 7, 2, 36, 1, 0xA8A8A8, 255, 0);   // Top
    make_rect(art, 7, 41, 36, 1, 0xA8A8A8, 255, 0);  // Bottom
    make_rect(art, 7, 3, 1, 38, 0xA8A8A8, 255, 0);   // Left
    make_rect(art, 42, 3, 1, 38, 0xA8A8A8, 255, 0);  // Right
    make_rect(art, 7, 2, 28, 10, 0xB2ADA0, 255, 3);
    make_rect(art, 10, 13, 30, 8, 0x4D4A45, 255, 3);
    make_rect(art, 10, 15, 30, 3, 0x6F6E69, 255, 1);
    make_rect(art, 14, 25, 6, 6, 0x4B4A46, 255, 2);
    make_rect(art, 21, 25, 8, 6, 0x5C5A55, 255, 2);
    make_rect(art, 30, 25, 6, 6, 0x4B4A46, 255, 2);
    make_rect(art, 9, 35, 14, 5, 0x938E84, 255, 2);
    make_rect(art, 25, 35, 10, 5, 0x938E84, 255, 2);
    make_rect(art, 31, 5, 3, 3, 0xC7C94A, 255, 2);
    make_rect(art, 34, 5, 3, 3, 0x3F6BC7, 255, 2);
    make_rect(art, 31, 8, 3, 3, 0x57A248, 255, 2);
    make_rect(art, 34, 8, 3, 3, 0xC44C63, 255, 2);
}

// ---- GBA (52x36, landscape wide) ----
void draw_gba_art_into(lv_obj_t *art, uint32_t accent) {
    lv_obj_t *body = make_rect(art, 0, 4, 52, 32, 0x6352B7, 255, 10);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x9183D3), LV_PART_MAIN | LV_STATE_DEFAULT);
    make_rect(art, 11, 7, 30, 18, 0x2B2B33, 255, 4);
    make_rect(art, 14, 10, 24, 12, 0x5A6A85, 175, 1);
    make_rect(art, 3, 22, 12, 4, 0xE8E8EA, 255, 1);
    make_rect(art, 7, 18, 4, 11, 0xE8E8EA, 255, 1);
    make_rect(art, 42, 17, 7, 7, 0xD7D7DE, 255, 4);
    make_rect(art, 34, 22, 6, 6, 0xD7D7DE, 210, 4);
    make_rect(art, 18, 29, 6, 3, 0xCBCAD7, 255, 2);
    make_rect(art, 27, 29, 6, 3, 0xCBCAD7, 255, 2);
}

// ---- Mega Drive (52x36, flat horizontal) ----
void draw_md_art_into(lv_obj_t *art, uint32_t accent) {
    lv_obj_t *body = make_rect(art, 0, 0, 52, 36, 0x262626, 255, 3);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x575757), LV_PART_MAIN | LV_STATE_DEFAULT);
    make_rect(art, 12, 3, 28, 28, 0x1A1A1A, 255, 14);
    make_rect(art, 18, 8, 16, 16, 0x2B2B2B, 255, 8);
    make_rect(art, 17, 1, 18, 8, 0x171717, 255, 1);
    make_rect(art, 22, 12, 8, 3, 0xD4C46B, 255, 1);
    make_rect(art, 4, 6, 4, 4, 0xC22A2A, 255, 3);
    make_rect(art, 7, 28, 12, 4, 0x272727, 255, 2);
    make_rect(art, 33, 28, 12, 4, 0x272727, 255, 2);
    make_rect(art, 42, 7, 5, 3, 0x777777, 255, 2);
}

// ---- Master System (50x40, tall boxy) ----
void draw_sms_art_into(lv_obj_t *art, uint32_t accent) {
    lv_obj_t *body = make_rect(art, 0, 4, 50, 34, 0x262626, 255, 8);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x565656), LV_PART_MAIN | LV_STATE_DEFAULT);
    make_rect(art, 10, 0, 26, 12, 0x1C1C1C, 255, 5);
    make_rect(art, 15, 2, 16, 6, 0x323232, 255, 2);
    make_rect(art, 4, 12, 8, 8, 0x3B3B3B, 255, 4);
    make_rect(art, 4, 14, 4, 4, 0xA02E2E, 255, 3);
    make_rect(art, 20, 15, 10, 3, 0x7E7E7E, 255, 1);
    make_rect(art, 34, 15, 8, 3, 0x7E7E7E, 255, 1);
    make_rect(art, 7, 29, 14, 4, 0x1E1E1E, 255, 2);
    make_rect(art, 29, 29, 14, 4, 0x1E1E1E, 255, 2);
}

// ---- Game Gear (52x38, landscape handheld) ----
void draw_gg_art_into(lv_obj_t *art, uint32_t accent) {
    lv_obj_t *body = make_rect(art, 0, 0, 52, 38, 0x242433, 255, 9);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x5E5E6E), LV_PART_MAIN | LV_STATE_DEFAULT);
    // Screen bezel
    make_rect(art,  9,  5, 30, 22, 0x2D2D44, 255, 4);
    // Screen LCD
    make_rect(art, 12,  8, 24, 16, 0x1A3A1A, 190, 0);
    // Power LED top left
    make_rect(art,  3,  3,  3,  3, accent,   255, 2);
    // Start button
    make_rect(art, 22, 30,  8,  4, 0x69697C, 255, 2);
    // D-pad H
    make_rect(art,  2, 24, 12,  4, 0x4A4A63, 255, 1);
    // D-pad V
    make_rect(art,  6, 20,  4, 12, 0x4A4A63, 255, 1);
    // A button
    make_rect(art, 42, 20,  7,  7, accent,   230, 4);
    // B button
    make_rect(art, 34, 25,  7,  7, accent,   130, 4);
}

// ---- Neo Geo AES (50x40, horizontal home console) ----
void draw_neogeo_art_into(lv_obj_t *art, uint32_t accent) {
    lv_obj_t *body = make_rect(art, 0, 4, 50, 32, 0x242424, 255, 3);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x575757), LV_PART_MAIN | LV_STATE_DEFAULT);
    make_rect(art, 10, 0, 30, 12, 0x1D1D1D, 255, 4);
    make_rect(art, 14, 3, 22, 5, 0x2B2B2B, 255, 1);
    make_rect(art, 4, 7, 5, 5, 0x444444, 255, 3);
    make_rect(art, 5, 8, 3, 3, accent, 255, 2);
    make_rect(art, 39, 7, 6, 6, 0x464646, 255, 3);
    make_rect(art, 8, 27, 14, 4, 0x1D1D1D, 255, 2);
    make_rect(art, 28, 27, 14, 4, 0x1D1D1D, 255, 2);
    make_rect(art, 19, 15, 12, 2, 0xC4B154, 255, 1);
}

// ---- MAME (arcade cabinet, 38x52 portrait) ----
void draw_mame_art_into(lv_obj_t *art, uint32_t accent) {
    // Cabinet top marquee
    make_rect(art,  4,  0, 30,  7, 0x7E4A12, 255, 3);
    // Bezel / monitor housing
    lv_obj_t *body = make_rect(art,  0,  6, 38, 46, 0x242424, 255, 3);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x5C5C5C), LV_PART_MAIN | LV_STATE_DEFAULT);
    // Monitor screen
    make_rect(art,  5, 10, 28, 18, 0x18182A, 255, 2);
    // CRT scanline tint
    make_rect(art,  5, 10, 28, 18, accent,    25, 0);
    // Control panel area
    make_rect(art,  2, 30, 34, 14, 0x2E2E2E, 255, 2);
    // Joystick ball
    make_rect(art,  7, 34,  8,  8, 0x4B4B4B, 255, 5);
    make_rect(art, 10, 36,  4,  4, 0x676767, 255, 3);
    // Buttons row
    for (int i = 0; i < 4; ++i)
        make_rect(art, 20 + i*4, 34, 3, 3, accent, 200 - i*30, 2);
    // Coin slot
    make_rect(art, 16, 44,  6,  3, 0x474747, 255, 1);
}

// ---- Capcom CPS (arcade cabinet, 38x52 portrait, FBNeo CPS palette) ----
void draw_cps_art_into(lv_obj_t *art, uint32_t accent) {
    (void)accent;
    // Cabinet top marquee
    make_rect(art,  4,  0, 30,  7, 0x0E8A63, 255, 3);
    make_rect(art,  8,  2, 22,  3, 0x45E0B0, 210, 2);
    // Bezel / monitor housing
    lv_obj_t *body = make_rect(art,  0,  6, 38, 46, 0x18252B, 255, 3);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x3AAE93), LV_PART_MAIN | LV_STATE_DEFAULT);
    // Monitor screen
    make_rect(art,  5, 10, 28, 18, 0x102034, 255, 2);
    make_rect(art,  5, 10, 28, 18, 0x2DDC84, 35, 0);
    // Control panel area
    make_rect(art,  2, 30, 34, 14, 0x213B45, 255, 2);
    // Joystick ball
    make_rect(art,  7, 34,  8,  8, 0xE8C84F, 255, 5);
    make_rect(art, 10, 36,  4,  4, 0xFFF1A1, 255, 3);
    // CPS button row
    make_rect(art, 20, 34, 3, 3, 0xF35E5E, 255, 2);
    make_rect(art, 24, 34, 3, 3, 0xF2D44E, 255, 2);
    make_rect(art, 28, 34, 3, 3, 0x4FB0FF, 255, 2);
    make_rect(art, 32, 34, 3, 3, 0x54E08C, 255, 2);
    // Coin slot
    make_rect(art, 16, 44,  6,  3, 0x80B7C8, 255, 1);
}

// ---- PlayStation 1 (52x36, flat wide horizontal) ----
void draw_ps1_art_into(lv_obj_t *art, uint32_t accent) {
    (void)accent;
    // Flat light-gray body
    lv_obj_t *body = make_rect(art, 1, 3, 50, 30, 0xD6D4D1, 255, 3);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0xB5B3B0), LV_PART_MAIN | LV_STATE_DEFAULT);
    make_rect(art, 3, 5, 46, 1, 0xECEAE7, 170, 0);
    make_rect(art, 3, 31, 46, 1, 0xA8A6A3, 180, 0);
    make_rect(art, 4, 5, 8, 18, 0xC9C7C4, 120, 1);
    make_rect(art, 40, 5, 8, 18, 0xC9C7C4, 120, 1);

    // Large circular lid (dominant PS1 shape)
    make_rect(art, 10, 4, 32, 26, 0xAAA8A5, 255, 13);
    make_rect(art, 11, 5, 30, 24, 0xC8C6C3, 255, 12);
    make_rect(art, 14, 8, 24, 18, 0xDAD8D5, 255, 9);
    make_rect(art, 21, 12, 10, 2, 0x38383A, 210, 1);

    // Power / open buttons (two small circles)
    make_rect(art, 4, 23, 9, 9, 0x8E8C8A, 255, 5);
    make_rect(art, 5, 24, 7, 7, 0xC8C6C3, 255, 4);
    make_rect(art, 7, 26, 3, 1, 0x43D1C6, 255, 0);
    make_rect(art, 8, 25, 1, 3, 0x43D1C6, 255, 0);
    make_rect(art, 40, 23, 9, 9, 0x8E8C8A, 255, 5);
    make_rect(art, 41, 24, 7, 7, 0xC8C6C3, 255, 4);
    make_rect(art, 43, 26, 3, 1, 0x6AB7E8, 255, 0);
    make_rect(art, 44, 25, 1, 3, 0x6AB7E8, 255, 0);

    // Small side vents.
    for (int y = 8; y <= 18; y += 2) {
        make_rect(art, 48, y, 2, 1, 0x9F9D9A, 220, 0);
    }

    // Tiny PlayStation color mark under the lid center.
    make_rect(art, 25, 18, 2, 5, 0xE13C35, 255, 1);
    make_rect(art, 23, 21, 5, 2, 0xF2D348, 255, 1);
    make_rect(art, 27, 20, 3, 2, 0x2B75D6, 255, 1);
    make_rect(art, 24, 20, 2, 2, 0x40A85C, 255, 1);
}

// ---- Nintendo 64 (54x42, top/front console silhouette) ----
void draw_n64_art_into(lv_obj_t *art, uint32_t accent) {
    const uint32_t shell = 0x303030;
    const uint32_t shell_top = 0x3D3D3D;
    const uint32_t shell_dark = 0x171717;
    const uint32_t recess = 0x1D1D1D;
    const uint32_t port = 0x3A3A3A;
    const uint32_t slot = 0xD4D4D4;

    // Layered rounded blocks approximate the N64's low, flared body.
    lv_obj_t *rear = make_rect(art, 6, 0, 42, 25, shell_top, 255, 5);
    lv_obj_set_style_border_width(rear, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(rear, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
    make_rect(art, 2, 10, 50, 24, shell, 255, 5);
    make_rect(art, 0, 23, 17, 17, shell, 255, 8);
    make_rect(art, 37, 23, 17, 17, shell, 255, 8);
    make_rect(art, 8, 27, 38, 13, shell, 255, 4);
    make_rect(art, 2, 37, 50, 4, shell_dark, 230, 3);
    make_rect(art, 8, 1, 38, 2, 0x5B5B5B, 150, 2);

    // Small top Nintendo badge.
    make_rect(art, 23, 3, 8, 3, 0x0B0B0B, 255, 1);
    make_rect(art, 24, 4, 6, 1, 0x858585, 190, 0);

    // Cartridge slot with the silver lip visible in the reference.
    make_rect(art, 13, 10, 28, 8, recess, 255, 2);
    make_rect(art, 14, 11, 26, 5, slot, 255, 2);
    make_rect(art, 15, 11, 24, 1, 0xEFEFEF, 210, 1);
    make_rect(art, 14, 15, 26, 1, 0x5D5D5D, 240, 1);
    make_rect(art, 13, 17, 28, 2, shell_dark, 220, 2);

    // Fan-shaped vent: stepped gray scoop, then dark vertical slots.
    make_rect(art, 14, 20, 26, 2, 0x555555, 220, 1);
    make_rect(art, 15, 22, 24, 2, 0x4B4B4B, 230, 1);
    make_rect(art, 17, 24, 20, 2, 0x424242, 240, 1);
    make_rect(art, 19, 26, 16, 2, 0x383838, 245, 1);
    for (int i = 0; i < 10; ++i) {
        const int x = 13 + i * 3;
        const int y = (i < 2 || i > 7) ? 21 : 20;
        const int h = (i < 2 || i > 7) ? 5 : (i == 4 || i == 5 ? 9 : 7);
        make_rect(art, x, y, 1, h, shell_dark, 255, 0);
    }

    // Front memory-cover plate and latch.
    make_rect(art, 21, 29, 12, 9, recess, 255, 2);
    make_rect(art, 22, 30, 10, 7, 0x171717, 255, 1);
    make_rect(art, 25, 29, 4, 2, 0x050505, 255, 1);
    make_rect(art, 26, 30, 2, 1, accent, 190, 0);

    // Side front port recesses.
    make_rect(art, 8, 27, 8, 12, recess, 255, 5);
    make_rect(art, 10, 30, 4, 7, port, 255, 3);
    make_rect(art, 11, 31, 2, 1, 0x5D5D5D, 160, 0);
    make_rect(art, 38, 27, 8, 12, recess, 255, 5);
    make_rect(art, 40, 30, 4, 7, port, 255, 3);
    make_rect(art, 41, 31, 2, 1, 0x5D5D5D, 160, 0);
}

// ---- Dreamcast (50x44, top lid inspired by the white console) ----
void draw_dc_art_into(lv_obj_t *art, uint32_t accent) {
    (void)accent;
    const uint32_t shell = 0xDCE5E8;
    const uint32_t shell_shadow = 0xB7C4C9;
    const uint32_t shell_dark = 0x5E6A70;
    const uint32_t lid = 0xE9F0F2;
    const uint32_t lid_shadow = 0xC8D2D6;
    const uint32_t blue = 0x2491D8;

    lv_obj_t *body = make_rect(art, 1, 1, 48, 40, shell, 255, 4);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0xEFF7F8), LV_PART_MAIN | LV_STATE_DEFAULT);
    make_rect(art, 3, 3, 44, 1, 0xF7FBFC, 210, 1);
    make_rect(art, 2, 38, 46, 2, shell_shadow, 220, 2);

    // Diagonal corner seams from the original top shell.
    for (int i = 0; i < 13; ++i) {
        make_rect(art, 5 + i, 4 + i, 1, 1, shell_dark, 170, 0);
        make_rect(art, 44 - i, 4 + i, 1, 1, shell_dark, 170, 0);
    }

    // Large circular lid, built from rounded bands at this tiny scale.
    make_rect(art, 8, 7, 34, 31, lid_shadow, 255, 16);
    make_rect(art, 9, 8, 32, 29, lid, 255, 15);
    make_rect(art, 12, 10, 26, 24, 0xF4F8F9, 150, 12);

    // Front triangular open latch.
    make_rect(art, 21, 34, 8, 5, 0xABB8BE, 255, 2);
    make_rect(art, 22, 38, 6, 3, 0x3B454B, 255, 1);
    make_rect(art, 24, 40, 2, 2, 0x141A1E, 255, 0);

    // Power/open buttons.
    make_rect(art, 5, 31, 10, 10, 0x9FAAAF, 255, 5);
    make_rect(art, 6, 32, 8, 8, 0xD7E0E3, 255, 4);
    make_rect(art, 8, 35, 4, 1, 0x8B989E, 255, 0);
    make_rect(art, 35, 31, 10, 10, 0x9FAAAF, 255, 5);
    make_rect(art, 36, 32, 8, 8, 0xD7E0E3, 255, 4);
    make_rect(art, 38, 35, 4, 1, 0x8B989E, 255, 0);

    // Side screw/vent dots.
    for (int y = 14; y <= 29; y += 5) {
        make_rect(art, 3, y, 2, 2, 0xC2CDD1, 255, 1);
        make_rect(art, 45, y, 2, 2, 0xC2CDD1, 255, 1);
    }

    // Tiny Dreamcast swirl logo and wordmark hint.
    make_rect(art, 23, 13, 5, 5, blue, 255, 3);
    make_rect(art, 24, 14, 3, 3, lid, 255, 2);
    make_rect(art, 25, 15, 2, 2, blue, 255, 1);
    make_rect(art, 20, 20, 10, 2, 0xA5B0B5, 255, 1);
    make_rect(art, 22, 22, 7, 1, 0x7F8C92, 220, 0);
}

// ---- Naomi / Atomiswave (46x54, candy arcade cabinet from the reference photo) ----
void draw_naomi_aw_art_into(lv_obj_t *art, uint32_t accent) {
    (void)accent;
    const uint32_t shell = 0xE7E1C7;
    const uint32_t shell_hi = 0xFFF8DB;
    const uint32_t shell_shadow = 0xBEB79C;
    const uint32_t red = 0xB71418;
    const uint32_t bezel = 0x303236;
    const uint32_t screen = 0x5C6A65;
    const uint32_t panel = 0x111316;
    const uint32_t pink = 0xFF5178;
    const uint32_t green = 0x00CA63;
    const uint32_t yellow = 0xFFD44B;

    // Upright cabinet body with the tall, pale Naomi-style face.
    lv_obj_t *cab = make_rect(art, 6, 0, 34, 47, shell, 255, 3);
    lv_obj_set_style_border_width(cab, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(cab, lv_color_hex(shell_shadow), LV_PART_MAIN | LV_STATE_DEFAULT);
    make_rect(art, 8, 2, 30, 2, shell_hi, 210, 1);
    make_rect(art, 8, 4, 30, 3, red, 255, 1);
    make_rect(art, 9, 4, 28, 1, 0xF15D43, 180, 0);

    // CRT bezel and glass, with a small reflection streak.
    make_rect(art, 9, 10, 28, 22, bezel, 255, 2);
    make_rect(art, 12, 13, 22, 15, screen, 255, 1);
    make_rect(art, 13, 14, 20, 13, 0x4E5A56, 210, 1);
    make_rect(art, 22, 11, 2, 9, 0xFFFFFF, 70, 1);
    make_rect(art, 23, 13, 1, 7, 0xFFFFFF, 90, 0);

    // Speaker grilles and coin/service plate below the screen.
    for (int x = 10; x <= 17; x += 2) {
        make_rect(art, x, 34, 1, 2, 0x6F6A5A, 220, 0);
    }
    for (int x = 29; x <= 36; x += 2) {
        make_rect(art, x, 34, 1, 2, 0x6F6A5A, 220, 0);
    }
    make_rect(art, 20, 33, 7, 4, 0x161719, 255, 1);

    // Wide control panel: black deck, two player sticks, pink/green buttons, yellow start.
    make_rect(art, 2, 36, 42, 9, shell_hi, 255, 3);
    make_rect(art, 4, 37, 38, 6, panel, 255, 2);
    make_rect(art, 8, 35, 2, 5, 0x6C6D68, 255, 1);
    make_rect(art, 32, 35, 2, 5, 0x6C6D68, 255, 1);
    make_rect(art, 7, 35, 5, 5, pink, 255, 3);
    make_rect(art, 31, 35, 5, 5, green, 255, 3);
    for (int i = 0; i < 6; ++i) {
        make_rect(art, 15 + (i % 3) * 3, 38 + (i / 3) * 2, 2, 2, pink, 255, 1);
        make_rect(art, 34 + (i % 3) * 3, 38 + (i / 3) * 2, 2, 2, green, 255, 1);
    }
    make_rect(art, 24, 39, 2, 2, yellow, 255, 1);
    make_rect(art, 27, 39, 2, 2, yellow, 255, 1);

    // Lower cabinet doors and small caution sticker hint.
    make_rect(art, 8, 45, 30, 8, 0xD8D0B4, 255, 2);
    make_rect(art, 10, 46, 12, 6, 0xCBC2A7, 255, 1);
    make_rect(art, 24, 46, 11, 6, 0xEFEAD8, 255, 1);
    make_rect(art, 19, 47, 4, 4, yellow, 255, 1);
    make_rect(art, 20, 48, 2, 2, 0x222222, 220, 0);
    make_rect(art, 31, 48, 1, 2, 0x444444, 255, 0);
}

// ---- PC Engine (44x44, small square) ----
void draw_pce_art_into(lv_obj_t *art, uint32_t accent) {
    const uint32_t shell = 0xEEECE4;
    const uint32_t shadow = 0xD4D1C8;
    const uint32_t groove = 0xB9B6AE;
    const uint32_t logo_red = 0xC93632;
    const uint32_t port_dark = 0x1F2022;

    lv_obj_t *body = make_rect(art, 1, 3, 42, 36, shell, 255, 5);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0xBEBBB2), LV_PART_MAIN | LV_STATE_DEFAULT);

    make_rect(art, 4, 5, 36, 1, 0xFAF9F4, 210, 0);
    make_rect(art, 4, 37, 36, 1, shadow, 255, 0);

    // Central HuCard bay and raised front flap.
    make_rect(art, 14, 13, 16, 15, 0xD9D6CD, 255, 1);
    make_rect(art, 16, 14, 12, 10, 0xC8C5BC, 255, 1);
    for (int x = 17; x <= 27; x += 2) {
        make_rect(art, x, 15, 1, 9, 0xA9A69E, 210, 0);
    }
    make_rect(art, 15, 24, 14, 3, 0xF5F2E9, 255, 2);
    make_rect(art, 17, 25, 10, 8, 0xE3E0D7, 255, 5);
    make_rect(art, 18, 25, 8, 1, 0xFAF8F1, 220, 0);

    // Vent grooves on both sides, like the reference shell.
    for (int y = 20; y <= 27; y += 2) {
        make_rect(art, 4, y, 7, 1, groove, 255, 0);
        make_rect(art, 33, y, 7, 1, groove, 255, 0);
    }
    make_rect(art, 5, 12, 1, 7, groove, 180, 0);
    make_rect(art, 38, 12, 1, 7, groove, 180, 0);

    // Tiny red PC Engine mark.
    make_rect(art, 18, 8, 8, 2, logo_red, 255, 1);
    make_rect(art, 20, 10, 10, 2, logo_red, 210, 1);
    make_rect(art, 25, 8, 4, 1, accent, 210, 0);

    // Front switches and round controller port.
    make_rect(art, 3, 34, 5, 3, 0x18A171, 255, 1);
    make_rect(art, 8, 34, 4, 3, 0xD8D5CB, 255, 1);
    make_rect(art, 33, 34, 8, 3, port_dark, 255, 2);
    make_rect(art, 35, 35, 4, 1, 0xD8D5CB, 255, 0);
}

// ---- Atari 2600 (52x38, ribbed front console) ----
void draw_a2600_art_into(lv_obj_t *art, uint32_t accent) {
    (void)accent;
    lv_obj_t *body = make_rect(art, 0, 3, 52, 34, 0x151719, 255, 3);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x4E555B), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Top control strip, slightly blue like the metal label plate.
    make_rect(art, 3, 7, 46, 9, 0x26334A, 255, 1);
    make_rect(art, 4, 8, 44, 1, 0x6F87A8, 190, 0);
    make_rect(art, 19, 10, 14, 4, 0x111315, 255, 1);

    // Tiny switch groups: two left, two right.
    const int switch_x[] = {7, 14, 36, 43};
    for (int x : switch_x) {
        make_rect(art, x, 10, 2, 4, 0xC7CCD2, 255, 1);
        make_rect(art, x, 12, 2, 1, 0x0E1012, 180, 0);
    }

    // Ribbed front grille.
    for (int y = 20; y <= 31; y += 3) {
        make_rect(art, 4, y, 44, 1, 0x34383C, 255, 0);
        make_rect(art, 4, y + 1, 44, 1, 0x0C0D0E, 180, 0);
    }

    // Woodgrain lower lip.
    make_rect(art, 2, 33, 48, 3, 0xA35F25, 255, 1);
    make_rect(art, 5, 34, 42, 1, 0xD18A3A, 180, 0);
}

// ---- Atari 7800 (52x36, low console base) ----
void draw_a7800_art_into(lv_obj_t *art, uint32_t accent) {
    (void)accent;
    lv_obj_t *body = make_rect(art, 0, 6, 52, 27, 0x15191D, 255, 3);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x3F4B55), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Rear vent notches.
    for (int x = 8; x <= 42; x += 4) {
        make_rect(art, x, 6, 2, 3, 0x2C343B, 255, 1);
    }

    // Wide white Atari 7800 label band.
    make_rect(art, 2, 15, 48, 9, 0xE9E7E2, 255, 1);
    make_rect(art, 4, 20, 9, 2, 0xE4812A, 255, 1);
    make_rect(art, 13, 20, 9, 2, 0xD8D84B, 255, 1);
    make_rect(art, 22, 20, 9, 2, 0x59C96E, 255, 1);
    make_rect(art, 31, 20, 9, 2, 0x33A7D8, 255, 1);
    make_rect(art, 40, 20, 8, 2, 0x2E68C9, 255, 1);
    make_rect(art, 34, 16, 12, 3, 0x2E68C9, 230, 1);

    // Front control strip.
    make_rect(art, 1, 28, 50, 2, 0x070809, 255, 0);
    make_rect(art, 13, 28, 7, 1, 0x8D969E, 200, 0);
    make_rect(art, 25, 28, 7, 1, 0x8D969E, 200, 0);
    make_rect(art, 37, 28, 7, 1, 0x8D969E, 200, 0);
}

// ---- Atari Lynx (52x40, landscape) ----
void draw_lynx_art_into(lv_obj_t *art, uint32_t accent) {
    lv_obj_t *body = make_rect(art, 0, 0, 52, 40, 0x2F3337, 255, 7);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x676B70), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Central darker deck where the screen sits.
    make_rect(art, 10, 2, 32, 36, 0x1F2227, 255, 2);
    // Small orange logo hint like the real LYNX mark.
    make_rect(art, 20, 3, 12, 2, 0xD8891D, 255, 1);

    // Screen frame + LCD
    make_rect(art, 14, 7, 24, 18, 0x5B6168, 255, 3);
    make_rect(art, 15, 8, 22, 16, 0x2B3138, 200, 2);
    make_rect(art, 17, 10, 18, 12, 0x0A0F14, 255, 1);

    // Left D-pad
    make_rect(art,  2, 18, 12, 4, 0x1F2125, 255, 1);
    make_rect(art,  6, 14,  4, 12, 0x1F2125, 255, 1);

    // Side option switches around the display.
    make_rect(art,  8, 14, 2, 4, 0x4A4E54, 255, 1);
    make_rect(art,  8, 20, 2, 4, 0x4A4E54, 255, 1);
    make_rect(art,  8, 26, 2, 4, 0x4A4E54, 255, 1);
    make_rect(art, 39, 14, 2, 4, 0x4A4E54, 255, 1);
    make_rect(art, 39, 20, 2, 4, 0x4A4E54, 255, 1);
    make_rect(art, 39, 26, 2, 4, 0x4A4E54, 255, 1);

    // Right speaker grille.
    make_rect(art, 43, 16, 1, 10, 0x1A1C1F, 255, 0);
    make_rect(art, 45, 16, 1, 10, 0x1A1C1F, 255, 0);
    make_rect(art, 47, 16, 1, 10, 0x1A1C1F, 255, 0);

    // Right action buttons (diamond-like placement)
    make_rect(art, 41,  7, 6, 6, 0x2A2D31, 255, 3);
    make_rect(art, 46,  9, 6, 6, 0x2A2D31, 255, 3);
    make_rect(art, 41, 28, 6, 6, 0x2A2D31, 255, 3);
    make_rect(art, 46, 30, 6, 6, 0x2A2D31, 255, 3);
    make_rect(art, 42,  8, 2, 2, accent, 120, 1);

    // Center lower control key.
    make_rect(art, 23, 31, 6, 3, 0x4A4E54, 255, 2);
}

// ---- Neo Geo Pocket (38x50, portrait compact) ----
void draw_ngp_art_into(lv_obj_t *art, uint32_t accent) {
    // Main grey body (inspired by GBA style)
    lv_obj_t *body = make_rect(art, 0, 4, 38, 42, 0x6B6B6B, 255, 6);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(accent), LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Main screen area (portrait centered)
    make_rect(art, 6, 8, 26, 20, 0x2B2B33, 255, 3);
    make_rect(art, 8, 10, 22, 16, 0x5A7A8F, 175, 1);
    
    // NGP mini-stick (round cap + highlight)
    make_rect(art, 4, 28, 10, 10, 0xBFC6D0, 255, 5);
    make_rect(art, 6, 30, 6, 6, 0xE3E7ED, 255, 3);
    make_rect(art, 8, 32, 2, 2, 0xA2A9B3, 255, 1);
    
    // Buttons (right side - A/B stacked, using accent color)
    make_rect(art, 28, 30, 6, 6, accent, 230, 3);
    make_rect(art, 28, 38, 6, 6, accent, 130, 3);
    
    // Bottom controls (similar to GBA)
    make_rect(art, 12, 44, 5, 2, 0xD8D8E0, 255, 1);
    make_rect(art, 21, 44, 5, 2, 0xD8D8E0, 255, 1);
}

// ---- WonderSwan (34x52, tall portrait) ----
void draw_ws_art_into(lv_obj_t *art, uint32_t accent) {
    lv_obj_t *body = make_rect(art, 0, 0, 34, 52, 0x25252A, 255, 6);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x5E5E67), LV_PART_MAIN | LV_STATE_DEFAULT);
    // Screen bezel
    make_rect(art,  5,  8, 24, 20, 0x2E2E46, 255, 3);
    // Screen LCD
    make_rect(art,  8, 11, 18, 14, 0x2A2A45, 190, 0);
    // Left d-pad cluster (X1-X4 horizontal)
    make_rect(art,  3, 36, 12,  4, 0x4A4A65, 255, 1);
    make_rect(art,  7, 32,  4, 12, 0x4A4A65, 255, 1);
    // Right d-pad cluster (Y1-Y4 horizontal)
    make_rect(art, 19, 36, 12,  4, 0x4A4A65, 255, 1);
    make_rect(art, 23, 32,  4, 12, 0x4A4A65, 255, 1);
    // Start
    make_rect(art, 14, 45,  6,  3, 0x6A6A82, 255, 2);
    // Power LED
    make_rect(art, 28,  6,  3,  3, accent,   255, 2);
}

// ---- ROM upload (46x46, cloud + arrow) ----
void draw_rom_upload_art_into(lv_obj_t *art, uint32_t accent) {
    const uint32_t border = 0x84BCE3;
    const uint32_t arrow_outline = 0x24435D;

    lv_obj_t *base = make_rect(art, 1, 21, 44, 20, accent, 255, 9);
    lv_obj_set_style_border_width(base, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(base, lv_color_hex(border), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *left = make_rect(art, 0, 13, 20, 21, accent, 255, 10);
    lv_obj_set_style_border_width(left, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(left, lv_color_hex(border), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mid = make_rect(art, 10, 4, 26, 27, accent, 255, 13);
    lv_obj_set_style_border_width(mid, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(mid, lv_color_hex(border), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *right = make_rect(art, 25, 12, 20, 21, accent, 255, 10);
    lv_obj_set_style_border_width(right, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(right, lv_color_hex(border), LV_PART_MAIN | LV_STATE_DEFAULT);

    make_rect(art, 18, 12, 10, 15, arrow_outline, 255, 3);
    make_rect(art, 12, 24, 22, 6, arrow_outline, 255, 3);
    make_rect(art, 14, 28, 18, 4, arrow_outline, 255, 2);
    make_rect(art, 16, 31, 14, 3, arrow_outline, 255, 2);
    make_rect(art, 18, 33, 10, 3, arrow_outline, 255, 1);
    make_rect(art, 20, 35, 6, 2, arrow_outline, 255, 1);
    make_rect(art, 22, 37, 2, 1, arrow_outline, 255, 1);

    make_rect(art, 20, 13, 6, 13, 0xFEFFFF, 255, 2);
    make_rect(art, 15, 25, 16, 4, 0xFEFFFF, 255, 2);
    make_rect(art, 17, 28, 12, 3, 0xFEFFFF, 255, 2);
    make_rect(art, 19, 31, 8, 2, 0xFEFFFF, 255, 1);
    make_rect(art, 21, 33, 4, 2, 0xFEFFFF, 255, 1);
}

// ---- MSX (44x44, Daewoo/Perfect style keyboard) ----
void draw_msx_art_into(lv_obj_t *art, uint32_t accent) {
    (void)accent;
    const uint32_t shell = 0x68695F;
    const uint32_t shell_dark = 0x3D443F;
    const uint32_t groove = 0x171918;
    const uint32_t key = 0xECE8DD;
    const uint32_t key_shadow = 0xC9C4BA;
    const uint32_t space_gold = 0xBDA061;
    const uint32_t pad_green = 0x556747;
    const uint32_t arrow_gold = 0xD8C577;

    lv_obj_t *body = make_rect(art, 0, 5, 44, 34, shell, 255, 3);
    lv_obj_set_style_border_width(body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(body, lv_color_hex(0x31332F), LV_PART_MAIN | LV_STATE_DEFAULT);

    make_rect(art, 1, 6, 42, 3, 0x7B7C73, 255, 1);
    make_rect(art, 1, 18, 42, 19, shell_dark, 255, 1);
    make_rect(art, 3, 20, 28, 15, 0x28302E, 255, 1);

    // Left top vent from the reference machine.
    make_rect(art, 2, 7, 17, 11, 0xF0EEE9, 255, 1);
    for (int y = 9; y <= 15; y += 2) {
        make_rect(art, 3, y, 16, 1, 0x252525, 255, 0);
    }
    make_rect(art, 2, 6, 11, 2, 0xF9F8F4, 255, 1);

    // Cartridge/display bay with the small 64K badge.
    make_rect(art, 24, 8, 17, 7, 0x54554E, 255, 1);
    make_rect(art, 27, 9, 11, 4, groove, 255, 1);
    make_rect(art, 33, 10, 4, 2, 0xD1AE38, 255, 1);

    make_rect(art, 3, 19, 25, 1, 0x5B615D, 255, 0);
    make_rect(art, 5, 21, 21, 12, 0x151816, 255, 1);

    // Keyboard block: cream keys and the distinctive gold space bar.
    for (int row = 0; row < 3; ++row) {
        const int y = 22 + row * 3;
        const int start_x = 6 + (row == 1 ? 1 : 0);
        const int count = row == 0 ? 8 : 7;
        for (int i = 0; i < count; ++i) {
            make_rect(art, start_x + i * 3, y, 2, 2, key_shadow, 255, 0);
            make_rect(art, start_x + i * 3, y, 2, 1, key, 255, 0);
        }
    }
    make_rect(art, 7, 31, 3, 2, key, 255, 0);
    make_rect(art, 11, 31, 12, 2, space_gold, 255, 0);
    make_rect(art, 24, 31, 3, 2, key, 255, 0);

    make_rect(art, 4, 17, 7, 2, 0xE8E4DA, 255, 1);
    make_rect(art, 12, 17, 10, 2, 0xE8E4DA, 255, 1);
    make_rect(art, 28, 17, 6, 2, 0xE8E4DA, 255, 1);
    make_rect(art, 36, 17, 5, 2, 0xE8E4DA, 255, 1);

    // Right-side olive direction pad, close to the attached photo.
    make_rect(art, 31, 22, 10, 10, 0x263025, 255, 2);
    make_rect(art, 34, 23, 4, 4, pad_green, 255, 1);
    make_rect(art, 34, 28, 4, 4, pad_green, 255, 1);
    make_rect(art, 30, 26, 4, 4, pad_green, 255, 1);
    make_rect(art, 38, 26, 4, 4, pad_green, 255, 1);
    make_rect(art, 35, 24, 2, 1, arrow_gold, 255, 0);
    make_rect(art, 35, 30, 2, 1, arrow_gold, 255, 0);
    make_rect(art, 32, 27, 1, 2, arrow_gold, 255, 0);
    make_rect(art, 40, 27, 1, 2, arrow_gold, 255, 0);

    make_rect(art, 34, 35, 4, 2, 0xD1AE38, 255, 0);
    make_rect(art, 38, 35, 3, 1, 0xDADAD2, 255, 0);
}

// ---- Settings (46x46, simple gear) ----
void draw_settings_art_into(lv_obj_t *art, uint32_t accent) {
    const uint32_t hole = 0x0F1724;
    const uint32_t rim = 0x1D2D43;
    const uint32_t ring_outer = 0x243750;

    // Gear teeth
    lv_obj_t *t_top = make_rect(art, 19,  0,  8, 10, accent, 255, 3);
    lv_obj_t *t_bottom = make_rect(art, 19, 36,  8, 10, accent, 255, 3);
    lv_obj_t *t_left = make_rect(art,  0, 18,  8, 10, accent, 255, 3);
    lv_obj_t *t_right = make_rect(art, 38, 18,  8, 10, accent, 255, 3);
    lv_obj_t *t_tl = make_rect(art,  6,  5,  8, 10, accent, 255, 3);
    lv_obj_t *t_tr = make_rect(art, 32,  5,  8, 10, accent, 255, 3);
    lv_obj_t *t_bl = make_rect(art,  6, 31,  8, 10, accent, 255, 3);
    lv_obj_t *t_br = make_rect(art, 32, 31,  8, 10, accent, 255, 3);

    // Gear body and center hole
    lv_obj_t *body = make_rect(art, 5, 5, 36, 36, accent, 255, 14);
    make_rect(art, 11, 11, 24, 24, ring_outer, 255, 12);
    make_rect(art, 13, 13, 20, 20, accent, 255, 10);
    make_rect(art, 15, 15, 16, 16, hole, 255, 8);

    lv_obj_t *parts[] = {t_top, t_bottom, t_left, t_right, t_tl, t_tr, t_bl, t_br, body};
    for (lv_obj_t *part : parts) {
        lv_obj_set_style_border_width(part, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(part, lv_color_hex(rim), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

// ---- dispatch table entry ----
struct ArtSpec {
    const char *env_core;
    int w, h;
    int y_off;     // LV_ALIGN_CENTER y offset for art
    int name_y;    // card.name y override (more space below art)
    void (*draw)(lv_obj_t*, uint32_t);
};

static const ArtSpec kArtSpecs[] = {
        // ...existing code...
    {"CP0_CORE_GB",     44, 50, -7, 32, draw_gb_art_into},
    {"CP0_CORE_GBC",    44, 50, -7, 32, draw_gb_art_into},
    {"CP0_CORE_NES",    50, 44, -5, 30, draw_nes_art_into},
    {"CP0_CORE_SNES",   50, 44, -5, 30, draw_snes_art_into},
    {"CP0_CORE_GBA",    52, 36, -4, 28, draw_gba_art_into},
    {"CP0_CORE_MD",     52, 36, -4, 28, draw_md_art_into},
    {"CP0_CORE_SMS",    50, 40, -5, 29, draw_sms_art_into},
    {"CP0_CORE_GG",     52, 38, -4, 28, draw_gg_art_into},
    {"CP0_CORE_NEOGEO", 50, 40, -5, 29, draw_neogeo_art_into},
    {"CP0_CORE_FBN",    38, 52, -7, 33, draw_cps_art_into},
    {"CP0_CORE_MAME",   38, 52, -7, 33, draw_mame_art_into},
    {"CP0_CORE_PS1",    52, 36, -4, 28, draw_ps1_art_into},
    {"CP0_CORE_N64",    54, 42, -5, 30, draw_n64_art_into},
    {"CP0_CORE_DC",     50, 44, -5, 30, draw_dc_art_into},
    {"CP0_CORE_NAOMI_ATOMISWAVE", 46, 54, -8, 34, draw_naomi_aw_art_into},
    {"CP0_CORE_PCE",    44, 44, -5, 30, draw_pce_art_into},
    {"CP0_CORE_MSX",    44, 44, -5, 30, draw_msx_art_into},
    {"CP0_CORE_A2600",  52, 38, -5, 29, draw_a2600_art_into},
    {"CP0_CORE_A7800",  52, 36, -4, 28, draw_a7800_art_into},
    {"CP0_CORE_LYNX",   52, 40, -5, 29, draw_lynx_art_into},
    {"CP0_CORE_NGP",    38, 50, -6, 32, draw_ngp_art_into},
    {"CP0_CORE_WS",     34, 52, -7, 33, draw_ws_art_into},
    {"CP0_TOOL_SETTINGS", 46, 46, -5, 31, draw_settings_art_into},
    {"CP0_TOOL_ROM_UPLOAD", 46, 46, -5, 31, draw_rom_upload_art_into},
};

// Returns true when art was drawn (caller should hide card.code).
// Repaints one console-art card according to the selected console profile.
bool rebuild(lv_obj_t *art, lv_obj_t *logo_badge,
             lv_obj_t *name_lbl, const CoreConfig &console, int slot) {
    lv_obj_clean(art);
    lv_obj_set_size(art, 0, 0);
    lv_obj_set_style_bg_opa(art, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(art, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Draw art on all slots so incoming cards already have their icon during animation.

    const ArtSpec *spec = nullptr;
    for (const ArtSpec &s : kArtSpecs) {
        if (std::strcmp(console.envCore, s.env_core) == 0) { spec = &s; break; }
    }
    if (!spec) return false;

    lv_obj_set_size(art, spec->w, spec->h);
    lv_obj_align(art, LV_ALIGN_CENTER, 0, spec->y_off);
    spec->draw(art, console.accent);

    lv_obj_set_y(name_lbl, spec->name_y);   // push title down for breathing room
    lv_obj_move_foreground(logo_badge);
    lv_obj_move_foreground(name_lbl);
    return true;
}


} // namespace ConsoleArtRenderer
