#include <gtest/gtest.h>

#include "../include/synthesizer.h"

#include <chrono>
#include <atomic>
#include <cmath>
#include <thread>

using namespace std::chrono_literals;

void setupStandardSynthesizer(Synthesizer &synth) {
    Synthesizer::Parameters params;
    params.audioBufferSize = 512 * 16;
    params.audioSampleRate = 16;
    params.inputBufferSize = 256;
    params.inputChannelCount = 8;
    params.inputSampleRate = 32;

    Synthesizer::AudioParameters audioParams;
    audioParams.airNoise = 0.0;
    audioParams.inputSampleNoise = 0.0;
    audioParams.convolution = 0.0;
    audioParams.levelerMaxGain = 1.0;
    audioParams.levelerMinGain = 1.0;
    audioParams.dF_F_mix = 0.0;
    params.initialAudioParameters = audioParams;

    synth.initialize(params);
}

TEST(SynthesizerTests, SynthesizerSanityCheck) {
    Synthesizer synth;
    setupStandardSynthesizer(synth);
    synth.destroy();
}

TEST(SynthesizerTests, RealtimeQueueCanDiscardStaleFrames) {
    SpscAudioRing<int> queue;
    queue.initialize(16);

    for (int value = 0; value < 10; ++value) {
        ASSERT_TRUE(queue.push(value));
    }

    EXPECT_EQ(queue.discard(7), 7u);
    EXPECT_EQ(queue.size(), 3u);

    int value = -1;
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 7);
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 8);
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 9);
}

TEST(SynthesizerTests, ThreadedProducerAndConsumerMaintainOutput) {
    Synthesizer synth;
    Synthesizer::Parameters params;
    params.audioBufferSize = 4096;
    params.audioSampleRate = 44100;
    params.inputBufferSize = 4096;
    params.inputChannelCount = 1;
    params.inputSampleRate = 44100;
    params.initialAudioParameters.airNoise = 0.0f;
    params.initialAudioParameters.inputSampleNoise = 0.0f;
    params.initialAudioParameters.convolution = 0.0f;
    params.initialAudioParameters.dF_F_mix = 0.0f;
    params.initialAudioParameters.levelerMaxGain = 1.0f;
    params.initialAudioParameters.levelerMinGain = 1.0f;
    synth.initialize(params);
    synth.startAudioRenderingThread();

    std::atomic<bool> producerFinished = false;
    std::thread producer([&] {
        for (int block = 0; block < 100; ++block) {
            for (int i = 0; i < 10; ++i) {
                const double phase = 2.0 * 3.141592653589793 * 0.01 * (block * 10 + i);
                const double sample[] = { 12000.0 * std::sin(phase) };
                synth.writeInput(sample);
            }
            synth.endInputBlock();
            std::this_thread::sleep_for(1ms);
        }
        producerFinished = true;
    });

    int received = 0;
    int longestNearZeroRun = 0;
    int nearZeroRun = 0;
    int16_t buffer[10] = {};
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline
        && (!producerFinished || received < 950))
    {
        const int read = synth.readAudioOutput(10, buffer);
        for (int i = 0; i < read; ++i) {
            if (std::abs(buffer[i]) < 20) ++nearZeroRun;
            else nearZeroRun = 0;
            longestNearZeroRun = std::max(longestNearZeroRun, nearZeroRun);
        }
        received += read;
        std::this_thread::sleep_for(1ms);
    }

    producer.join();
    EXPECT_GE(received, 950);
    // The high-pass stage settles through zero around waveform crossings, but
    // the threaded hand-off must not create a long silent run.
    EXPECT_LE(longestNearZeroRun, 20);

    synth.endAudioRenderingThread();
    synth.destroy();
}

TEST(SynthesizerTests, PumpedProducerMaintainsOutputWithoutWorkerThread) {
    Synthesizer synth;
    setupStandardSynthesizer(synth);
    synth.discardAudioOutput();

    for (int i = 0; i < 64; ++i) {
        const double sample[] = { 1000.0, 1000.0, 1000.0, 1000.0,
            1000.0, 1000.0, 1000.0, 1000.0 };
        synth.writeInput(sample);
    }
    synth.endInputBlock();

    EXPECT_TRUE(synth.pumpAudioRendering());
    int16_t output[16] = {};
    EXPECT_EQ(synth.readAudioOutput(16, output), 16);
    synth.destroy();
}
/*
TEST(SynthesizerTests, SynthesizerConversionTest) {
    Synthesizer synth;
    setupStandardSynthesizer(synth);

    EXPECT_NEAR(synth.inputSampleToTimeOffset(0.0), 0.0, 1E-6);
    EXPECT_NEAR(synth.inputSampleToTimeOffset(1.0), 1 / 32.0, 1E-6);

    EXPECT_NEAR(synth.audioSampleToTimeOffset(0), -0.5, 1E-6);

    synth.destroy();
}

TEST(SynthesizerTests, SynthesizerTrimTest) {
    Synthesizer synth;
    setupStandardSynthesizer(synth);

    const double timeOffset0 = synth.audioSampleToTimeOffset(0);

    synth.trimInput(0.5, false);

    const double timeOffset1 = synth.audioSampleToTimeOffset(8);

    EXPECT_NEAR(timeOffset1, timeOffset0, 1E-6);

    synth.destroy();
}

TEST(SynthesizerTests, SynthesizerSampleTest) {
    Synthesizer synth;
    setupStandardSynthesizer(synth);

    for (int i = 0; i < 1024; ++i) {
        const double v = (double)i;
        const double data[] = { v, v, v, v, v, v, v, v };
        synth.writeInput(data);
    }

    const double end_t = 1023 / 32.0;

    const double v0 = synth.sampleInput(end_t, 0);
    const double v1 = synth.sampleInput(end_t - 1 / 64.0, 0);

    EXPECT_NEAR(v0, 1023.0, 1E-6);
    EXPECT_NEAR(v1, 1022.5, 1E-6);

    synth.trimInput(0.5);

    const double v0_trim = synth.sampleInput(end_t, 0);
    const double v1_trim = synth.sampleInput(end_t - 1 / 64.0, 0);

    EXPECT_NEAR(v0, v0_trim, 1E-6);
    EXPECT_NEAR(v1, v1_trim, 1E-6);

    synth.destroy();
}
*/
