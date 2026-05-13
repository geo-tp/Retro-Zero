#include "audio_ringbuffer.h"
#include <cstring>

AudioRingBuffer::AudioRingBuffer(size_t frames, size_t channels)
    : buffer_(frames * channels), channels_(channels), capacity_(frames), head_(0), tail_(0) {}

AudioRingBuffer::~AudioRingBuffer() {}

size_t AudioRingBuffer::write(const int16_t* data, size_t frames) {
    std::lock_guard<std::mutex> guard(lock_);
    size_t written = 0;
    while (written < frames) {
        size_t next_head = (head_ + 1) % capacity_;
        if (next_head == tail_) break; // buffer full
        size_t idx = head_ * channels_;
        std::memcpy(&buffer_[idx], &data[written * channels_], sizeof(int16_t) * channels_);
        head_ = next_head;
        ++written;
    }
    return written;
}

size_t AudioRingBuffer::read(int16_t* data, size_t frames) {
    std::lock_guard<std::mutex> guard(lock_);
    size_t read = 0;
    while (read < frames && tail_ != head_) {
        size_t idx = tail_ * channels_;
        std::memcpy(&data[read * channels_], &buffer_[idx], sizeof(int16_t) * channels_);
        tail_ = (tail_ + 1) % capacity_;
        ++read;
    }
    return read;
}

size_t AudioRingBuffer::available() const {
    std::lock_guard<std::mutex> guard(lock_);
    if (head_ >= tail_) return head_ - tail_;
    return capacity_ - (tail_ - head_);
}

size_t AudioRingBuffer::free() const {
    return capacity_ - available() - 1;
}

void AudioRingBuffer::clear() {
    std::lock_guard<std::mutex> guard(lock_);
    head_ = tail_ = 0;
}
