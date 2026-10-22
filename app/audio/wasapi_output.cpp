// AirGlass — WASAPI audio sink implementation.
// GPL-3.0. No upstream UxPlay code in this file.

#include "wasapi_output.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
}

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "log.h"

#pragma comment(lib, "ole32.lib")

namespace ap::audio {
namespace {

using Microsoft::WRL::ComPtr;

constexpr REFERENCE_TIME kBufferDuration = 200000;  // 20 ms in 100-ns units
constexpr DWORD kEventTimeoutMs = 100;

bool com_init() {
    // The worker thread owns its COM apartment. S_FALSE means already
    // initialised on this thread, which is fine.
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    return SUCCEEDED(hr);
}

}  // namespace

struct WasapiOutput::Impl {
    // ---- configuration -----------------------------------------------------
    int  src_rate = 44100;
    int  src_channels = 2;
    int  out_rate = 48000;
    int  out_channels = 2;

    // ---- WASAPI objects ----------------------------------------------------
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice>           device;
    ComPtr<IAudioClient>        client;
    ComPtr<IAudioRenderClient>  render;
    HANDLE                      event = nullptr;

    // ---- buffering ---------------------------------------------------------
    std::mutex          ring_mu;
    std::deque<float>   ring;            // interleaved float at out_rate/out_channels
    SwrContext*         swr = nullptr;

    std::atomic<float>  gain{1.0f};
    std::atomic<bool>   stop_flag{false};
    std::atomic<bool>   running{false};
    std::atomic<bool>   device_changed{false};

    std::atomic<uint64_t> reinits{0};
    std::atomic<uint64_t> dropped_frames{0};

    std::thread         worker;

