#ifndef CP0_AUDIO_RINGBUFFER_H
#define CP0_AUDIO_RINGBUFFER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

// Thread-safe stereo frame ring buffer used to decouple emulation and ALSA writes.
class AudioRingBuffer {
public:
    AudioRingBuffer(size_t frames, size_t channels = 2);
    ~AudioRingBuffer();

    // Writes as many frames as fit and returns the number of frames accepted.
    size_t write(const int16_t *data, size_t frames);

    // Reads up to the requested frame count and returns the number read.
    size_t read(int16_t *data, size_t frames);

    // Reports queued readable frames.
    size_t available() const;

    // Reports writable free frames.
    size_t free() const;

    // Drops all queued audio.
    void clear();

    size_t channels() const { return channels_; }
    size_t capacity() const { return capacity_; }

private:
    std::vector<int16_t> buffer_;
    const size_t channels_;
    const size_t capacity_; // In frames.
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
    mutable std::mutex lock_;
};

#endif // CP0_AUDIO_RINGBUFFER_H
