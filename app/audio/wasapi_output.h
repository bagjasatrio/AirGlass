// Glasscast — WASAPI audio sink.
//
// Replaces upstream's SDL_QueueAudio sink. Three things the SDL path cannot do,
// and which are exactly the "audio sometimes disappears / arrives late" bugs:
//
//   1. Survive a default-device change. WASAPI hands us
//      IMMNotificationClient; when the user plugs in headphones or switches to
//      HDMI mid-session we rebuild IAudioClient on the new endpoint and keep
//      streaming — the network stream is never touched.
//   2. Cope with a device mix rate that differs from AirPlay's 44.1 kHz.
//      libswresample converts per push; upstream assumed the two always match,
//      which is false for most HDMI/48 kHz endpoints.
//   3. Event-driven rendering with a short period instead of SDL's queue, so
//      latency stays ~20-30 ms and does not grow while the queue drains.
//
// GPL-3.0. No upstream UxPlay code in this file.

#pragma once

#include <cstdint>
#include <memory>

namespace ap::audio {

class WasapiOutput {
public:
    WasapiOutput();
    ~WasapiOutput();

    WasapiOutput(const WasapiOutput&)            = delete;
    WasapiOutput& operator=(const WasapiOutput&) = delete;

    // Opens the default render endpoint. `sample_rate`/`channels` describe the
    // INCOMING stream (44.1 kHz stereo for AirPlay); conversion to the endpoint
    // mix format happens internally.
    bool start(int sample_rate, int channels);
    void stop();

    bool running() const;

    // Interleaved int16 samples; `count` is total int16 values, not frames.
    // Non-blocking: drops the oldest audio if the sink falls more than
    // `max_backlog_ms` behind, because stale audio is worse than a gap.
    void push(const int16_t* samples, int count);

    // AirPlay volume convention: 0 dB = full, <= -144 dB = mute.
    void set_volume_db(float db);

    uint32_t queued_bytes() const;
    int64_t  pending_ns() const;
    void     clear();

    // Diagnostics for the stats overlay.
    int      endpoint_rate() const;
    int      endpoint_channels() const;
    uint64_t device_reinits() const;   // how many times the endpoint was rebuilt
    uint64_t dropped_frames() const;   // frames discarded by the backlog guard

    static constexpr int kMaxBacklogMs = 100;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ap::audio
