#include "../include/web_audio_output.h"

#include "../include/sdl_audio_util.h"
#include "../include/simulator.h"
#include "../include/synthesizer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
constexpr int AudioStateIdle = 0;
constexpr int AudioStateStarting = 1;
constexpr int AudioStateReady = 2;
constexpr int AudioStateFailed = 3;
alignas(16) std::uint8_t g_workletStack[64 * 1024];
}

bool WebAudioOutput::start(Simulator *simulator) {
    stop();
    m_simulator = simulator;
    if (m_simulator == nullptr) {
        m_error = "Audio was requested without a simulator";
        return false;
    }
    if (!prepare()) return false;

    // A hot engine reload replaces the simulator, so its old worklet node has
    // already been removed by stop(). The browser permission was granted by
    // the original user gesture; attach a fresh node to the already-running
    // AudioContext instead of making the user enable sound again.
    if (m_userEnabled) {
        m_simulator->synthesizer().clearRealtimeInput();
        m_callbackCount = 0;
        m_lastPeakMilli = 0;
        m_enabled = true;
        m_state = AudioStateStarting;
        if (m_workletThreadStarted) createProcessor();
        else {
            emscripten_start_wasm_audio_worklet_thread_async(
                m_context, g_workletStack, sizeof(g_workletStack),
                workletThreadInitialized, this);
        }
    }
    return true;
}

bool WebAudioOutput::prepare() {
    if (m_context != 0) return true;

    EmscriptenWebAudioCreateAttributes attributes = {};
    attributes.latencyHint = "interactive";
    // iOS commonly uses a fixed 48 kHz hardware context. Asking Web Audio for
    // its native rate lets the simulator and converted impulse responses share
    // one clock instead of rejecting a perfectly valid device.
    attributes.sampleRate = 0;
    attributes.renderSizeHint = AUDIO_CONTEXT_RENDER_SIZE_DEFAULT;
    m_context = emscripten_create_audio_context(&attributes);
    if (m_context == 0) {
        fail("The browser could not create an AudioContext");
        return false;
    }
    m_sampleRate = emscripten_audio_context_sample_rate(m_context);
    if (m_sampleRate <= 0) {
        fail("The browser did not report an AudioContext sample rate");
        emscripten_destroy_audio_context(m_context);
        m_context = 0;
        return false;
    }
    m_error.clear();
    return true;
}

bool WebAudioOutput::enable() {
    if (m_state == AudioStateReady || m_state == AudioStateStarting) return true;
    if (m_state == AudioStateFailed) return false;
    if (m_simulator == nullptr) {
        m_error = "Audio was requested before the simulator initialized";
        return false;
    }
    m_userEnabled = true;
    // Drop samples accumulated behind the user-gesture overlay before the
    // worklet begins consuming live simulation data.
    m_simulator->synthesizer().clearRealtimeInput();
    m_callbackCount = 0;
    m_lastPeakMilli = 0;
    m_enabled = true;
    emscripten_resume_audio_context_sync(m_context);
    // Create the worklet from the same user gesture that resumes playback.
    // This is accepted by browsers that restrict AudioContext operations more
    // strictly than the Web Audio specification requires.
    m_state = AudioStateStarting;
    if (m_workletThreadStarted) createProcessor();
    else {
        emscripten_start_wasm_audio_worklet_thread_async(
            m_context, g_workletStack, sizeof(g_workletStack),
            workletThreadInitialized, this);
    }
    return true;
}

bool WebAudioOutput::loadImpulseResponse(Synthesizer &synthesizer, const std::string &path,
    float volume, int index)
{
    return loadSdlImpulseResponse(synthesizer, path, volume, index, m_sampleRate);
}

void WebAudioOutput::stop() {
    m_enabled = false;
    if (m_node != 0) emscripten_destroy_web_audio_node(m_node);
    m_node = 0;
    m_simulator = nullptr;
    m_state = AudioStateIdle;
}

