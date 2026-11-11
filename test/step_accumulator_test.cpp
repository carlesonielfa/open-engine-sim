#include <gtest/gtest.h>

#include "../include/step_accumulator.h"

#include <cmath>
#include <cstdint>

namespace {
// Frame durations in whole milliseconds, like the platform tick counter, in a
// repeating mix that resembles a phone alternating render and idle frames.
const int FrameMs[] = { 17, 33, 25, 16, 33, 17, 20, 33, 25, 12, 33, 17 };
}

TEST(StepAccumulator, LongRunStepRateIsExactAtAndroidFrequency) {
    constexpr double frequency = 4500.0;
    StepAccumulator accumulator;
    std::int64_t steps = 0;
    std::int64_t elapsedMs = 0;
    for (int i = 0; i < 200000; ++i) {
        const int ms = FrameMs[i % (sizeof(FrameMs) / sizeof(FrameMs[0]))];
        steps += accumulator.advance(ms / 1000.0, frequency);
        elapsedMs += ms;
    }

    const double expected = elapsedMs / 1000.0 * frequency;
    EXPECT_LE(std::abs(static_cast<double>(steps) - expected), 1.0);
}

TEST(StepAccumulator, PerFrameRoundingWouldHaveDrifted) {
    // Documents the defect this class replaces, so the test fails loudly if
    // someone "simplifies" the accumulator back to std::round.
    constexpr double frequency = 4500.0;
    std::int64_t rounded = 0;
    std::int64_t elapsedMs = 0;
    for (int i = 0; i < 200000; ++i) {
        const int ms = FrameMs[i % (sizeof(FrameMs) / sizeof(FrameMs[0]))];
        rounded += static_cast<std::int64_t>(std::round(ms / 1000.0 * frequency));
        elapsedMs += ms;
    }
    const double expected = elapsedMs / 1000.0 * frequency;
    EXPECT_GT(std::abs(static_cast<double>(rounded) - expected), 1000.0);
}

TEST(StepAccumulator, NeverReturnsNegativeOrNonFiniteStepCounts) {
    StepAccumulator accumulator;
    EXPECT_EQ(accumulator.advance(-1.0, 4500.0), 0);
    EXPECT_EQ(accumulator.advance(0.0, 4500.0), 0);
    EXPECT_EQ(accumulator.advance(std::nan(""), 4500.0), 0);
    EXPECT_EQ(accumulator.advance(0.01, 0.0), 0);
    EXPECT_EQ(accumulator.advance(0.01, 4500.0), 45);
}

TEST(StepAccumulator, CarriesFractionsAcrossFrames) {
    StepAccumulator accumulator;
    // 0.4 step per frame: 0, 0, 1, 0, 1, 0, 0, 1, ...
    int total = 0;
    for (int i = 0; i < 10; ++i) total += accumulator.advance(0.0004, 1000.0);
    EXPECT_EQ(total, 4);
    accumulator.reset();
    EXPECT_EQ(accumulator.remainder(), 0.0);
}