    // ---- endpoint notification --------------------------------------------
    class Notifier : public IMMNotificationClient {
    public:
        explicit Notifier(Impl* owner) : owner_(owner) {}
        ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
        ULONG STDMETHODCALLTYPE Release() override {
            const ULONG left = --refs_;
            if (!left) delete this;
            return left;
        }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
            if (!out) return E_POINTER;
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
                *out = static_cast<IMMNotificationClient*>(this);
                AddRef();
                return S_OK;
            }
            *out = nullptr;
            return E_NOINTERFACE;
        }
        HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role,
                                                         LPCWSTR) override {
            if (flow == eRender && role == eConsole) owner_->device_changed.store(true);
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
            owner_->device_changed.store(true);
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
            return S_OK;
        }

    private:
        ~Notifier() = default;
        Impl* owner_;
        std::atomic<ULONG> refs_{1};
    };

    Notifier* notifier = nullptr;

    // ---- helpers -----------------------------------------------------------
    void release_swr() {
        if (swr) {
            swr_free(&swr);
            swr = nullptr;
        }
    }

    void close_client() {
        if (client) client->Stop();
        render.Reset();
        client.Reset();
        device.Reset();
    }

    // Opens the current default render endpoint. Tries to keep our own 44.1 kHz
    // float format (Windows converts via AUTOCONVERTPCM); falls back to the mix
    // format plus libswresample when the driver refuses.
    bool open_endpoint() {
        if (!enumerator) {
            if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                        IID_PPV_ARGS(&enumerator))) ||
                !enumerator) {
                LOG_WARN << "wasapi: no device enumerator";
                return false;
            }
        }
        close_client();
        if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) || !device) {
            LOG_WARN << "wasapi: no default render endpoint";
            return false;
        }
        if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client)) ||
            !client) {
            LOG_WARN << "wasapi: IAudioClient activation failed";
            return false;
        }

        WAVEFORMATEX* mix = nullptr;
        if (FAILED(client->GetMixFormat(&mix)) || !mix) {
            LOG_WARN << "wasapi: GetMixFormat failed";
            return false;
        }
        out_rate = static_cast<int>(mix->nSamplesPerSec);
        out_channels = mix->nChannels;
        CoTaskMemFree(mix);

        // Preferred: hand WASAPI our source format and let it resample.
        WAVEFORMATEX src{};
        src.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        src.nChannels = static_cast<WORD>(src_channels);
        src.nSamplesPerSec = static_cast<DWORD>(src_rate);
        src.wBitsPerSample = 32;
        src.nBlockAlign = static_cast<WORD>(src.nChannels * src.wBitsPerSample / 8);
        src.nAvgBytesPerSec = src.nSamplesPerSec * src.nBlockAlign;

        DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                      AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, kBufferDuration, 0, &src,
                                       nullptr);
        bool swr_needed = false;
        if (FAILED(hr)) {
            // Fallback: mix format in, libswresample does the conversion.
            WAVEFORMATEX native{};
            native.wFormatTag = WAVE_FORMAT_PCM;
            native.nChannels = static_cast<WORD>(out_channels);
            native.nSamplesPerSec = static_cast<DWORD>(out_rate);
            native.wBitsPerSample = 16;
            native.nBlockAlign = static_cast<WORD>(native.nChannels * native.wBitsPerSample / 8);
            native.nAvgBytesPerSec = native.nSamplesPerSec * native.nBlockAlign;
            flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
            hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, kBufferDuration, 0, &native,
                                    nullptr);
            swr_needed = true;
        }
        if (FAILED(hr)) {
            LOG_WARN << "wasapi: Initialize failed hr=0x" << std::hex << hr;
            close_client();
            return false;
        }

        if (!event) event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event || FAILED(client->SetEventHandle(event))) {
            LOG_WARN << "wasapi: SetEventHandle failed";
            close_client();
            return false;
        }
        if (FAILED(client->GetService(__uuidof(IAudioRenderClient), &render)) || !render) {
            LOG_WARN << "wasapi: no render client";
            close_client();
            return false;
        }

        release_swr();
        if (swr_needed) {
            AVChannelLayout in_layout{};
            AVChannelLayout out_layout{};
            av_channel_layout_default(&in_layout, src_channels);
            av_channel_layout_default(&out_layout, out_channels);
            if (swr_alloc_set_opts2(&swr, &out_layout, AV_SAMPLE_FMT_FLT, out_rate, &in_layout,
                                    AV_SAMPLE_FMT_FLT, src_rate, 0, nullptr) < 0 ||
                swr_init(swr) < 0) {
                LOG_WARN << "wasapi: swr init failed, audio will be dropped";
                release_swr();
            }
        }

        if (FAILED(client->Start())) {
            LOG_WARN << "wasapi: Start failed";
            close_client();
            return false;
        }
        LOG_INFO << "wasapi: playing " << src_rate << " Hz x" << src_channels << " -> "
                 << out_rate << " Hz x" << out_channels
                 << (swr_needed ? " (swresample)" : " (AUTOCONVERTPCM)");
        return true;
    }

    size_t ring_frames() {
        std::lock_guard<std::mutex> lock(ring_mu);
        return out_channels > 0 ? ring.size() / static_cast<size_t>(out_channels) : 0;
    }

    void worker_loop() {
        com_init();
        // Register for endpoint changes before opening anything: a device swap
        // that happens during the first open must still be caught.
        if (!enumerator &&
            (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                     IID_PPV_ARGS(&enumerator))) ||
             !enumerator)) {
            LOG_WARN << "wasapi: no device enumerator (audio disabled)";
            CoUninitialize();
            return;
        }
        if (!notifier) {
            notifier = new Notifier(this);
            enumerator->RegisterEndpointNotificationCallback(notifier);
        }
        while (!stop_flag.load()) {
            if (device_changed.exchange(false)) {
                ++reinits;
                ring_mu.lock();
                ring.clear();  // never replay audio into a different endpoint
                ring_mu.unlock();
                open_endpoint();
                continue;
            }
            if (!client || !render) {
                if (!open_endpoint()) {
                    Sleep(kEventTimeoutMs);
                    continue;
                }
            }
            WaitForSingleObject(event, kEventTimeoutMs);

            UINT32 padding = 0;
            if (FAILED(client->GetCurrentPadding(&padding))) continue;
            UINT32 buffer_frames = 0;
            if (FAILED(client->GetBufferSize(&buffer_frames))) continue;
            const UINT32 available = buffer_frames - padding;
            if (!available) continue;

            BYTE* data = nullptr;
            if (FAILED(render->GetBuffer(available, &data)) || !data) continue;

            const size_t want = static_cast<size_t>(available) * out_channels;
            std::vector<float> scratch(want, 0.0f);
            size_t got = 0;
            {
                std::lock_guard<std::mutex> lock(ring_mu);
                got = std::min(want, ring.size());
                for (size_t i = 0; i < got; ++i) {
                    scratch[i] = ring.front();
                    ring.pop_front();
                }
            }
            std::memcpy(data, scratch.data(), want * sizeof(float));
            render->ReleaseBuffer(available, 0);
        }

        close_client();
        if (event) {
            CloseHandle(event);
            event = nullptr;
        }
        CoUninitialize();
    }
};

WasapiOutput::WasapiOutput() : impl_(std::make_unique<Impl>()) {}

WasapiOutput::~WasapiOutput() { stop(); }

