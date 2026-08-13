#include "../include/sdl_audio_output.h"
#include "../include/sdl_audio_util.h"
#include "../include/simulator.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>

bool SdlAudioOutput::start(Simulator *simulator) {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    stopLocked();
    if (simulator == nullptr) return false;
    // The synthesizer produces 44.1 kHz PCM. Keep this stream in that native
    // clock domain; SDL handles only the final conversion to the device rate.
    const SDL_AudioSpec spec = { SDL_AUDIO_S16, 1, 44100 };
    m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (m_stream == nullptr) return false;
    m_simulator = simulator;
    m_diagnostics = SDL_GetHintBoolean("ENGINE_SIM_AUDIO_DIAGNOSTICS", false);
    m_lastDiagnosticTick = SDL_GetTicks();
    m_pcmFrames = 0;
    m_silenceFrames = 0;
    m_peakQueuedBytes = 0;
    if (m_diagnostics) {
        SDL_AudioSpec source = {}, destination = {};
        if (SDL_GetAudioStreamFormat(m_stream, &source, &destination)) {
            std::fprintf(stderr, "audio: stream=%dHz/%dch -> device=%dHz/%dch\n",
                source.freq, source.channels, destination.freq, destination.channels);
        }
    }
    if (!SDL_ResumeAudioStreamDevice(m_stream)) {
        stop();
        return false;
    }
    m_running = true;
    m_thread = std::thread(&SdlAudioOutput::audioThread, this);
    return true;
}

void SdlAudioOutput::audioThread() {
    while (m_running) {
        fillStream();
        if (m_diagnostics) {
            const std::uint64_t now = SDL_GetTicks();
            if (now - m_lastDiagnosticTick >= 1000) {
                const int queuedBytes = SDL_GetAudioStreamQueued(m_stream);
                std::fprintf(stderr, "audio: pcm=%llu silence=%llu input=%.3fs output=%.3fs stream=%.3fs peak-queue=%dB\n",
                    static_cast<unsigned long long>(m_pcmFrames),
                    static_cast<unsigned long long>(m_silenceFrames),
                    m_simulator != nullptr ? m_simulator->getSynthesizerInputLatency() : 0.0,
                    m_simulator != nullptr ? m_simulator->getSynthesizerOutputLatency() : 0.0,
                    std::max(0, queuedBytes) / (44100.0 * sizeof(std::int16_t)),
                    m_peakQueuedBytes);
                m_pcmFrames = 0;
                m_silenceFrames = 0;
                m_peakQueuedBytes = 0;
                m_lastDiagnosticTick = now;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void SdlAudioOutput::fillStream() {
    if (m_stream == nullptr || m_simulator == nullptr) return;
    constexpr int chunkFrames = 512;
    // Keep a small, stable device lead. Larger queues hide underruns but make
    // controls feel disconnected and turn a single discontinuity into a
    // conspicuous delayed clack.
    constexpr int targetFrames = 1024;
    constexpr int targetBytes = targetFrames * static_cast<int>(sizeof(std::int16_t));
    int queuedBytes = SDL_GetAudioStreamQueued(m_stream);
    if (queuedBytes < 0) return;
    while (queuedBytes < targetBytes) {
        std::array<std::int16_t, chunkFrames> samples{};
        const int frames = std::min(chunkFrames,
            (targetBytes - queuedBytes) / static_cast<int>(sizeof(std::int16_t)));
        // readAudioOutput zero-fills a short read. Queuing the complete chunk
        // preserves the fixed lead the DirectSound ring buffer provided at
        // startup and during a transient synthesizer underrun.
        const int pcmFrames = m_simulator->readAudioOutput(frames, samples.data());
        m_pcmFrames += std::max(0, pcmFrames);
        m_silenceFrames += frames - std::max(0, pcmFrames);
        const int bytes = frames * static_cast<int>(sizeof(std::int16_t));
        if (!SDL_PutAudioStreamData(m_stream, samples.data(), bytes)) return;
        queuedBytes += bytes;
        m_peakQueuedBytes = std::max(m_peakQueuedBytes, queuedBytes);
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
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
    if (m_stream != nullptr) SDL_DestroyAudioStream(m_stream);
    m_stream = nullptr;
    m_simulator = nullptr;
}
