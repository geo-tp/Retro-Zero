#ifndef CP0_ALSA_AUDIO_H
#define CP0_ALSA_AUDIO_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <alsa/asoundlib.h>

#include "audio_ringbuffer.h"

// Applies the boot-time ES8388 mixer defaults used by the Cardputer Zero.
void apply_boot_audio_mixer();

// ALSA playback backend used by Libretro audio callbacks.
class AlsaAudio {
public:
    // Opens ALSA and prepares optional threaded/ring-buffered playback.
    bool init(double sample_rate, bool use_ring_buffer = true);

    // Stops the audio thread and closes the ALSA PCM device.
    void shutdown();

    // Writes interleaved stereo frames produced by retro_audio_sample_batch.
    size_t write_batch(const int16_t *data, size_t frames);

    // Writes one stereo frame produced by retro_audio_sample.
    void write_sample(int16_t left, int16_t right);

    // Emits a short tone for device/audio-route diagnostics.
    void play_test_tone(unsigned milliseconds = 800);

    bool enabled() const { return pcm_ != nullptr; }

    // Adjusts and reports the software gain applied before ALSA output.
    void adjust_gain(double delta);
    double gain() const { return gain_; }

private:
    bool use_ring_buffer_ = true;

    bool open_device(const char *device, unsigned requested_rate);
    bool configure(unsigned requested_rate);

    size_t convert_frames(
        const int16_t *data,
        size_t frames,
        std::vector<int16_t> &output
    );

    int16_t condition_sample(double sample);

    void audio_thread_func();
    void start_audio_thread();
    void stop_audio_thread();

    snd_pcm_t *pcm_ = nullptr;

    unsigned input_rate_ = 0;
    unsigned rate_ = 0;
    unsigned channels_ = 0;

    double gain_ = 0.50;
    double resample_pos_ = 0.0;

    size_t clipped_samples_ = 0;
    size_t dropped_frames_ = 0;

    std::vector<int16_t> sample_accum_;
    std::vector<int16_t> conversion_buffer_;

    static constexpr size_t kAccumFlushFrames = 256;

    std::unique_ptr<AudioRingBuffer> ring_buffer_;
    std::thread audio_thread_;
    std::atomic<bool> audio_thread_running_{false};
};

#endif // CP0_ALSA_AUDIO_H
