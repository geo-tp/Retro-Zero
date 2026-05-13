#include "eglfbdev_video.h"

#include "fbdev_video.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iomanip>
#include <iostream>

#if CP0_WITH_EGL_FBDEV
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif
#endif

namespace {
uint16_t rgba8888_to_rgb565(uint32_t p)
{
    const uint8_t r = p & 0xff;
    const uint8_t g = (p >> 8) & 0xff;
    const uint8_t b = (p >> 16) & 0xff;
    return static_cast<uint16_t>(((r & 0xf8) << 8) |
                                 ((g & 0xfc) << 3) |
                                 (b >> 3));
}

bool env_enabled(const char *name, bool fallback)
{
    const char *value = std::getenv(name);
    if (!value || !value[0]) return fallback;
    return value[0] != '0';
}

unsigned env_u32(const char *name, unsigned fallback)
{
    const char *value = std::getenv(name);
    if (!value || !value[0]) return fallback;

    char *end = nullptr;
    unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value || parsed == 0 || parsed > 4096) return fallback;

    return static_cast<unsigned>(parsed);
}

#if CP0_WITH_EGL_FBDEV
const char *egl_error_name(EGLint error)
{
    switch (error) {
    case EGL_SUCCESS: return "EGL_SUCCESS";
    case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
    case EGL_BAD_ACCESS: return "EGL_BAD_ACCESS";
    case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE: return "EGL_BAD_ATTRIBUTE";
    case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
    case EGL_BAD_CONTEXT: return "EGL_BAD_CONTEXT";
    case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
    case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
    case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
    case EGL_BAD_PARAMETER: return "EGL_BAD_PARAMETER";
    case EGL_BAD_SURFACE: return "EGL_BAD_SURFACE";
    case EGL_CONTEXT_LOST: return "EGL_CONTEXT_LOST";
    default: return "EGL_UNKNOWN";
    }
}

void log_egl_error(const char *what)
{
    const EGLint error = eglGetError();
    std::cerr << "egl: " << what << " failed: " << egl_error_name(error)
              << " (0x" << std::hex << error << std::dec << ")\n";
}

EGLDisplay get_surfaceless_display()
{
    using GetPlatformDisplay = EGLDisplay (*)(EGLenum, void *, const EGLint *);

    auto get_platform_display = reinterpret_cast<GetPlatformDisplay>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));

    if (!get_platform_display) {
        get_platform_display = reinterpret_cast<GetPlatformDisplay>(
            eglGetProcAddress("eglGetPlatformDisplay"));
    }

    if (!get_platform_display) {
        return EGL_NO_DISPLAY;
    }

    return get_platform_display(EGL_PLATFORM_SURFACELESS_MESA,
                                EGL_DEFAULT_DISPLAY,
                                nullptr);
}

bool initialize_display(EGLDisplay display, const char *label)
{
    if (display == EGL_NO_DISPLAY) {
        return false;
    }

    if (eglInitialize(display, nullptr, nullptr)) {
        std::cout << "egl: using " << label << " display\n";
        return true;
    }

    log_egl_error(label);
    return false;
}
#endif
}

// Creates a surfaceless/pbuffer GLES2 context used by hardware-rendered cores.
bool EglFbdevVideo::init(FbdevVideo *fbdev,
                         unsigned width,
                         unsigned height,
                         bool depth,
                         bool stencil)
{
    shutdown();

    fbdev_ = fbdev;
    width_ = env_u32("CP0_EGL_FBDEV_WIDTH", width ? width : 320);
    height_ = env_u32("CP0_EGL_FBDEV_HEIGHT", height ? height : 240);
    logged_first_present_ = false;
    explicit_fbo_ = env_enabled("CP0_EGL_EXPLICIT_FBO", true);

    readback_rgb565_.resize(static_cast<size_t>(width_) * height_);
    readback_rgba8888_.resize(static_cast<size_t>(width_) * height_);

#if CP0_WITH_EGL_FBDEV
    EGLDisplay display = get_surfaceless_display();

    if (!initialize_display(display, "surfaceless")) {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

        if (!initialize_display(display, "default")) {
            std::cerr << "egl: no usable display. On Pi/KMS, install Mesa EGL/GLES "
                         "and ensure the vc4/v3d DRM driver is enabled.\n";
            return false;
        }
    }

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        log_egl_error("eglBindAPI GLES");
        eglTerminate(display);
        return false;
    }

    const EGLint attrs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 6,
        EGL_BLUE_SIZE, 5,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, depth ? 16 : 0,
        EGL_STENCIL_SIZE, stencil ? 8 : 0,
        EGL_NONE
    };

    EGLConfig config = nullptr;
    EGLint configs = 0;

    if (!eglChooseConfig(display, attrs, &config, 1, &configs) || configs == 0) {
        log_egl_error("eglChooseConfig GLES2 pbuffer");
        eglTerminate(display);
        return false;
    }

    const EGLint pbuffer_attrs[] = {
        EGL_WIDTH, static_cast<EGLint>(width_),
        EGL_HEIGHT, static_cast<EGLint>(height_),
        EGL_NONE
    };

    EGLSurface surface = eglCreatePbufferSurface(display, config, pbuffer_attrs);

    if (surface == EGL_NO_SURFACE) {
        log_egl_error("eglCreatePbufferSurface");
        eglTerminate(display);
        return false;
    }

    const EGLint context_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrs);

    if (context == EGL_NO_CONTEXT) {
        log_egl_error("eglCreateContext GLES2");
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }

    display_ = display;
    surface_ = surface;
    context_ = context;
    fbo_depth_ = depth;
    fbo_stencil_ = stencil;

    if (!make_current()) {
        shutdown();
        return false;
    }

    ensure_explicit_fbo();

    glViewport(0,
               0,
               static_cast<GLsizei>(width_),
               static_cast<GLsizei>(height_));

    initialized_ = true;

    std::cout << "egl fbdev: GLES2 pbuffer " << width_ << "x" << height_ << "\n";

    return true;
