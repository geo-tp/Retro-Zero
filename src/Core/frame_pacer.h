#ifndef CP0_FRAME_PACER_H
#define CP0_FRAME_PACER_H

#include <chrono>

// Simple frame limiter used when a core should not run faster than its target FPS.
class FramePacer {
public:
    // Reads runtime pacing overrides and computes the frame interval.
    void configure(double core_fps, bool default_enabled, const char *env_core, float target_refresh_hz);

    // Sleeps/spins until the next frame slot when pacing is enabled.
    void wait_for_next_frame();

    bool enabled() const { return enabled_; }
    double limit_hz() const { return limit_hz_; }

private:
    using Clock = std::chrono::steady_clock;

    bool enabled_ = false;
    double limit_hz_ = 60.0;
    Clock::duration interval_ {};
    Clock::time_point next_time_ {};
};

#endif // CP0_FRAME_PACER_H