void WebAudioOutput::shutdown() {
    stop();
    m_userEnabled = false;
    if (m_context != 0) emscripten_destroy_audio_context(m_context);
    m_context = 0;
    m_workletThreadStarted = false;
    m_processorCreated = false;
}

const char *WebAudioOutput::lastError() const {
    return m_error.empty() ? SDL_GetError() : m_error.c_str();
}

int WebAudioOutput::contextState() const {
    return m_context == 0 ? AUDIO_CONTEXT_STATE_CLOSED
        : emscripten_audio_context_state(m_context);
}

void WebAudioOutput::workletThreadInitialized(
    EMSCRIPTEN_WEBAUDIO_T context, bool success, void *userData)
{
    auto *output = static_cast<WebAudioOutput *>(userData);
    if (!success || output == nullptr || output->m_context != context) {
        if (output != nullptr) output->fail("The browser could not start the AudioWorklet thread");
        return;
    }
    output->m_workletThreadStarted = true;
    output->createProcessor();
}

void WebAudioOutput::createProcessor() {
    // A processor name can be registered only once in an AudioWorklet scope.
    // Reloading an engine needs a new node using that existing processor, not
    // another registration attempt (which leaves the async callback pending).
    if (m_processorCreated) {
        createNode();
        return;
    }
    WebAudioWorkletProcessorCreateOptions options = {};
    options.name = "open-engine-synth";
    emscripten_create_wasm_audio_worklet_processor_async(
        m_context, &options, processorCreated, this);
}

void WebAudioOutput::processorCreated(
    EMSCRIPTEN_WEBAUDIO_T context, bool success, void *userData)
{
    auto *output = static_cast<WebAudioOutput *>(userData);
    if (!success || output == nullptr || output->m_context != context) {
        if (output != nullptr) output->fail("The browser could not create the audio processor");
        return;
    }
    output->m_processorCreated = true;
    output->createNode();
}

void WebAudioOutput::createNode() {
    int channels[] = {1};
    EmscriptenAudioWorkletNodeCreateOptions options = {};
    options.numberOfInputs = 0;
    options.numberOfOutputs = 1;
    options.outputChannelCounts = channels;
    options.channelCount = 1;
    options.channelCountMode = WEBAUDIO_CHANNEL_COUNT_MODE_EXPLICIT;
    options.channelInterpretation = WEBAUDIO_CHANNEL_INTERPRETATION_SPEAKERS;
    m_node = emscripten_create_wasm_audio_worklet_node(
        m_context, "open-engine-synth", &options, processAudio, this);
    if (m_node == 0) {
        fail("The browser could not create the audio node");
        return;
    }
    emscripten_audio_node_connect(m_node, m_context, 0, 0);
    m_state = AudioStateReady;
}

bool WebAudioOutput::processAudio(int, const AudioSampleFrame *, int outputCount,
    AudioSampleFrame *outputs, int, const AudioParamFrame *, void *userData)
{
    auto *output = static_cast<WebAudioOutput *>(userData);
    if (output == nullptr || output->m_simulator == nullptr || outputCount == 0) {
        return false;
    }
    AudioSampleFrame &frame = outputs[0];
    if (output->m_enabled) {
        output->m_simulator->synthesizer().renderRealtimeAudio(
            frame.data, frame.samplesPerChannel);
    }
    else {
        std::fill(frame.data, frame.data + frame.samplesPerChannel, 0.0f);
    }
    float peak = 0.0f;
    for (int sample = 0; sample < frame.samplesPerChannel; ++sample) {
        peak = std::max(peak, std::abs(frame.data[sample]));
    }
    output->m_lastPeakMilli = static_cast<int>(peak * 1000.0f);
    output->m_callbackCount.fetch_add(1, std::memory_order_relaxed);
    for (int channel = 1; channel < frame.numberOfChannels; ++channel) {
        std::copy(frame.data, frame.data + frame.samplesPerChannel,
            frame.data + channel * frame.samplesPerChannel);
    }
    return true;
}

void WebAudioOutput::fail(const char *message) {
    m_error = message;
    m_state = AudioStateFailed;
    m_enabled = false;
}