#else
    (void)depth;
    (void)stencil;

    std::cerr << "egl fbdev: disabled at build time (CP0_WITH_EGL_FBDEV=0)\n";

    return false;
#endif
}

// Releases EGL and GL objects owned by the hardware-render bridge.
void EglFbdevVideo::shutdown()
{
#if CP0_WITH_EGL_FBDEV
    EGLDisplay display = static_cast<EGLDisplay>(display_);
    EGLContext context = static_cast<EGLContext>(context_);
    EGLSurface surface = static_cast<EGLSurface>(surface_);

    if (display != EGL_NO_DISPLAY && display) {
        if (context != EGL_NO_CONTEXT && context) {
            eglMakeCurrent(display, surface, surface, context);
            destroy_explicit_fbo();
        }

        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (context != EGL_NO_CONTEXT && context) {
            eglDestroyContext(display, context);
        }

        if (surface != EGL_NO_SURFACE && surface) {
            eglDestroySurface(display, surface);
        }

        eglTerminate(display);
    }

    display_ = nullptr;
    context_ = nullptr;
    surface_ = nullptr;
#endif

    initialized_ = false;
    fbdev_ = nullptr;
    logged_first_present_ = false;
    explicit_fbo_ = true;

    readback_rgb565_.clear();
    readback_rgba8888_.clear();
}

bool EglFbdevVideo::make_current()
{
#if CP0_WITH_EGL_FBDEV
    if (!display_ || !surface_ || !context_) {
        return false;
    }

    return eglMakeCurrent(static_cast<EGLDisplay>(display_),
                          static_cast<EGLSurface>(surface_),
                          static_cast<EGLSurface>(surface_),
                          static_cast<EGLContext>(context_)) == EGL_TRUE;
#else
    return false;
#endif
}

