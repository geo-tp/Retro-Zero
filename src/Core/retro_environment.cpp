#include "retro_frontend.h"

#include "core_registry.h"
#include "libretro_core.h"
#include "../Utils/bios_utils.h"
#include "../Utils/env_utils.h"
#include "../Utils/file_utils.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

void core_log(retro_log_level level, const char *fmt, ...)
{
    const char *prefix = "core";
    switch (level) {
    case RETRO_LOG_DEBUG: return;
    // case RETRO_LOG_DEBUG: prefix = "core debug"; break;
    case RETRO_LOG_INFO: prefix = "core info"; break;
    case RETRO_LOG_WARN: prefix = "core warn"; break;
    case RETRO_LOG_ERROR: prefix = "core error"; break;
    default: break;
    }

    std::fprintf(stderr, "%s: ", prefix);
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
}

retro_time_t RETRO_CALLCONV perf_get_time_usec_cb()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<retro_time_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

retro_perf_tick_t RETRO_CALLCONV perf_get_counter_cb()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<retro_perf_tick_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

uint64_t RETRO_CALLCONV perf_get_cpu_features_cb()
{
#if defined(__aarch64__)
    return RETRO_SIMD_NEON | RETRO_SIMD_ASIMD;
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    return RETRO_SIMD_NEON;
#else
    return 0;
#endif
}

void RETRO_CALLCONV perf_register_cb(retro_perf_counter *counter)
{
    if (!counter) return;
    counter->registered = true;
}

void RETRO_CALLCONV perf_start_cb(retro_perf_counter *counter)
{
    if (!counter) return;
    counter->start = perf_get_counter_cb();
    ++counter->call_cnt;
}

void RETRO_CALLCONV perf_stop_cb(retro_perf_counter *counter)
{
    if (!counter) return;
    const retro_perf_tick_t now = perf_get_counter_cb();
    if (now >= counter->start) {
        counter->total += now - counter->start;
    }
}

void RETRO_CALLCONV perf_log_cb()
{
}

bool RETRO_CALLCONV set_rumble_state_cb(unsigned, retro_rumble_effect, uint16_t)
{
    return false;
}

// Initializes callback structures returned through Libretro environment queries.
void init_frontend_interfaces()
{
    retro_frontend_context().perf_callback.get_time_usec = perf_get_time_usec_cb;
    retro_frontend_context().perf_callback.get_cpu_features = perf_get_cpu_features_cb;
    retro_frontend_context().perf_callback.get_perf_counter = perf_get_counter_cb;
    retro_frontend_context().perf_callback.perf_register = perf_register_cb;
    retro_frontend_context().perf_callback.perf_start = perf_start_cb;
    retro_frontend_context().perf_callback.perf_stop = perf_stop_cb;
    retro_frontend_context().perf_callback.perf_log = perf_log_cb;
    retro_frontend_context().rumble_interface.set_rumble_state = set_rumble_state_cb;
}

bool drm_node_available()
{
    return path_exists("/dev/dri/card0") || path_exists("/dev/dri/card1");
}

unsigned default_hw_width()
{
    return 320;
}

unsigned default_hw_height()
{
    return 240;
}

uintptr_t RETRO_CALLCONV get_current_framebuffer_cb()
{
    auto &frontend = retro_frontend_context();

    // PPSSPP's GLRenderManager may ask this from a worker/render thread.
    // In keep-current mode, the EGL context belongs to the video callback
    // thread, so this callback must only return the cached FBO name.
    if (frontend.egl_video.keep_current()) {
        const uintptr_t fb = frontend.egl_video.current_framebuffer();

        if (env_enabled_value(std::getenv("CP0_EGL_DEBUG"))) {
            std::fprintf(stderr,
                         "hw render: get_current_framebuffer keep-current returning fb=%lu without eglMakeCurrent\n",
                         static_cast<unsigned long>(fb));
        }

        return fb;
    }

    if (!frontend.egl_video.make_current("get-current-framebuffer")) {
        return 0;
    }

    return frontend.egl_video.current_framebuffer();
}

retro_proc_address_t RETRO_CALLCONV get_proc_address_cb(const char *sym)
{
    return retro_frontend_context().egl_video.get_proc_address(sym);
}

