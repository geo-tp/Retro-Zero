#include "fbdev_video.h"

#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {
uint16_t xrgb8888_to_rgb565(uint32_t p)
{
    uint8_t r = (p >> 16) & 0xff;
    uint8_t g = (p >> 8) & 0xff;
    uint8_t b = p & 0xff;
    return static_cast<uint16_t>(((r & 0xf8) << 8) |
                                 ((g & 0xfc) << 3) |
                                 (b >> 3));
}

uint16_t rgba8888_to_rgb565(uint32_t p)
{
    const uint8_t r = p & 0xff;
    const uint8_t g = (p >> 8) & 0xff;
    const uint8_t b = (p >> 16) & 0xff;
    return static_cast<uint16_t>(((r & 0xf8) << 8) |
                                 ((g & 0xfc) << 3) |
                                 (b >> 3));
}

uint16_t rgb1555_to_rgb565(uint16_t p)
{
    // 0RGB1555: [0][R4..R0][G4..G0][B4..B0]
    uint16_t r5 = (p >> 10) & 0x1f;
    uint16_t g5 = (p >> 5) & 0x1f;
    uint16_t b5 = p & 0x1f;
    // Expand G from 5 to 6 bits for RGB565.
    uint16_t g6 = static_cast<uint16_t>((g5 << 1) | (g5 >> 4));
    return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}
}

