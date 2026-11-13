#include <gtest/gtest.h>

#include "../include/audio_declicker.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {
constexpr double Pi = 3.14159265358979323846;

// Streams a 200 Hz sine through 512-frame blocks, with the given blocks
// partially or fully missing, and returns the largest sample-to-sample step.
int largestStep(bool declick, const std::vector<int> &validPerBlock) {
    AudioDeclicker declicker;
    std::vector<std::int16_t> out;
    long phase = 0;
    for (int valid : validPerBlock) {
        std::int16_t block[512] = {};
        for (int i = 0; i < valid; ++i) {
            block[i] = static_cast<std::int16_t>(
                20000.0 * std::sin(2.0 * Pi * 200.0 * static_cast<double>(phase + i) / 44100.0));
        }
        phase += valid;
        if (declick) declicker.process(block, valid, 512);
        out.insert(out.end(), block, block + 512);
    }
    int worst = 0;
    for (size_t i = 1; i < out.size(); ++i) {
        worst = std::max(worst, std::abs(static_cast<int>(out[i]) - static_cast<int>(out[i - 1])));
    }
    return worst;
}
}

TEST(AudioDeclickerTest, LeavesContinuousAudioUntouched) {
    AudioDeclicker declicker;
    std::int16_t block[512];
    for (int i = 0; i < 512; ++i) block[i] = static_cast<std::int16_t>(i * 10);
    std::int16_t expected[512];
    std::copy(block, block + 512, expected);
    declicker.process(block, 512, 512);
    for (int i = 0; i < 512; ++i) EXPECT_EQ(block[i], expected[i]);
}

TEST(AudioDeclickerTest, UnderrunWithoutSmoothingProducesAStep) {
    // Sanity check of the test itself: a sine at this amplitude never moves
    // more than ~600 per sample, so anything near the amplitude is a click.
    EXPECT_GT(largestStep(false, { 512, 300, 0, 512 }), 5000);
}

TEST(AudioDeclickerTest, PartialUnderrunHasNoStep) {
    EXPECT_LT(largestStep(true, { 512, 300, 512, 512 }), 1500);
}

TEST(AudioDeclickerTest, FullyMissingBlocksHaveNoStep) {
    EXPECT_LT(largestStep(true, { 512, 0, 0, 512, 512 }), 1500);
}

TEST(AudioDeclickerTest, RepeatedGapsStaySmooth) {
    EXPECT_LT(largestStep(true, { 512, 150, 512, 0, 0, 512, 200, 0, 512 }), 1500);
}

TEST(AudioDeclickerTest, TinyFragmentsStillAvoidFullScaleSteps) {
    // A fragment of only a few frames cannot be ramped over the full ramp
    // length, so the bound is looser, but it must stay far below the
    // full-scale step an unsmoothed cut produces.
    EXPECT_LT(largestStep(true, { 512, 5, 512, 0, 7, 0, 512, 200, 0, 512 }), 3500);
}
