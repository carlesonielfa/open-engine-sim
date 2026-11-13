#include "../include/sdl_audio_output.h"
#include "../include/sdl_audio_util.h"
#include "../include/simulator.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

bool SdlAudioOutput::start(Simulator *simulator) {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    stopLocked();
    if (simulator == nullptr) return false;
    // The synthesizer produces 44.1 kHz PCM. Keep this stream in that native
    // clock domain; SDL handles only the final conversion to the device rate.
    const SDL_AudioSpec spec = { SDL_AUDIO_S16, 1, 44100 };
    m_simulator = simulator;
    m_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &SdlAudioOutput::audioCallback, this);
    if (m_stream == nullptr) {
        m_simulator = nullptr;
        return false;
    }
    m_diagnostics = SDL_GetHintBoolean("ENGINE_SIM_AUDIO_DIAGNOSTICS", false);
    m_lastDiagnosticTick = SDL_GetTicks();
    m_pcmFrames = 0;
    m_silenceFrames = 0;
    m_peakQueuedBytes = 0;
    m_underrunEvents.store(0);
    m_clipEvents.store(0);
    m_declicker.reset();
    m_sourceBytesPerFrame = static_cast<int>(sizeof(std::int16_t));
    m_deviceFrequency = 0;
    if (m_diagnostics) {
        SDL_AudioSpec source = {}, destination = {};
        if (SDL_GetAudioStreamFormat(m_stream, &source, &destination)) {
            std::fprintf(stderr, "audio: stream=%dHz/%dch -> device=%dHz/%dch\n",
                source.freq, source.channels, destination.freq, destination.channels);
        }
    }
#if defined(__ANDROID__)
    // SDL's stream callback reports demand in bytes in the stream's SOURCE
    // format. Let SDL own resampling to the device's actual format/rate instead
    // of trying to infer a hardware queue from SDL_GetAudioStreamQueued().
    SDL_AudioSpec source = {}, destination = {};
    if (SDL_GetAudioStreamFormat(m_stream, &source, &destination)) {
        m_sourceBytesPerFrame = std::max(1,
            static_cast<int>(SDL_AUDIO_BYTESIZE(source.format)) * source.channels);
        m_deviceFrequency = destination.freq;
    }
#endif
    m_running.store(true, std::memory_order_release);
    if (!SDL_ResumeAudioStreamDevice(m_stream)) {
        stopLocked();
        return false;
    }
    return true;
}

void SDLCALL SdlAudioOutput::audioCallback(
    void *userdata, SDL_AudioStream *stream, int additionalAmount, int)
{
    auto *output = static_cast<SdlAudioOutput *>(userdata);
    if (output == nullptr) return;
    output->m_callbacksInFlight.fetch_add(1, std::memory_order_acq_rel);
    if (output->m_running.load(std::memory_order_acquire) &&
        output->m_simulator != nullptr && additionalAmount > 0) {
        output->fillStream(stream, additionalAmount);
    }
    output->m_callbacksInFlight.fetch_sub(1, std::memory_order_acq_rel);
}

void SdlAudioOutput::fillStream(SDL_AudioStream *stream, int requestedBytes) {
    if (stream == nullptr || m_simulator == nullptr || requestedBytes <= 0) return;

    // Nothing in this function may allocate, log, call into Java, or wait on a
    // lock held by anything slow: it runs on the device's real-time thread.
#if defined(__ANDROID__)
    constexpr int chunkFrames = 2048;
#else
    constexpr int chunkFrames = 512;
#endif
    constexpr int bytesPerFrame = static_cast<int>(sizeof(std::int16_t));
    std::array<std::int16_t, chunkFrames> samples{};

    // SDL can request a very small refill exactly when the render worker is
    // briefly late. Feed at least one block on Android so the device stream
    // keeps some headroom instead of repeatedly running on the edge.
    // additionalAmount is SDL's current source-side demand, so supplying
    // exactly that (plus the cushion) keeps the stream from growing latency.
#if defined(__ANDROID__)
    int remainingBytes = std::max(requestedBytes, chunkFrames * bytesPerFrame);
#else
    int remainingBytes = requestedBytes;
#endif
    while (remainingBytes > 0) {
        const int frames = std::min(chunkFrames,
            (remainingBytes + bytesPerFrame - 1) / bytesPerFrame);
        const int pcmFrames = m_simulator->readAudioOutput(frames, samples.data());
        const int validFrames = std::max(0, std::min(pcmFrames, frames));
        m_pcmFrames += validFrames;
        m_silenceFrames += frames - validFrames;

        bool clipped = false;
        for (int i = 0; i < validFrames; ++i) {
            if (std::abs(static_cast<int>(samples[i])) >= 32760) {
                clipped = true;
                break;
            }
        }
        if (validFrames < frames) m_underrunEvents.fetch_add(1, std::memory_order_relaxed);
        if (clipped) m_clipEvents.fetch_add(1, std::memory_order_relaxed);

        // Ramp the edges of any gap so an underrun is a brief dip instead of
        // a click.
        m_declicker.process(samples.data(), validFrames, frames);

        const int bytes = frames * bytesPerFrame;
        if (!SDL_PutAudioStreamData(stream, samples.data(), bytes)) return;
        remainingBytes -= bytes;
    }

    if (m_diagnostics) {
        const std::uint64_t now = SDL_GetTicks();
        if (now - m_lastDiagnosticTick >= 1000) {
            std::fprintf(stderr,
                "audio-demand: pcm=%llu silence=%llu underruns=%llu clips=%llu input=%.3fs output=%.3fs requested=%dB device=%dHz\n",
                static_cast<unsigned long long>(m_pcmFrames),
                static_cast<unsigned long long>(m_silenceFrames),
                static_cast<unsigned long long>(m_underrunEvents.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(m_clipEvents.load(std::memory_order_relaxed)),
                m_simulator->getSynthesizerInputLatency(),
                m_simulator->getSynthesizerOutputLatency(),
                requestedBytes,
                m_deviceFrequency);
            m_pcmFrames = 0;
            m_silenceFrames = 0;
            m_lastDiagnosticTick = now;
        }
    }
}


bool SdlAudioOutput::loadImpulseResponse(Synthesizer &synthesizer, const std::string &path, float volume, int index) {
    return loadSdlImpulseResponse(synthesizer, path, volume, index);
}

void SdlAudioOutput::stop() {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    stopLocked();
}

void SdlAudioOutput::stopLocked() {
    // Hotload safety: publish the stopped state first, detach SDL's callback,
    // then wait for a callback that was already executing to leave before the
    // old Simulator can be destroyed by EngineSimApplication.
    m_running.store(false, std::memory_order_release);
    SDL_AudioStream *oldStream = m_stream;
    m_stream = nullptr;
    if (oldStream != nullptr) {
        SDL_SetAudioStreamGetCallback(oldStream, nullptr, nullptr);
        while (m_callbacksInFlight.load(std::memory_order_acquire) != 0) {
            SDL_Delay(1);
        }
        SDL_DestroyAudioStream(oldStream);
    }
    m_simulator = nullptr;
}
