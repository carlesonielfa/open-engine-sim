#ifndef ATG_ENGINE_SIM_SDL_AUDIO_OUTPUT_H
#define ATG_ENGINE_SIM_SDL_AUDIO_OUTPUT_H

#include "audio_output.h"

#include <SDL3/SDL_audio.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

class SdlAudioOutput final : public AudioOutput {
public:
    ~SdlAudioOutput() override { stop(); }
    bool start(Simulator *simulator) override;
    bool loadImpulseResponse(Synthesizer &synthesizer, const std::string &path, float volume, int index) override;
    void stop() override;

private:
    void audioThread();
    void fillStream();
    void stopLocked();

    SDL_AudioStream *m_stream = nullptr;
    Simulator *m_simulator = nullptr;
    std::atomic<bool> m_running = false;
    std::mutex m_lifecycleMutex;
    std::thread m_thread;
    bool m_diagnostics = false;
    std::uint64_t m_lastDiagnosticTick = 0;
    std::uint64_t m_pcmFrames = 0;
    std::uint64_t m_silenceFrames = 0;
    int m_peakQueuedBytes = 0;
};

#endif
