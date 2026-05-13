#ifndef CP0_FBDEV_VIDEO_H
#define CP0_FBDEV_VIDEO_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "libretro.h"

// Direct /dev/fb0 video backend for software frames and GLES2 readback frames.
class FbdevVideo {
public:
    enum class ViewMode {
        Fullscreen,
        Aspect4x3,
    };

    // Opens the framebuffer, maps it, and initializes scaling/backlight state.
    bool init(const char *path = "/dev/fb0");

    // Unmaps and closes the framebuffer.
    void shutdown();

    // Sets the pixel format used by software-rendered Libretro cores.
    void set_pixel_format(retro_pixel_format format);

    // Presents one software-rendered Libretro frame.
    void render(const void *data, unsigned width, unsigned height, size_t pitch);

    // Presents an RGB565 frame, optionally flipped vertically.
    void render_rgb565(const uint16_t *data, unsigned width, unsigned height,
                       size_t pitch_pixels, bool bottom_up);

    // Presents Dreamcast RGBA8888 output with the current crop/conversion path.
    void render_dc_crop_rgba8888(const uint32_t *data, unsigned width, unsigned height);

    // Draws a diagnostic pattern directly to the framebuffer.
    void render_test_pattern(const char *label);

    // Adjusts LCD backlight by relative steps.
    void adjust_brightness_steps(int steps);

    // Toggles fullscreen versus 4:3 aspect-correct scaling.
    void toggle_view_mode();

    bool aspect_4x3_enabled() const { return view_mode_ == ViewMode::Aspect4x3; }
    unsigned width() const { return width_; }
    unsigned height() const { return height_; }

private:
    void wait_for_vsync();
    bool init_backlight();
    void apply_default_brightness_from_env();
    void compute_destination(unsigned &dst_x, unsigned &dst_y,
                             unsigned &dst_w, unsigned &dst_h);
    void update_scale_luts(unsigned src_w, unsigned src_h,
                           unsigned dst_w, unsigned dst_h);
    bool read_int_file(const std::string &path, int &out) const;
    bool write_int_file(const std::string &path, int value) const;

    int fd_ = -1;
    uint8_t *fb_ = nullptr;
    size_t fb_size_ = 0;
    unsigned width_ = 0;
    unsigned height_ = 0;
    unsigned bpp_ = 0;
    unsigned line_length_ = 0;
    retro_pixel_format core_format_ = RETRO_PIXEL_FORMAT_0RGB1555;
    bool vsync_enabled_ = true;
    bool vsync_supported_ = true;
    ViewMode view_mode_ = ViewMode::Fullscreen;
    bool clear_bars_next_frame_ = true;
    unsigned lut_src_w_ = 0;
    unsigned lut_src_h_ = 0;
    unsigned lut_dst_w_ = 0;
    unsigned lut_dst_h_ = 0;
    std::vector<unsigned> x_lut_;
    std::vector<unsigned> y_lut_;
    std::string backlight_brightness_path_;
    int backlight_min_ = 1;
    int backlight_max_ = 0;
    int backlight_cur_ = 0;
};

#endif // CP0_FBDEV_VIDEO_H
