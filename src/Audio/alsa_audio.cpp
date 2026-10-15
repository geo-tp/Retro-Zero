#include "alsa_audio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <pulse/error.h>
#include <pulse/simple.h>

// Sets the ES8388 output mixer to a known boot volume.
void apply_boot_audio_mixer()
{
    if (std::getenv("CP0_SKIP_BOOT_AUDIO_MIXER")) {
        return;
    }

    const char *left_cmd = "amixer -c 1 sset 'DACL' 80%";
    const char *right_cmd = "amixer -c 1 sset 'DACR' 80%";

    const int left_rc = std::system(left_cmd);
    const int right_rc = std::system(right_cmd);

    if (left_rc != 0 || right_rc != 0) {
        std::cerr << "audio mixer: failed to set DACL/DACR to 80%"
                  << " (left rc=" << left_rc
                  << ", right rc=" << right_rc << ")\n";
    }
}


void AlsaAudio::start_audio_thread()
{
    if (audio_thread_running_.load()) {
        return;
    }

    const size_t buffer_frames = std::max<size_t>(2048, rate_ / 5);
    ring_buffer_ = std::make_unique<AudioRingBuffer>(buffer_frames, channels_);
    audio_thread_running_ = true;
    audio_thread_ = std::thread(&AlsaAudio::audio_thread_func, this);
}

void AlsaAudio::stop_audio_thread()
{
    audio_thread_running_ = false;

    if (audio_thread_.joinable()) {
        audio_thread_.join();
    }
}

void AlsaAudio::audio_thread_func()
{
    constexpr size_t kBatchFrames = 512;
    std::vector<int16_t> buffer(kBatchFrames * channels_, 0);

    while (audio_thread_running_.load()) {
        size_t frames = ring_buffer_
            ? ring_buffer_->read(buffer.data(), kBatchFrames)
            : 0;

        if (frames == 0) {
            std::fill(buffer.begin(), buffer.end(), 0);
            frames = kBatchFrames;
        }

        if (!write_output(buffer.data(), frames)) {
            // shutdown() still joins this thread after an output failure.
            audio_thread_running_ = false;
        }
    }
}


namespace {
std::vector<const char *> candidate_devices()
{
    const char *env = std::getenv("CP0_ALSA_DEVICE");
    if (env && env[0]) {
        return {env};
    }

    return {
        "default",
        "sysdefault",
        "plug:default",
        "plughw:0,0",
        "hw:0,0"
    };
}

std::vector<std::string> enumerate_card_devices()
{
    std::vector<std::string> devices;
    int card = -1;
    while (snd_card_next(&card) == 0 && card >= 0) {
        char ctl_name[32];
        snprintf(ctl_name, sizeof(ctl_name), "hw:%d", card);

        snd_ctl_t *ctl = nullptr;
        if (snd_ctl_open(&ctl, ctl_name, 0) < 0) {
            continue;
        }

        snd_ctl_card_info_t *info = nullptr;
        snd_ctl_card_info_alloca(&info);
        if (snd_ctl_card_info(ctl, info) < 0) {
            snd_ctl_close(ctl);
            continue;
        }

        const char *short_id = snd_ctl_card_info_get_id(info);
        const char *name = snd_ctl_card_info_get_name(info);
        std::cerr << "alsa: found card " << card << " id="
                  << (short_id ? short_id : "?") << " name="
                  << (name ? name : "?") << "\n";

        if (short_id && short_id[0]) {
            devices.emplace_back("plughw:CARD=" + std::string(short_id) + ",DEV=0");
            devices.emplace_back("hw:CARD=" + std::string(short_id) + ",DEV=0");
        }
        devices.emplace_back("plughw:" + std::to_string(card) + ",0");
        devices.emplace_back("hw:" + std::to_string(card) + ",0");
        snd_ctl_close(ctl);
    }
    return devices;
}

unsigned env_uint(const char *name, unsigned fallback)
{
    const char *value = std::getenv(name);
    if (!value || !value[0]) {
        return fallback;
    }
    char *end = nullptr;
    unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value || parsed == 0) {
        return fallback;
    }
    return static_cast<unsigned>(parsed);
}

double env_double(const char *name, double fallback)
{
    const char *value = std::getenv(name);
    if (!value || !value[0]) {
        return fallback;
    }
    char *end = nullptr;
    double parsed = std::strtod(value, &end);
    if (end == value || parsed <= 0.0) {
        return fallback;
    }
    return parsed;
}