const char *hw_context_name(retro_hw_context_type type)
{
    switch (type) {
    case RETRO_HW_CONTEXT_NONE: return "RETRO_HW_CONTEXT_NONE";
    case RETRO_HW_CONTEXT_OPENGL: return "RETRO_HW_CONTEXT_OPENGL";
    case RETRO_HW_CONTEXT_OPENGLES2: return "RETRO_HW_CONTEXT_OPENGLES2";
    case RETRO_HW_CONTEXT_OPENGL_CORE: return "RETRO_HW_CONTEXT_OPENGL_CORE";
    case RETRO_HW_CONTEXT_OPENGLES3: return "RETRO_HW_CONTEXT_OPENGLES3";
    case RETRO_HW_CONTEXT_OPENGLES_VERSION: return "RETRO_HW_CONTEXT_OPENGLES_VERSION";
    case RETRO_HW_CONTEXT_VULKAN: return "RETRO_HW_CONTEXT_VULKAN";
    case RETRO_HW_CONTEXT_D3D11: return "RETRO_HW_CONTEXT_D3D11";
    case RETRO_HW_CONTEXT_D3D10: return "RETRO_HW_CONTEXT_D3D10";
    case RETRO_HW_CONTEXT_D3D12: return "RETRO_HW_CONTEXT_D3D12";
    case RETRO_HW_CONTEXT_D3D9: return "RETRO_HW_CONTEXT_D3D9";
    default: return "RETRO_HW_CONTEXT_UNKNOWN";
    }
}

// Accepts GLES2 hardware-render requests and creates the EGL framebuffer bridge.
bool init_hw_render_context(retro_hw_render_callback *requested)
{
    if (!requested) return false;

    if (requested->context_type != RETRO_HW_CONTEXT_OPENGLES2) {
        std::cerr << "hw render probe: core requested "
                  << hw_context_name(requested->context_type)
                  << " (" << requested->context_type
                  << "); GLES2 supported: no\n";
        return false;
    }

    std::cout << "hw render probe: core requested RETRO_HW_CONTEXT_OPENGLES2; "
                 "GLES2 supported: yes\n";

    if (drm_node_available() && !std::getenv("CP0_FORCE_EGL_FBDEV")) {
        std::cout << "hw render: DRM/KMS node detected; KMS backend is not "
                     "wired yet, using EGL fbdev fallback\n";
    }

    unsigned width = env_u32_or("CP0_HW_RENDER_WIDTH", default_hw_width());
    unsigned height = env_u32_or("CP0_HW_RENDER_HEIGHT", default_hw_height());
    const bool is_psp =
        CoreRegistry::isPspCore(retro_frontend_context().active_env_core.c_str());
    if (CoreRegistry::isN64Core(retro_frontend_context().active_env_core.c_str())) {
        width = env_u32_or("CP0_N64_INTERNAL_WIDTH", width);
        height = env_u32_or("CP0_N64_INTERNAL_HEIGHT", height);
    } else if (is_flycast_env_core(retro_frontend_context().active_env_core.c_str())) {
        width = env_u32_or("CP0_DC_INTERNAL_WIDTH", 320);
        height = env_u32_or("CP0_DC_INTERNAL_HEIGHT", 240);
    } else if (is_psp) {
        width = env_u32_or("CP0_PSP_INTERNAL_WIDTH", 480);
        height = env_u32_or("CP0_PSP_INTERNAL_HEIGHT", 272);
    }
    const bool keep_egl_current =
        is_psp && env_enabled_value(std::getenv("CP0_PSP_KEEP_EGL_CURRENT"));
    if (keep_egl_current) {
        std::cout << "hw render PSP: CP0_PSP_KEEP_EGL_CURRENT=1, no EGL_NO_CONTEXT ping-pong during runtime\n";
    }
    if (!retro_frontend_context().egl_video.init(&retro_frontend_context().video,
                                                width,
                                                height,
                                                requested->depth,
                                                requested->stencil,
                                                keep_egl_current)) {
        return false;
    }

    requested->get_current_framebuffer = get_current_framebuffer_cb;
    requested->get_proc_address = get_proc_address_cb;
    retro_frontend_context().hw_render = *requested;
    retro_frontend_context().hw_render_requested = true;
    retro_frontend_context().hw_render_ready = false;
    retro_frontend_context().hw_context_reset_done = false;
    retro_frontend_context().hw_frame_width = width;
    retro_frontend_context().hw_frame_height = height;

    std::cout << "hw render: GLES2 context created "
              << width << "x" << height
              << " depth=" << (requested->depth ? "yes" : "no")
              << " stencil=" << (requested->stencil ? "yes" : "no")
              << ", reset deferred\n";
    return true;
}

