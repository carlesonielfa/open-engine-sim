#ifndef OPEN_ENGINE_SIM_WEB_AUDIO_OUTPUT_H
#define OPEN_ENGINE_SIM_WEB_AUDIO_OUTPUT_H

#include "audio_output.h"

#include <atomic>
#include <cstdint>
#include <string>

#include <emscripten/webaudio.h>

// Browser audio starts after a user gesture, then runs entirely in an
// Emscripten Wasm AudioWorklet. SDL is deliberately not involved in playback:
// its queued WebAudio backend runs from the browser frame and cannot meet a
// real-time audio deadline.
class WebAudioOutput final : public AudioOutput {
public:
    ~WebAudioOutput() override { shutdown(); }

    // Create the suspended browser context early enough for the simulator to
    // adopt its fixed device sample rate.
    bool prepare();
    bool start(Simulator *simulator) override;
    bool loadImpulseResponse(Synthesizer &synthesizer, const std::string &path,
        float volume, int index) override;
    void stop() override;
    void shutdown();
    int outputSampleRate() const override { return m_sampleRate; }

    bool enable();
    const char *lastError() const;
    int state() const { return m_state.load(); }
    int contextState() const;
    bool hasWorkletThread() const { return m_workletThreadStarted; }
    bool hasNode() const { return m_node != 0; }
    std::uint64_t callbackCount() const { return m_callbackCount.load(); }
    int lastPeakMilli() const { return m_lastPeakMilli.load(); }

private:
    static void workletThreadInitialized(
        EMSCRIPTEN_WEBAUDIO_T context, bool success, void *userData);
    static void processorCreated(
        EMSCRIPTEN_WEBAUDIO_T context, bool success, void *userData);
    static bool processAudio(int inputCount, const AudioSampleFrame *inputs,
        int outputCount, AudioSampleFrame *outputs, int parameterCount,
        const AudioParamFrame *parameters, void *userData);
    void createProcessor();
    void createNode();
    void fail(const char *message);

    EMSCRIPTEN_WEBAUDIO_T m_context = 0;
    EMSCRIPTEN_WEBAUDIO_T m_node = 0;
    Simulator *m_simulator = nullptr;
    std::atomic<bool> m_enabled{false};
    // This records the user's initial browser gesture separately from the
    // currently connected worklet node. Engine reloads intentionally destroy
    // that node, then restore it for the replacement simulator.
    bool m_userEnabled = false;
    std::string m_error;
    std::atomic<int> m_state{0};
    int m_sampleRate = 44100;
    bool m_workletThreadStarted = false;
    bool m_processorCreated = false;
    std::atomic<std::uint64_t> m_callbackCount{0};
    std::atomic<int> m_lastPeakMilli{0};
};

#endif
