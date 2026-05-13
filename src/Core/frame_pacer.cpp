#include "frame_pacer.h"

#include <cstdlib>
#include <iostream>
#include <thread>

// Configures frame limiting from core timing and CP0_FRAME_LIMIT* overrides.
void FramePacer::configure(double core_fps, bool default_enabled, const char *env_core, float target_refresh_hz)
{
    limit_hz_ = core_fps;
    if (limit_hz_ < 10.0 || limit_hz_ > 240.0) {
        limit_hz_ = 60.0;
    }

    if (const char *limit_env = std::getenv("CP0_FRAME_LIMIT_HZ")) {
        char *end = nullptr;
        const double parsed = std::strtod(limit_env, &end);
        if (end != limit_env && parsed >= 10.0 && parsed <= 240.0) {
            limit_hz_ = parsed;
        }
    }

    enabled_ = default_enabled;
    if (const char *env = std::getenv("CP0_FRAME_LIMIT")) {
        enabled_ = env[0] != '0';
    }

    interval_ = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / limit_hz_)
    );
    next_time_ = Clock::now();

    std::cout << "pacer: "
              << (enabled_ ? "enabled" : "disabled")
              << " limit_hz=" << limit_hz_
              << " av_fps=" << core_fps
              << " target_refresh=" << target_refresh_hz
              << " core=" << (env_core ? env_core : "unknown")
              << "\n";
}

// Waits until the next presentation slot when frame limiting is enabled.
void FramePacer::wait_for_next_frame()
{
    if (!enabled_ || limit_hz_ <= 10.0 || limit_hz_ >= 240.0) {
        return;
    }

    next_time_ += interval_;

    constexpr auto spin_margin = std::chrono::microseconds(10000);

    while (true) {
        const auto now = Clock::now();
        if (now >= next_time_) {
            break;
        }

        const auto remaining = next_time_ - now;
        if (remaining > spin_margin) {
            std::this_thread::sleep_for(remaining - spin_margin);
        } else {
            while (Clock::now() < next_time_) {
#if defined(__aarch64__) || defined(__arm__)
                asm volatile("yield" ::: "memory");
#endif
            }
            break;
        }
    }

    const auto after_wait = Clock::now();
    if (after_wait - next_time_ > interval_) {
        next_time_ = after_wait;
    }
}