bool env_enabled(const char *name)
{
    const char *value = std::getenv(name);
    return value && value[0] && value[0] != '0';
}
}

// Connects to the stock audio server first; explicit ALSA overrides take priority.
bool AlsaAudio::init(double sample_rate, bool use_ring_buffer)
{
    shutdown();
    output_failed_ = false;
    unsigned requested_rate = sample_rate > 0.0
        ? static_cast<unsigned>(sample_rate + 0.5)
        : 48000;
    input_rate_ = requested_rate;
    gain_ = env_double("CP0_AUDIO_GAIN", 0.50);
    resample_pos_ = 0.0;
    clipped_samples_ = 0;
    dropped_frames_ = 0;
    conversion_buffer_.clear();

    use_ring_buffer_ = use_ring_buffer;
    const char *alsa_override = std::getenv("CP0_ALSA_DEVICE");
    if ((!alsa_override || !alsa_override[0]) && open_pulse(requested_rate)) {
        if (use_ring_buffer_) start_audio_thread();
        return true;
    }


    for (const char *device : candidate_devices()) {
        if (open_device(device, requested_rate)) {
            if (use_ring_buffer_) start_audio_thread();
            return true;
        }
    }

    for (const std::string &device : enumerate_card_devices()) {
        if (open_device(device.c_str(), requested_rate)) {
            if (use_ring_buffer_) start_audio_thread();
            return true;
        }
    }

    std::cerr << "audio: no server or playback device opened (muted). "
              << "Try CP0_ALSA_DEVICE=plughw:CARD=<id>,DEV=0\n";
    ring_buffer_.reset();
    return false;
}

bool AlsaAudio::open_pulse(unsigned requested_rate)
{
    const pa_sample_spec format {PA_SAMPLE_S16LE, requested_rate, 2};
    // Match the previous ALSA target: 100 ms of server buffering, 20 ms writes.
    const pa_buffer_attr buffering {
        UINT32_MAX,
        static_cast<uint32_t>(pa_usec_to_bytes(100000, &format)),
        UINT32_MAX,
        static_cast<uint32_t>(pa_usec_to_bytes(20000, &format)),
        UINT32_MAX,
    };
    int error = 0;
    pulse_ = pa_simple_new(nullptr, "Retro-Zero", PA_STREAM_PLAYBACK, nullptr,
                           "Emulation", &format, nullptr, &buffering, &error);
    if (!pulse_) {
        std::cerr << "pulse: connection failed: " << pa_strerror(error)
                  << "; trying ALSA\n";
        return false;
    }
    rate_ = requested_rate;
    channels_ = 2;
    std::cout << "pulse: connected to audio server S16_LE 2ch " << rate_
              << " Hz gain=" << gain_ << " (native client, no ALSA plugin)\n";
    return true;
}

bool AlsaAudio::open_device(const char *device, unsigned requested_rate)
{
    int err = snd_pcm_open(&pcm_, device, SND_PCM_STREAM_PLAYBACK, 0); // mode bloquant
    if (err < 0) {
        std::cerr << "alsa: open '" << device << "' failed: "
                  << snd_strerror(err) << "\n";
        pcm_ = nullptr;
        return false;
    }

    if (!configure(requested_rate)) {
        // A failed candidate must not reset the input rate or gain needed by
        // the next candidate. Playback has not started yet.
        snd_pcm_close(pcm_);
        pcm_ = nullptr;
        rate_ = 0;
        channels_ = 0;
        return false;
    }

    std::cout << "alsa: opened " << device << " S16_LE "
              << channels_ << "ch " << rate_ << " Hz"
              << " (core " << requested_rate << " Hz)"
              << " gain=" << gain_ << "\n";
    return true;
}