void EglFbdevVideo::clear_current()
{
#if CP0_WITH_EGL_FBDEV
    if (!display_) {
        return;
    }

    eglMakeCurrent(static_cast<EGLDisplay>(display_),
                   EGL_NO_SURFACE,
                   EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
#endif
}

uintptr_t EglFbdevVideo::current_framebuffer() const
{
#if CP0_WITH_EGL_FBDEV
    if (fbo_ != 0) {
        return static_cast<uintptr_t>(fbo_);
    }
#endif

    return 0;
}

retro_proc_address_t EglFbdevVideo::get_proc_address(const char *sym) const
{
#if CP0_WITH_EGL_FBDEV
    retro_proc_address_t proc =
        reinterpret_cast<retro_proc_address_t>(eglGetProcAddress(sym));

    if (!proc) {
        proc = reinterpret_cast<retro_proc_address_t>(dlsym(RTLD_DEFAULT, sym));

        static unsigned fallback_logs_left = 40;

        if (proc && fallback_logs_left > 0) {
            --fallback_logs_left;
            std::cout << "egl fbdev diag: get_proc_address fallback dlsym("
                      << sym << ")\n";
        }
    }

    return proc;
#else
    (void)sym;
    return nullptr;
#endif
}

void EglFbdevVideo::ensure_explicit_fbo()
{
#if CP0_WITH_EGL_FBDEV
    if (fbo_ != 0) {
        return;
    }

    auto cleanup = [](GLuint fbo, GLuint color_tex, GLuint color_rb, GLuint depth_rb) {
        if (depth_rb) {
            glDeleteRenderbuffers(1, &depth_rb);
        }

        if (color_rb) {
            glDeleteRenderbuffers(1, &color_rb);
        }

        if (fbo) {
            glDeleteFramebuffers(1, &fbo);
        }

        if (color_tex) {
            glDeleteTextures(1, &color_tex);
        }
    };

    auto attach_depth = [this](GLuint *depth_rb) {
        glGenRenderbuffers(1, depth_rb);
        glBindRenderbuffer(GL_RENDERBUFFER, *depth_rb);
        glRenderbufferStorage(GL_RENDERBUFFER,
                              GL_DEPTH_COMPONENT16,
                              static_cast<GLsizei>(width_),
                              static_cast<GLsizei>(height_));
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                  GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER,
                                  *depth_rb);
    };

    while (glGetError() != GL_NO_ERROR) {
    }

    GLuint rgba_fbo = 0;
    GLuint rgba_tex = 0;
    GLuint rgba_depth = 0;

    glGenTextures(1, &rgba_tex);
    glBindTexture(GL_TEXTURE_2D, rgba_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 static_cast<GLsizei>(width_),
                 static_cast<GLsizei>(height_),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 nullptr);

    glGenFramebuffers(1, &rgba_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rgba_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           rgba_tex,
                           0);

    attach_depth(&rgba_depth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GLenum error = glGetError();

    std::cout << "egl fbdev: explicit FBO RGBA texture status=0x"
              << std::hex << status << " glError=0x" << error << std::dec << "\n";

    if (status == GL_FRAMEBUFFER_COMPLETE && error == GL_NO_ERROR) {
        fbo_ = rgba_fbo;
        color_tex_ = rgba_tex;
        color_rb_ = 0;
        depth_rb_ = rgba_depth;
        stencil_rb_ = 0;

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0,
                   0,
                   static_cast<GLsizei>(width_),
                   static_cast<GLsizei>(height_));

        std::cout << "egl fbdev: explicit FBO enabled id=" << fbo_
                  << " RGBA texture + DEPTH_COMPONENT16 "
                  << width_ << "x" << height_ << "\n";

        return;
    }

    cleanup(rgba_fbo, rgba_tex, 0, rgba_depth);

    while (glGetError() != GL_NO_ERROR) {
    }

    GLuint rgb565_fbo = 0;
    GLuint rgb565_color = 0;
    GLuint rgb565_depth = 0;

    glGenFramebuffers(1, &rgb565_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rgb565_fbo);

    glGenRenderbuffers(1, &rgb565_color);
    glBindRenderbuffer(GL_RENDERBUFFER, rgb565_color);
    glRenderbufferStorage(GL_RENDERBUFFER,
                          GL_RGB565,
                          static_cast<GLsizei>(width_),
                          static_cast<GLsizei>(height_));

    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER,
                              rgb565_color);

    attach_depth(&rgb565_depth);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    error = glGetError();

    std::cout << "egl fbdev: explicit FBO RGB565 renderbuffer status=0x"
              << std::hex << status << " glError=0x" << error << std::dec << "\n";

    if (status == GL_FRAMEBUFFER_COMPLETE && error == GL_NO_ERROR) {
        fbo_ = rgb565_fbo;
        color_tex_ = 0;
        color_rb_ = rgb565_color;
        depth_rb_ = rgb565_depth;
        stencil_rb_ = 0;

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0,
                   0,
                   static_cast<GLsizei>(width_),
                   static_cast<GLsizei>(height_));

        std::cout << "egl fbdev: explicit FBO enabled id=" << fbo_
                  << " RGB565 renderbuffer + DEPTH_COMPONENT16 "
                  << width_ << "x" << height_ << "\n";

        return;
    }

    cleanup(rgb565_fbo, 0, rgb565_color, rgb565_depth);

    std::cerr << "egl fbdev: explicit FBO unavailable; GL_FRAMEBUFFER_COMPLETE is 0x"
              << std::hex << GL_FRAMEBUFFER_COMPLETE << std::dec << "\n";

    explicit_fbo_ = false;
    fbo_ = 0;
    color_tex_ = 0;
    color_rb_ = 0;
    depth_rb_ = 0;
    stencil_rb_ = 0;
#endif
}

