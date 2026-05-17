#ifndef CP0_EGLFBDEV_VIDEO_H
#define CP0_EGLFBDEV_VIDEO_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "libretro.h"

class FbdevVideo;

// GLES2/EGL bridge used by hardware-rendered Libretro cores before fbdev blit.
class EglFbdevVideo {
public:
    // Creates an EGL GLES2 context and explicit FBO sized for the core output.
    bool init(FbdevVideo *fbdev, unsigned width, unsigned height,
              bool depth, bool stencil, bool keep_current = false);

    // Destroys GL/EGL resources.
    void shutdown();

    bool available() const { return initialized_; }
    bool keep_current() const { return keep_current_; }

    // Makes or clears the context current for core rendering/readback.
    bool make_current(const char *site = "unknown");
    void clear_current(const char *site = "clear_current");

    // Libretro hardware-render callbacks.
    uintptr_t current_framebuffer() const;
    retro_proc_address_t get_proc_address(const char *sym) const;

    // Reads back the current hardware frame and presents it through fbdev.
    void present(unsigned width, unsigned height);

private:
    void run_gl_clear_readback_test();
    void ensure_explicit_fbo();
    void destroy_explicit_fbo();

    bool initialized_ = false;
    FbdevVideo *fbdev_ = nullptr;
    unsigned width_ = 0;
    unsigned height_ = 0;
    bool logged_first_present_ = false;
    bool explicit_fbo_ = true;
    bool keep_current_ = false;
    std::vector<uint16_t> readback_rgb565_;
    std::vector<uint32_t> readback_rgba8888_;

#if CP0_WITH_EGL_FBDEV
    void *display_ = nullptr;
    void *context_ = nullptr;
    void *surface_ = nullptr;
    unsigned fbo_ = 0;
    unsigned color_tex_ = 0;
    unsigned color_rb_ = 0;
    unsigned depth_rb_ = 0;
    unsigned stencil_rb_ = 0;
    bool fbo_depth_ = false;
    bool fbo_stencil_ = false;
#endif
};

#endif // CP0_EGLFBDEV_VIDEO_H