bool WasapiOutput::start(int sample_rate, int channels) {
    if (impl_->running.load()) return true;
    impl_->src_rate = sample_rate > 0 ? sample_rate : 44100;
    impl_->src_channels = channels > 0 ? channels : 2;
    impl_->stop_flag.store(false);
    impl_->device_changed.store(false);

    // Endpoint registration must happen on a COM-initialised thread; the worker
    // does its own initialisation, so register from there via a one-shot barrier.
    impl_->worker = std::thread([this] {
        impl_->worker_loop();
    });

    // Give the worker a moment to open the endpoint so `running()` is truthful
    // for callers that check right after start().
    for (int i = 0; i < 50; ++i) {
        if (impl_->client && impl_->render) break;
        Sleep(10);
    }
    impl_->running.store(impl_->client != nullptr);
    return impl_->running.load();
}

void WasapiOutput::stop() {
    if (!impl_ || !impl_->worker.joinable()) return;
    impl_->stop_flag.store(true);
    if (impl_->event) SetEvent(impl_->event);
    impl_->worker.join();
    impl_->running.store(false);
}

bool WasapiOutput::running() const { return impl_ && impl_->running.load(); }

void WasapiOutput::push(const int16_t* samples, int count) {
    if (!impl_ || !samples || count <= 0 || !impl_->running.load()) return;

    const float gain = impl_->gain.load();
    const size_t frames = static_cast<size_t>(count) / static_cast<size_t>(impl_->src_channels);
    if (!frames) return;

    // int16 -> float, gain applied once here (never in the render loop).
    std::vector<float> interleaved(frames * static_cast<size_t>(impl_->src_channels));
    for (size_t i = 0; i < interleaved.size(); ++i) {
        interleaved[i] = static_cast<float>(samples[i]) / 32768.0f * gain;
    }

    std::lock_guard<std::mutex> lock(impl_->ring_mu);

    if (impl_->swr) {
        const uint8_t* in_ptr = reinterpret_cast<const uint8_t*>(interleaved.data());
        int in_left = static_cast<int>(frames);
        const int max_out = static_cast<int>(av_rescale_rnd(
            swr_get_delay(impl_->swr, impl_->src_rate) + in_left, impl_->out_rate, impl_->src_rate,
            AV_ROUND_UP));
        if (max_out > 0) {
            std::vector<float> out(static_cast<size_t>(max_out) * impl_->out_channels);
            uint8_t* out_ptr = reinterpret_cast<uint8_t*>(out.data());
            const int converted = swr_convert(impl_->swr, &out_ptr, max_out, &in_ptr, in_left);
            if (converted > 0) {
                out.resize(static_cast<size_t>(converted) * impl_->out_channels);
                impl_->ring.insert(impl_->ring.end(), out.begin(), out.end());
            }
        }
    } else {
        impl_->ring.insert(impl_->ring.end(), interleaved.begin(), interleaved.end());
    }

    // Backlog guard: stale audio is worse than a gap. Drop from the front so
    // what plays is always the newest material.
    const size_t channels = static_cast<size_t>(impl_->out_channels);
    const size_t max_frames = static_cast<size_t>(impl_->out_rate) * kMaxBacklogMs / 1000;
    if (channels && impl_->ring.size() / channels > max_frames) {
        const size_t excess_frames = impl_->ring.size() / channels - max_frames;
        impl_->dropped_frames += excess_frames;
        impl_->ring.erase(impl_->ring.begin(),
                          impl_->ring.begin() + static_cast<long>(excess_frames * channels));
    }
}

void WasapiOutput::set_volume_db(float db) {
    if (!impl_) return;
    // AirPlay: 0 dB is unity, <= -144 dB is silence.
    const float linear = db <= -144.0f ? 0.0f : std::pow(10.0f, db / 20.0f);
    impl_->gain.store(linear);
}

uint32_t WasapiOutput::queued_bytes() const {
    if (!impl_) return 0;
    const size_t frames = impl_->ring_frames();
    return static_cast<uint32_t>(frames * static_cast<size_t>(impl_->out_channels) *
                                 sizeof(float));
}

int64_t WasapiOutput::pending_ns() const {
    if (!impl_ || impl_->out_rate <= 0) return 0;
    const size_t frames = impl_->ring_frames();
    return static_cast<int64_t>(frames) * 1000000000ll / impl_->out_rate;
}

void WasapiOutput::clear() {
    if (!impl_) return;
    std::lock_guard<std::mutex> lock(impl_->ring_mu);
    impl_->ring.clear();
    if (impl_->swr) swr_convert(impl_->swr, nullptr, 0, nullptr, 0);  // drop swr delay
}

int WasapiOutput::endpoint_rate() const { return impl_ ? impl_->out_rate : 0; }
int WasapiOutput::endpoint_channels() const { return impl_ ? impl_->out_channels : 0; }
uint64_t WasapiOutput::device_reinits() const { return impl_ ? impl_->reinits.load() : 0; }
uint64_t WasapiOutput::dropped_frames() const { return impl_ ? impl_->dropped_frames.load() : 0; }

}  // namespace ap::audio