bool AlsaAudio::configure(unsigned requested_rate)
{
    snd_pcm_hw_params_t *hw = nullptr;
    snd_pcm_hw_params_alloca(&hw);

    int err = snd_pcm_hw_params_any(pcm_, hw);
    if (err < 0) {
        std::cerr << "alsa: hw_params_any failed: " << snd_strerror(err) << "\n";
        return false;
    }

    err = snd_pcm_hw_params_set_access(pcm_, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        std::cerr << "alsa: interleaved access unsupported: " << snd_strerror(err) << "\n";
        return false;
    }

    err = snd_pcm_hw_params_set_format(pcm_, hw, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        std::cerr << "alsa: S16_LE unsupported: " << snd_strerror(err) << "\n";
        return false;
    }

    channels_ = env_uint("CP0_ALSA_CHANNELS", 2);
    if (channels_ != 1 && channels_ != 2) {
        channels_ = 1;
    }
    err = snd_pcm_hw_params_set_channels_near(pcm_, hw, &channels_);
    if (err < 0 || (channels_ != 1 && channels_ != 2)) {
        std::cerr << "alsa: could not set mono/stereo channels: "
                  << snd_strerror(err) << "\n";
        return false;
    }

    rate_ = requested_rate;
    err = snd_pcm_hw_params_set_rate_near(pcm_, hw, &rate_, nullptr);
    if (err < 0) {
        std::cerr << "alsa: could not set sample rate: " << snd_strerror(err) << "\n";
        return false;
    }

    unsigned buffer_time = 100000;
    snd_pcm_hw_params_set_buffer_time_near(pcm_, hw, &buffer_time, nullptr);
    unsigned period_time = 20000;
    snd_pcm_hw_params_set_period_time_near(pcm_, hw, &period_time, nullptr);

    err = snd_pcm_hw_params(pcm_, hw);
    if (err < 0) {
        std::cerr << "alsa: apply hw params failed: " << snd_strerror(err) << "\n";
        return false;
    }

    err = snd_pcm_prepare(pcm_);
    if (err < 0) {
        std::cerr << "alsa: prepare failed: " << snd_strerror(err) << "\n";
        return false;
    }

    return true;
}

void AlsaAudio::adjust_gain(double delta)
{
    gain_ += delta;
    if (gain_ < 0.0) gain_ = 0.0;
    if (gain_ > 1.0) gain_ = 1.0;
    std::cout << "audio: gain=" << gain_ << "\n";
}

// Stops audio playback and releases ALSA resources.
void AlsaAudio::shutdown()
{
    stop_audio_thread();
    sample_accum_.clear();
    if (pulse_) {
        pa_simple_free(pulse_);
        pulse_ = nullptr;
    }
    if (pcm_) {
        snd_pcm_drop(pcm_);
        snd_pcm_close(pcm_);
        pcm_ = nullptr;
    }
    ring_buffer_.reset();
    input_rate_ = 0;
    rate_ = 0;
    channels_ = 0;
    resample_pos_ = 0.0;
    conversion_buffer_.clear();
}

// Serial output shared by the worker and direct mode (never used concurrently).
bool AlsaAudio::write_output(const int16_t *data, size_t frames)
{
    if (pulse_) {
        int error = 0;
        if (pa_simple_write(pulse_, data, frames * channels_ * sizeof(int16_t), &error) < 0) {
            std::cerr << "pulse: write failed: " << pa_strerror(error) << "; audio disabled\n";
            output_failed_ = true;
            return false;
        }
        return true;
    }

    size_t remaining = frames;
    while (remaining > 0 && (!use_ring_buffer_ || audio_thread_running_.load())) {
        const snd_pcm_sframes_t written = snd_pcm_writei(pcm_, data, remaining);
        if (written == -EPIPE && snd_pcm_prepare(pcm_) >= 0) continue;
        if (written == -EINTR) continue;
        if (written <= 0) {
            std::cerr << "alsa: write failed: " << snd_strerror(written)
                      << "; audio disabled\n";
            output_failed_ = true;
            return false;
        }
        data += written * channels_;
        remaining -= static_cast<size_t>(written);
    }
    return true;
}


// Converts and queues a batch of stereo frames for ALSA playback.
size_t AlsaAudio::write_batch(const int16_t *data, size_t frames)
{
    if (!data || frames == 0) {
        return 0;
    }
    if (!enabled()) {
        // Muted playback still consumes the core's frames. In particular, do
        // not convert with channels_ == 0 after every ALSA open has failed.
        return frames;
    }

    const bool direct_stereo =
        input_rate_ == rate_ && channels_ == 2 && gain_ >= 0.999 && gain_ <= 1.001;
    const int16_t *output_data = data;
    size_t output_frames = frames;
    if (!direct_stereo) {
        output_frames = convert_frames(data, frames, conversion_buffer_);
        output_data = conversion_buffer_.data();
    }

    if (use_ring_buffer_) {
        if (!ring_buffer_) return 0;
        size_t written = ring_buffer_->write(output_data, output_frames);
        if (written < output_frames) {
            if (env_enabled("CP0_AUDIO_DEBUG")) {
                std::cerr << "alsa: ring buffer overflow, dropped " << (output_frames - written) << " frames\n";
            }
        }
        return written;
    } else {
        write_output(output_data, output_frames);
        return frames;
    }
}