void EglFbdevVideo::destroy_explicit_fbo()
{
#if CP0_WITH_EGL_FBDEV
    GLuint fbo = static_cast<GLuint>(fbo_);
    GLuint color_tex = static_cast<GLuint>(color_tex_);
    GLuint color_rb = static_cast<GLuint>(color_rb_);
    GLuint depth_rb = static_cast<GLuint>(depth_rb_);
    GLuint stencil_rb = static_cast<GLuint>(stencil_rb_);

    if (stencil_rb) {
        glDeleteRenderbuffers(1, &stencil_rb);
    }

    if (depth_rb) {
        glDeleteRenderbuffers(1, &depth_rb);
    }

    if (color_rb) {
        glDeleteRenderbuffers(1, &color_rb);
    }

    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
    }

    if (color_tex) {
        glDeleteTextures(1, &color_tex);
    }

    fbo_ = 0;
    color_tex_ = 0;
    color_rb_ = 0;
    depth_rb_ = 0;
    stencil_rb_ = 0;
#endif
}

void EglFbdevVideo::run_gl_clear_readback_test()
{
#if CP0_WITH_EGL_FBDEV
    if (!fbdev_ || !make_current() || readback_rgba8888_.empty()) {
        return;
    }

    if (fbo_ != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fbo_));
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glViewport(0,
               0,
               static_cast<GLsizei>(width_),
               static_cast<GLsizei>(height_));

    glDisable(GL_SCISSOR_TEST);
    glClearColor(1.0f, 0.0f, 0.35f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0,
                 0,
                 static_cast<GLsizei>(width_),
                 static_cast<GLsizei>(height_),
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 readback_rgba8888_.data());

    const GLenum error = glGetError();

    const size_t pixels = static_cast<size_t>(width_) * height_;
    uint64_t checksum = 0;
    size_t nonzero = 0;

    for (size_t i = 0; i < pixels; ++i) {
        const uint32_t px = readback_rgba8888_[i];
        checksum = (checksum * 131u) + px;

        if (px) {
            ++nonzero;
        }

        readback_rgb565_[i] = rgba8888_to_rgb565(px);
    }

    std::cout << "egl fbdev diag: GL clearColor readback RGBA8 "
              << width_ << "x" << height_
              << " nonzero=" << nonzero
              << " checksum=0x" << std::hex << checksum
              << " glError=0x" << error << std::dec << "\n";

    fbdev_->render_rgb565(readback_rgb565_.data(),
                          width_,
                          height_,
                          width_,
                          true);
#endif
}

// Reads the GLES2 framebuffer and forwards it to the fbdev backend.
void EglFbdevVideo::present(unsigned width, unsigned height)
{
#if CP0_WITH_EGL_FBDEV
    if (!initialized_ || !fbdev_ || !make_current()) {
        return;
    }

    if (fbo_ != 0) {
        width = width_;
        height = height_;
    } else {
        width = std::min(width ? width : width_, width_);
        height = std::min(height ? height : height_, height_);
    }

    if (width == 0 || height == 0) {
        return;
    }

    if (!logged_first_present_) {
        std::cout << "egl fbdev: first HW frame " << width << "x" << height << "\n";
        logged_first_present_ = true;
    }

    if (fbo_ != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fbo_));
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glGetError();

    glReadPixels(0,
                 0,
                 static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height),
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 readback_rgba8888_.data());

    const GLenum error = glGetError();

    if (error != GL_NO_ERROR) {
        std::cerr << "egl fbdev: glReadPixels failed 0x"
                  << std::hex << error << std::dec << "\n";
        return;
    }

    const size_t pixels = static_cast<size_t>(width) * height;

    // Fast fullscreen path for 320x240 hardware-rendered cores on 320x170 panel.
    // Important:
    // - In fullscreen mode, keep the old crop/stretch path for max performance.
    // - In 4:3 mode, do NOT use this path, because it bypasses FbdevVideo::compute_destination().
    //   We must fall through to render_rgb565(), which applies the current view mode.
    if (!fbdev_->aspect_4x3_enabled() &&
        width == 320 &&
        height == 240 &&
        fbdev_->width() == 320 &&
        fbdev_->height() == 170) {
        fbdev_->render_dc_crop_rgba8888(readback_rgba8888_.data(),
                                        width,
                                        height);
        return;
    }

    for (size_t i = 0; i < pixels; ++i) {
        const uint32_t px = readback_rgba8888_[i];
        readback_rgb565_[i] = rgba8888_to_rgb565(px);
    }

    fbdev_->render_rgb565(readback_rgb565_.data(),
                          width,
                          height,
                          width,
                          true);
#else
    (void)width;
    (void)height;
#endif
}