// Runs the deferred context_reset callback once geometry and video are ready.
void reset_hw_render_context_if_needed()
{
    if (!retro_frontend_context().hw_render_requested || retro_frontend_context().hw_context_reset_done) {
        return;
    }

    if (!retro_frontend_context().egl_video.make_current("context-reset")) {
        std::cerr << "hw render: failed to make EGL context current for reset\n";
        return;
    }

    if (retro_frontend_context().hw_render.context_reset) {
        retro_frontend_context().hw_render.context_reset();
    }

    retro_frontend_context().hw_context_reset_done = true;
    retro_frontend_context().hw_render_ready = true;

    if (env_enabled_value(std::getenv("CP0_HW_CONTEXT_CORE_THREAD")) &&
        !retro_frontend_context().egl_video.keep_current()) {
        retro_frontend_context().egl_video.clear_current("context-reset-release-core-thread");
        std::cout << "hw render: context reset done; released for core thread\n";
    } else if (env_enabled_value(std::getenv("CP0_HW_CONTEXT_CORE_THREAD"))) {
        std::cout << "hw render: context reset done; kept current due to CP0_PSP_KEEP_EGL_CURRENT\n";
    } else {
        std::cout << "hw render: context reset done; kept on main thread\n";
    }
}

// Main Libretro environment callback for options, paths, logging, timing, and HW render.
bool environment_cb(unsigned cmd, void *data)
{
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        auto *requested = static_cast<retro_hw_render_callback *>(data);
        return init_hw_render_context(requested);
    }
    case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION: {
        auto *version = static_cast<unsigned *>(data);
        if (version) *version = 0;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
        return true;
#ifdef RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
        return true;
#endif
    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER: {
        auto *preferred = static_cast<unsigned *>(data);
        if (preferred) {
            if (CoreRegistry::isPspCore(retro_frontend_context().active_env_core.c_str())) {
                // PPSSPP's GLES2 libretro path treats DUMMY as "pick the GL path",
                // then requests RETRO_HW_CONTEXT_OPENGLES2 in SET_HW_RENDER.
                *preferred = RETRO_HW_CONTEXT_DUMMY;
                std::cout << "hw render preferred: PSP/PPSSPP -> DUMMY probe for GLES2 path\n";
            } else {
                *preferred = RETRO_HW_CONTEXT_OPENGLES2;
            }
        }
        return true;
    }
    case RETRO_ENVIRONMENT_SET_ROTATION: {
        auto *rotation = static_cast<unsigned *>(data);
        retro_frontend_context().video_rotation = rotation ? (*rotation & 3u) : 0u;
        if (retro_frontend_context().video_rotation) {
            std::cout << "video rotation: " << retro_frontend_context().video_rotation << "\n";
        }
        return retro_frontend_context().video_rotation == 0;
    }
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
        // Performance hint accepted; frontend has no specific tuning path here.
        return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
        auto *can_dupe = static_cast<bool *>(data);
        if (can_dupe) *can_dupe = true;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
        auto requested = *static_cast<retro_pixel_format *>(data);
        if (requested == RETRO_PIXEL_FORMAT_RGB565 ||
            requested == RETRO_PIXEL_FORMAT_0RGB1555 ||
            requested == RETRO_PIXEL_FORMAT_XRGB8888) {
            retro_frontend_context().video.set_pixel_format(requested);
            std::cout << "core pixel format: "
                      << (requested == RETRO_PIXEL_FORMAT_RGB565 ? "RGB565" :
                          requested == RETRO_PIXEL_FORMAT_0RGB1555 ? "0RGB1555" :
                          "XRGB8888")
                      << "\n";
            return true;
        }
        std::cerr << "core requested unsupported pixel format " << requested << "\n";
        return false;
    }
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
        auto *log = static_cast<retro_log_callback *>(data);
        log->log = core_log;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_PERF_INTERFACE: {
        auto *perf = static_cast<retro_perf_callback *>(data);
        if (!perf) return false;
        *perf = retro_frontend_context().perf_callback;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
        auto *rumble = static_cast<retro_rumble_interface *>(data);
        if (!rumble) return false;
        *rumble = retro_frontend_context().rumble_interface;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
        auto *out = static_cast<const char **>(data);
        *out = retro_frontend_context().system_dir.empty() ? nullptr : retro_frontend_context().system_dir.c_str();
        std::cout << "environment: GET_SYSTEM_DIRECTORY -> "
                  << (*out ? *out : "<null>") << "\n";
        return true;
    }
    case RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY: {
        auto *out = static_cast<const char **>(data);
        *out = retro_frontend_context().content_dir.empty() ? nullptr : retro_frontend_context().content_dir.c_str();
        return true;
    }
    case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE: {
        auto *hz = static_cast<float *>(data);
        if (hz) *hz = retro_frontend_context().target_refresh_hz;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_FASTFORWARDING: {
        auto *fastforwarding = static_cast<bool *>(data);
        if (fastforwarding) *fastforwarding = false;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_LANGUAGE: {
        auto *language = static_cast<unsigned *>(data);
        if (language) *language = RETRO_LANGUAGE_ENGLISH;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES: {
        auto *caps = static_cast<uint64_t *>(data);
        if (caps) {
            *caps = (1ULL << RETRO_DEVICE_JOYPAD) |
                    (1ULL << RETRO_DEVICE_ANALOG);
        }
        return true;
    }
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        if (data) {
            auto *ports = static_cast<const retro_controller_info *>(data);
            std::cout << "input: SET_CONTROLLER_INFO\n";
            for (unsigned port = 0; port < 8 && ports[port].types && ports[port].num_types; ++port) {
                std::cout << "input: port " << port << " controller types:";
                for (unsigned i = 0; i < ports[port].num_types; ++i) {
                    const auto &type = ports[port].types[i];
                    std::cout << " [" << type.id << " "
                              << (type.desc ? type.desc : "<null>") << "]";
                }
                std::cout << "\n";
            }
        }
        return true;
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
        auto *out = static_cast<const char **>(data);
        if (!out) return false;
        if (retro_frontend_context().active_env_core == "CP0_CORE_DC") {
            static const char *dc_save_dir = "/home/pi/retrozero/saves/dc";
            ensure_dir_exists(dc_save_dir);
            *out = dc_save_dir;
        } else {
            *out = retro_frontend_context().save_dir.empty() ? (retro_frontend_context().content_dir.empty()
                                             ? (retro_frontend_context().system_dir.empty() ? nullptr : retro_frontend_context().system_dir.c_str())
                                             : retro_frontend_context().content_dir.c_str())
                                     : retro_frontend_context().save_dir.c_str();
        }
        std::cout << "environment: GET_SAVE_DIRECTORY -> "
                  << (*out ? *out : "<null>") << "\n";
        return true;
    }
#ifdef RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY
#if !defined(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY) || \
    (RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY != RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY)
    case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY: {
        auto *out = static_cast<const char **>(data);
        if (!out) return false;
        *out = retro_frontend_context().system_dir.empty() ? nullptr : retro_frontend_context().system_dir.c_str();
        return true;
    }
#endif
#endif
#ifdef RETRO_ENVIRONMENT_GET_VARIABLE
    case RETRO_ENVIRONMENT_GET_VARIABLE:
        return get_core_option_override(static_cast<retro_variable *>(data));
#endif
#ifdef RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
        auto *updated = static_cast<bool *>(data);
        if (updated) *updated = false;
        return true;
    }
#endif
#ifndef RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE
    // Older/newer headers may vary; command 17 is GET_VARIABLE_UPDATE.
    case 17: {
        auto *updated = static_cast<bool *>(data);
        if (updated) *updated = false;
        return true;
    }
#endif
#ifdef RETRO_ENVIRONMENT_SET_VARIABLES
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        mark_core_options_dirty();
        return true;
#endif
#ifdef RETRO_ENVIRONMENT_SET_CORE_OPTIONS
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
        mark_core_options_dirty();
        return true;
#endif
#ifdef RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
        mark_core_options_dirty();
        return true;
#endif
#ifdef RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
        mark_core_options_dirty();
        return true;
#endif
#ifdef RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
        mark_core_options_dirty();
        return true;
#endif
#ifdef RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: {
        auto *version = static_cast<unsigned *>(data);
        if (version) *version = 2;
        return true;
    }