// Opens and maps /dev/fb0, then prepares scaling and backlight control.
bool FbdevVideo::init(const char *path)
{
    fd_ = open(path, O_RDWR);
    if (fd_ < 0) {
        perror("open /dev/fb0");
        return false;
    }

    fb_var_screeninfo vinfo {};
    fb_fix_screeninfo finfo {};
    if (ioctl(fd_, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(fd_, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("FBIOGET_*SCREENINFO");
        shutdown();
        return false;
    }

    width_ = vinfo.xres;
    height_ = vinfo.yres;
    bpp_ = vinfo.bits_per_pixel;
    line_length_ = finfo.line_length;
    fb_size_ = static_cast<size_t>(line_length_) * height_;

    std::cout << "fb0: " << width_ << "x" << height_
              << " " << bpp_ << "bpp line_length=" << line_length_ << "\n";

    const char *vsync_env = std::getenv("CP0_FBDEV_VSYNC");
    if (vsync_env && vsync_env[0] == '0') {
        vsync_enabled_ = false;
    }
    std::cout << "fb0 vsync: " << (vsync_enabled_ ? "enabled" : "disabled") << "\n";

    if (bpp_ != 16) {
        std::cerr << "Only 16bpp framebuffers are supported in this prototype\n";
        shutdown();
        return false;
    }

    fb_ = static_cast<uint8_t *>(mmap(nullptr, fb_size_, PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fd_, 0));
    if (fb_ == MAP_FAILED) {
        fb_ = nullptr;
        perror("mmap /dev/fb0");
        shutdown();
        return false;
    }

    clear_bars_next_frame_ = true;
    init_backlight();
    return true;
}

// Unmaps and closes the framebuffer device.
void FbdevVideo::shutdown()
{
    if (fb_) {
        munmap(fb_, fb_size_);
        fb_ = nullptr;
    }
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

void FbdevVideo::set_pixel_format(retro_pixel_format format)
{
    core_format_ = format;
}

void FbdevVideo::wait_for_vsync()
{
    if (!vsync_enabled_ || !vsync_supported_ || fd_ < 0) {
        return;
    }

#ifdef FBIO_WAITFORVSYNC
    int arg = 0;
    if (ioctl(fd_, FBIO_WAITFORVSYNC, &arg) < 0) {
        vsync_supported_ = false;
        if (errno != ENOTTY && errno != EINVAL) {
            std::perror("FBIO_WAITFORVSYNC");
        } else {
            std::cerr << "fb0: FBIO_WAITFORVSYNC not supported by driver\n";
        }
    }
#else
    vsync_supported_ = false;
#endif
}

void FbdevVideo::compute_destination(unsigned &dst_x, unsigned &dst_y,
                                     unsigned &dst_w, unsigned &dst_h)
{
    dst_x = 0;
    dst_y = 0;
    dst_w = width_;
    dst_h = height_;

    if (view_mode_ == ViewMode::Aspect4x3) {
        dst_h = height_;
        dst_w = std::min(width_, (height_ * 4u) / 3u);
        dst_x = (width_ - dst_w) / 2u;
        dst_y = 0;

        if (clear_bars_next_frame_) {
            std::memset(fb_, 0, fb_size_);
            clear_bars_next_frame_ = false;
        }
    } else {
        clear_bars_next_frame_ = true;
    }
}

void FbdevVideo::update_scale_luts(unsigned src_w, unsigned src_h,
                                   unsigned dst_w, unsigned dst_h)
{
    if (src_w == lut_src_w_ && src_h == lut_src_h_ &&
        dst_w == lut_dst_w_ && dst_h == lut_dst_h_) {
        return;
    }

    x_lut_.resize(dst_w);
    y_lut_.resize(dst_h);
    for (unsigned x = 0; x < dst_w; ++x) {
        x_lut_[x] = static_cast<unsigned>((static_cast<uint64_t>(x) * src_w) / dst_w);
    }
    for (unsigned y = 0; y < dst_h; ++y) {
        y_lut_[y] = static_cast<unsigned>((static_cast<uint64_t>(y) * src_h) / dst_h);
    }
    lut_src_w_ = src_w;
    lut_src_h_ = src_h;
    lut_dst_w_ = dst_w;
    lut_dst_h_ = dst_h;
}

// Converts and displays one software-rendered Libretro frame.
void FbdevVideo::render(const void *data, unsigned src_w, unsigned src_h, size_t pitch)
{
    if (!data || !fb_ || src_w == 0 || src_h == 0) {
        return;
    }

    wait_for_vsync();

    unsigned dst_x = 0;
    unsigned dst_y = 0;
    unsigned dst_w = width_;
    unsigned dst_h = height_;
    compute_destination(dst_x, dst_y, dst_w, dst_h);

    if (core_format_ == RETRO_PIXEL_FORMAT_RGB565 &&
        src_w == dst_w && src_h == dst_h && pitch >= dst_w * 2u) {
        for (unsigned y = 0; y < dst_h; ++y) {
            const uint8_t *src_row = static_cast<const uint8_t *>(data) + y * pitch;
            uint8_t *dst_row = fb_ + (y + dst_y) * line_length_ + dst_x * 2u;
            std::memcpy(dst_row, src_row, dst_w * 2u);
        }
        return;
    }

    update_scale_luts(src_w, src_h, dst_w, dst_h);

    for (unsigned y = 0; y < dst_h; ++y) {
        unsigned sy = y_lut_[y];
        uint16_t *dst = reinterpret_cast<uint16_t *>(fb_ + (y + dst_y) * line_length_) + dst_x;

        if (core_format_ == RETRO_PIXEL_FORMAT_RGB565) {
            const uint8_t *row = static_cast<const uint8_t *>(data) + sy * pitch;
            const uint16_t *src = reinterpret_cast<const uint16_t *>(row);
            for (unsigned x = 0; x < dst_w; ++x) {
                dst[x] = src[x_lut_[x]];
            }
        } else if (core_format_ == RETRO_PIXEL_FORMAT_XRGB8888) {
            const uint8_t *row = static_cast<const uint8_t *>(data) + sy * pitch;
            const uint32_t *src = reinterpret_cast<const uint32_t *>(row);
            for (unsigned x = 0; x < dst_w; ++x) {
                dst[x] = xrgb8888_to_rgb565(src[x_lut_[x]]);
            }
        } else if (core_format_ == RETRO_PIXEL_FORMAT_0RGB1555) {
            const uint8_t *row = static_cast<const uint8_t *>(data) + sy * pitch;
            const uint16_t *src = reinterpret_cast<const uint16_t *>(row);
            for (unsigned x = 0; x < dst_w; ++x) {
                dst[x] = rgb1555_to_rgb565(src[x_lut_[x]]);
            }
        }
    }
}

// Displays an RGB565 frame, including GLES2 readback output.
void FbdevVideo::render_rgb565(const uint16_t *data, unsigned src_w, unsigned src_h,
                               size_t pitch_pixels, bool bottom_up)
{
    if (!data || !fb_ || src_w == 0 || src_h == 0) {
        return;
    }

    wait_for_vsync();

    unsigned dst_x = 0;
    unsigned dst_y = 0;
    unsigned dst_w = width_;
    unsigned dst_h = height_;
    compute_destination(dst_x, dst_y, dst_w, dst_h);

    if (!bottom_up && src_w == dst_w && src_h == dst_h && pitch_pixels >= dst_w) {
        for (unsigned y = 0; y < dst_h; ++y) {
            const uint16_t *src_row = data + y * pitch_pixels;
            uint8_t *dst_row = fb_ + (y + dst_y) * line_length_ + dst_x * 2u;
            std::memcpy(dst_row, src_row, dst_w * 2u);
        }
        return;
    }

    update_scale_luts(src_w, src_h, dst_w, dst_h);

    for (unsigned y = 0; y < dst_h; ++y) {
        unsigned sy = y_lut_[y];
        if (bottom_up) sy = src_h - 1u - sy;
        const uint16_t *src = data + sy * pitch_pixels;
        uint16_t *dst = reinterpret_cast<uint16_t *>(fb_ + (y + dst_y) * line_length_) + dst_x;
        for (unsigned x = 0; x < dst_w; ++x) {
            dst[x] = src[x_lut_[x]];
        }
    }
}

void FbdevVideo::render_dc_crop_rgba8888(const uint32_t *data, unsigned src_w, unsigned src_h)
{
    if (!data || !fb_ || src_w != 320 || src_h != 240 || width_ != 320 || height_ != 170) {
        return;
    }

    wait_for_vsync();

    for (unsigned y = 0; y < height_; ++y) {
        const unsigned logical_sy = static_cast<unsigned>((static_cast<uint64_t>(y) * src_h) / height_);
        const unsigned gl_sy = src_h - 1u - logical_sy;
        const uint32_t *src = data + gl_sy * src_w;
        uint16_t *dst = reinterpret_cast<uint16_t *>(fb_ + y * line_length_);
        for (unsigned x = 0; x < width_; ++x) {
            dst[x] = rgba8888_to_rgb565(src[x]);
        }
    }
}

void FbdevVideo::render_test_pattern(const char *label)
{
    if (!fb_ || width_ == 0 || height_ == 0) {
        return;
    }

    static const uint16_t colors[] = {
        0xf800, // red
        0x07e0, // green
        0x001f, // blue
        0xffe0, // yellow
        0x07ff, // cyan
        0xf81f, // magenta
        0xffff, // white
        0x0000, // black
    };
    const unsigned bars = sizeof(colors) / sizeof(colors[0]);
    uint64_t checksum = 0;
    size_t nonzero = 0;

    for (unsigned y = 0; y < height_; ++y) {
        uint16_t *dst = reinterpret_cast<uint16_t *>(fb_ + y * line_length_);
        for (unsigned x = 0; x < width_; ++x) {
            uint16_t px = colors[(static_cast<uint64_t>(x) * bars) / width_];
            if ((y / 12u) & 1u) {
                px ^= 0x39e7;
            }
            dst[x] = px;
            checksum = (checksum * 131u) + px;
            if (px) ++nonzero;
        }
    }

    std::cout << "fb0 diag: CPU test pattern"
              << (label ? " " : "") << (label ? label : "")
              << " " << width_ << "x" << height_
              << " nonzero=" << nonzero
              << " checksum=0x" << std::hex << checksum << std::dec << "\n";
}

void FbdevVideo::toggle_view_mode()
{
    view_mode_ = (view_mode_ == ViewMode::Fullscreen) ? ViewMode::Aspect4x3
                                                      : ViewMode::Fullscreen;
    clear_bars_next_frame_ = true;
    std::cout << "video view mode: "
              << (view_mode_ == ViewMode::Fullscreen ? "fullscreen" : "4:3")
              << "\n";
}

bool FbdevVideo::read_int_file(const std::string &path, int &out) const
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

bool FbdevVideo::write_int_file(const std::string &path, int value) const
{
    FILE *fp = fopen(path.c_str(), "w");
    if (!fp) return false;
    const bool ok = fprintf(fp, "%d\n", value) > 0;
    fclose(fp);
    return ok;
}

bool FbdevVideo::init_backlight()
{
    backlight_brightness_path_.clear();
    backlight_max_ = 0;
    backlight_cur_ = 0;

    const char *forced = std::getenv("CP0_BACKLIGHT_BRIGHTNESS_PATH");
    if (forced && forced[0]) {
        std::string p = forced;
        const std::string dir = p.substr(0, p.find_last_of('/'));
        int maxv = 0;
        int curv = 0;
        if (read_int_file(dir + "/max_brightness", maxv) && read_int_file(p, curv)) {
            backlight_brightness_path_ = p;
            backlight_max_ = maxv;
            backlight_cur_ = curv;
            apply_default_brightness_from_env();
            std::cout << "backlight: using " << backlight_brightness_path_ << "\n";
            return true;
        }
    }

    DIR *dp = opendir("/sys/class/backlight");
    if (!dp) return false;
    dirent *ent = nullptr;
    while ((ent = readdir(dp)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        std::string dir = std::string("/sys/class/backlight/") + ent->d_name;
        std::string bpath = dir + "/brightness";
        int maxv = 0;
        int curv = 0;
        if (read_int_file(dir + "/max_brightness", maxv) && read_int_file(bpath, curv)) {
            backlight_brightness_path_ = bpath;
            backlight_max_ = maxv;
            backlight_cur_ = curv;
            break;
        }
    }
    closedir(dp);

    if (!backlight_brightness_path_.empty()) {
        apply_default_brightness_from_env();
        std::cout << "backlight: using " << backlight_brightness_path_ << "\n";
        return true;
    }
    return false;
}

void FbdevVideo::apply_default_brightness_from_env()
{
    if (backlight_brightness_path_.empty() || backlight_max_ <= 0) return;

    const char *env = std::getenv("CP0_DEFAULT_BRIGHTNESS_PERCENT");
    if (!env || !env[0]) return;

    int pct = std::atoi(env);
    pct = std::clamp(pct, 0, 100);

    const int range = std::max(0, backlight_max_ - backlight_min_);
    const int target = backlight_min_ + (range * pct) / 100;

    if (!read_int_file(backlight_brightness_path_, backlight_cur_)) return;
    if (target == backlight_cur_) return;
    if (write_int_file(backlight_brightness_path_, target)) {
        backlight_cur_ = target;
    }
}

void FbdevVideo::adjust_brightness_steps(int steps)
{
    if (steps == 0 || backlight_brightness_path_.empty() || backlight_max_ <= 0) return;

    if (!read_int_file(backlight_brightness_path_, backlight_cur_)) return;

    const int quantum = std::max(1, backlight_max_ / 20); // ~5% per step
    const int next = std::clamp(backlight_cur_ + steps * quantum, backlight_min_, backlight_max_);
    if (next == backlight_cur_) return;
    if (write_int_file(backlight_brightness_path_, next)) {
        backlight_cur_ = next;
    }
}