// Queues one stereo frame for ALSA playback.
void AlsaAudio::write_sample(int16_t left, int16_t right)
{
    if (!enabled()) {
        return;
    }
    sample_accum_.push_back(left);
    sample_accum_.push_back(right);
    if (sample_accum_.size() >= kAccumFlushFrames * 2) {
        write_batch(sample_accum_.data(), sample_accum_.size() / 2);
        sample_accum_.clear();
    }
}

void AlsaAudio::play_test_tone(unsigned milliseconds)
{
    if (!enabled() || rate_ == 0) {
        return;
    }

    size_t frames = static_cast<size_t>((static_cast<uint64_t>(rate_) * milliseconds) / 1000);
    std::vector<int16_t> stereo(frames * 2);
    const double pi = 3.14159265358979323846;
    for (size_t i = 0; i < frames; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(rate_);
        int16_t sample = static_cast<int16_t>(std::sin(2.0 * pi * 440.0 * t) * 9000.0);
        stereo[i * 2] = sample;
        stereo[i * 2 + 1] = sample;
    }

    std::cerr << "audio: playing " << milliseconds << "ms 440Hz test tone\n";
    write_batch(stereo.data(), frames);
    // The worker owns buffered output; do not call the server API concurrently.
    if (!use_ring_buffer_ && enabled()) {
        if (pulse_) {
            int error = 0;
            if (pa_simple_drain(pulse_, &error) < 0) {
                std::cerr << "pulse: drain failed: " << pa_strerror(error) << "\n";
                output_failed_ = true;
            }
        } else {
            snd_pcm_drain(pcm_);
            snd_pcm_prepare(pcm_);
        }
    }
}

size_t AlsaAudio::convert_frames(const int16_t *data, size_t frames, std::vector<int16_t> &output)
{
    double step = 1.0;
    size_t output_frames = frames;
    if (input_rate_ > 0 && rate_ > 0 && input_rate_ != rate_) {
        step = static_cast<double>(input_rate_) / static_cast<double>(rate_);
        output_frames = static_cast<size_t>(
            (static_cast<double>(frames) - resample_pos_) / step);
        if (output_frames == 0) {
            output_frames = 1;
        }
    }

    output.resize(output_frames * channels_);
    double pos = resample_pos_;

    for (size_t out = 0; out < output_frames; ++out) {
        size_t idx = static_cast<size_t>(pos);
        double frac = pos - static_cast<double>(idx);
        if (idx >= frames) {
            idx = frames - 1;
            frac = 0.0;
        }
        size_t next = idx + 1 < frames ? idx + 1 : idx;

        double left = static_cast<double>(data[idx * 2]) +
                      (static_cast<double>(data[next * 2]) - static_cast<double>(data[idx * 2])) * frac;
        double right = static_cast<double>(data[idx * 2 + 1]) +
                       (static_cast<double>(data[next * 2 + 1]) - static_cast<double>(data[idx * 2 + 1])) * frac;

        if (channels_ == 1) {
            output[out] = condition_sample((left + right) * 0.5);
        } else {
            output[out * 2] = condition_sample(left);
            output[out * 2 + 1] = condition_sample(right);
        }

        pos += step;
    }

    if (input_rate_ > 0 && rate_ > 0 && input_rate_ != rate_) {
        resample_pos_ = pos - static_cast<double>(frames);
        while (resample_pos_ < 0.0) {
            resample_pos_ += step;
        }
        if (env_enabled("CP0_AUDIO_DEBUG")) {
            static bool logged = false;
            if (!logged) {
                std::cerr << "alsa: software resample " << input_rate_
                          << " -> " << rate_ << " Hz\n";
                logged = true;
            }
        }
    } else {
        resample_pos_ = 0.0;
    }

    return output_frames;
}

int16_t AlsaAudio::condition_sample(double sample)
{
    sample *= gain_;

    if (sample > 30000.0) {
        ++clipped_samples_;
        sample = 30000.0 + std::tanh((sample - 30000.0) / 2768.0) * 2767.0;
    } else if (sample < -30000.0) {
        ++clipped_samples_;
        sample = -30000.0 + std::tanh((sample + 30000.0) / 2768.0) * 2767.0;
    }

    if (sample > 32767.0) {
        return 32767;
    }
    if (sample < -32768.0) {
        return -32768;
    }
    return static_cast<int16_t>(sample);
}