#endif
#ifndef RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION
    // Compatibility fallback when macro is absent in headers.
    case 52: {
        auto *version = static_cast<unsigned *>(data);
        if (version) *version = 2;
        return true;
    }
#endif
#ifdef RETRO_ENVIRONMENT_GET_INPUT_BITMASKS
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: {
        auto *supports = static_cast<bool *>(data);
        if (supports) *supports = true;
        return true;
    }
#endif
#ifdef RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
        // Frontend requests full AV generation for normal gameplay.
        auto *flags = static_cast<unsigned *>(data);
        if (flags) *flags = RETRO_AV_ENABLE_VIDEO | RETRO_AV_ENABLE_AUDIO;
        return true;
    }
#endif
#if !defined(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE) || (RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE != 65583)
    case 65583:
#endif
    {
        // 47 | RETRO_ENVIRONMENT_EXPERIMENTAL from modern cores.
        auto *flags = static_cast<unsigned *>(data);
        if (flags) *flags = RETRO_AV_ENABLE_VIDEO | RETRO_AV_ENABLE_AUDIO;
        return true;
    }
#ifdef RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
        return true;
#endif
#ifdef RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK:
        return true;
#endif
#ifdef RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION
    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION: {
        auto *version = static_cast<unsigned *>(data);
        if (version) *version = 1;
        return true;
    }
