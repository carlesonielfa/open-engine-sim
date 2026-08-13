#ifndef ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H
#define ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H

#include "convolution_filter.h"
#include "leveling_filter.h"
#include "derivative_filter.h"
#include "low_pass_filter.h"
#include "jitter_filter.h"
#include "ring_buffer.h"
#include "butterworth_low_pass_filter.h"
#include "spsc_audio_ring.h"

#include <cinttypes>
#if !defined(__EMSCRIPTEN__)
#include <thread>
#include <mutex>
#include <condition_variable>
#endif
#include <atomic>

class Synthesizer {
    public:
        struct AudioParameters {
            float volume = 1.0f;
            float convolution = 1.0f;
            float dF_F_mix = 0.01f;
            float inputSampleNoise = 0.5f;
            float inputSampleNoiseFrequencyCutoff = 10000.0f;
            float airNoise = 1.0f;
            float airNoiseFrequencyCutoff = 2000.0f;
            float levelerTarget = 30000.0f;
            float levelerMaxGain = 1.9f;
            float levelerMinGain = 0.00001f;
        };

        struct Parameters {
            int inputChannelCount = 1;
            int inputBufferSize = 1024;
            int audioBufferSize = 44100;
            float inputSampleRate = 10000;
            float audioSampleRate = 44100;
            AudioParameters initialAudioParameters;
        };

        struct InputChannel {
            RingBuffer<float> data;
            SpscAudioRing<float> realtimeData;
            float *transferBuffer = nullptr;
            double lastInputSample = 0.0f;
        };

        struct ProcessingFilters {
            ConvolutionFilter convolution;
            DerivativeFilter derivative;
            JitterFilter jitterFilter;
            ButterworthLowPassFilter<float> airNoiseLowPass;
            LowPassFilter inputDcFilter;
            ButterworthLowPassFilter<double> antialiasing;
        };

    public:
        Synthesizer();
        ~Synthesizer();

        void initialize(const Parameters &p);
        void initializeImpulseResponse(
            const int16_t *impulseResponse,
            unsigned int samples,
            float volume,
            int index);
        void startAudioRenderingThread();
        void endAudioRenderingThread();
        // Browser builds do not use Wasm pthreads. Their host invokes this
        // non-blocking producer from its browser frame instead.
        bool pumpAudioRendering();
        // Called only by a real-time audio host. `target` is preallocated by
        // the host and must hold `samples` mono float samples.
        void renderRealtimeAudio(float *target, int samples);
        void clearRealtimeInput();
        void discardAudioOutput();
        void destroy();

        int readAudioOutput(int samples, int16_t *buffer);

        void writeInput(const double *data);
        void endInputBlock();

        void audioRenderingThread();
        void renderAudio();

        double getLatency() const;
        double getAudioOutputLatency() const {
            return static_cast<double>(m_audioBufferedSamples.load()) / m_audioSampleRate;
        }

        int inputDelta(int s1, int s0) const;
        double inputDistance(double s1, double s0) const;

        void setInputSampleRate(double sampleRate);
        double getInputSampleRate() const { return m_inputSampleRate; }
        // A real-time host may discard stale samples rather than let source
        // latency grow indefinitely. A negative value leaves it unbounded.
        void setMaximumInputLatency(double seconds);

        int16_t renderAudio(int inputOffset, const AudioParameters &parameters);

        double getLevelerGain();
        AudioParameters getAudioParameters();
        void setAudioParameters(const AudioParameters &params);

    //protected:
        ButterworthLowPassFilter<float> m_antialiasing;
        LevelingFilter m_levelingFilter;
        InputChannel *m_inputChannels;
        AudioParameters m_audioParameters;
        int m_inputChannelCount;
        int m_inputBufferSize;
        int m_inputSamplesRead;
        int m_latency;
        double m_inputWriteOffset;
        double m_lastInputSampleOffset;

        RingBuffer<int16_t> m_audioBuffer;
        int m_audioBufferSize;

        float m_inputSampleRate;
        float m_audioSampleRate;
        double m_maximumInputLatency;
        std::atomic<int> m_realtimeMaximumInputLatency{-1};

        // Native hosts retain the legacy worker/PCM queue. The web host uses
        // the lock-free realtime hand-off above and must not link pthreads.
#if !defined(__EMSCRIPTEN__)
        std::thread *m_thread;
#endif
        std::atomic<bool> m_run;
        std::atomic<int> m_audioBufferedSamples;
        std::atomic<int> m_realtimeLatency{0};
        std::atomic<uint32_t> m_audioNoiseState{0x6d2b79f5u};
        bool m_processed;

        // Input is produced on the simulation thread, rendered on a worker,
        // then consumed by the platform audio output. Keep each hand-off
        // independent so convolution work never blocks the device reader.
#if !defined(__EMSCRIPTEN__)
        mutable std::mutex m_inputLock;
        mutable std::mutex m_lock0;
        mutable std::mutex m_parameterLock;
        mutable std::mutex m_renderLock;
        std::condition_variable m_cv0;
#endif

        ProcessingFilters *m_filters;

        // These atomics are the only simulation/UI-to-realtime parameter
        // hand-off. Do not replace them with a mutex: AudioWorklets cannot
        // wait without causing audible dropouts.
        std::atomic<float> m_realtimeVolume{1.0f};
        std::atomic<float> m_realtimeConvolution{1.0f};
        std::atomic<float> m_realtimeDfFMix{0.01f};
        std::atomic<float> m_realtimeInputNoise{0.5f};
        std::atomic<float> m_realtimeInputNoiseCutoff{10000.0f};
        std::atomic<float> m_realtimeAirNoise{1.0f};
        std::atomic<float> m_realtimeAirNoiseCutoff{2000.0f};
        std::atomic<float> m_realtimeLevelerTarget{30000.0f};
        std::atomic<float> m_realtimeLevelerMaxGain{1.9f};
        std::atomic<float> m_realtimeLevelerMinGain{0.00001f};
};

#endif /* ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H */