#endif
#ifdef RETRO_ENVIRONMENT_GET_LED_INTERFACE
    case RETRO_ENVIRONMENT_GET_LED_INTERFACE:
        return false;
#endif
#ifdef RETRO_ENVIRONMENT_SET_MESSAGE
    case RETRO_ENVIRONMENT_SET_MESSAGE:
        return true;
#endif
#if !defined(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL) || (RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL != 8)
    case 8:
        return true;
#endif
#ifdef RETRO_ENVIRONMENT_GET_VFS_INTERFACE
    case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
        // No frontend VFS bridge; allow core fallback to internal file access.
        return false;
#endif
#if !defined(RETRO_ENVIRONMENT_GET_VFS_INTERFACE) || (RETRO_ENVIRONMENT_GET_VFS_INTERFACE != 65581)
    case 65581:
        return false;
#endif
#ifdef RETRO_ENVIRONMENT_SET_GEOMETRY
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        return true;
#endif
#ifdef RETRO_ENVIRONMENT_SHUTDOWN
    case RETRO_ENVIRONMENT_SHUTDOWN:
        retro_frontend_context().quit = true;
        return true;
#endif
    case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK: {
        auto *keyboard = static_cast<retro_keyboard_callback *>(data);
        retro_frontend_context().keyboard_event_cb = keyboard ? keyboard->callback : nullptr;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
        if (std::getenv("CP0_INPUT_DEBUG")) {
            auto *desc = static_cast<const retro_input_descriptor *>(data);

            std::cout << "input: SET_INPUT_DESCRIPTORS\n";

            if (desc) {
                for (unsigned i = 0; desc[i].description; ++i) {
                    std::cout << "input desc:"
                            << " port=" << desc[i].port
                            << " device=" << desc[i].device
                            << " index=" << desc[i].index
                            << " id=" << desc[i].id
                            << " desc=" << desc[i].description
                            << "\n";
                }
            }
        }

        return true;
    }
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
        return true;
    default:
        std::cerr << "unhandled environment command " << cmd << "\n";
        return false;
    }
}
