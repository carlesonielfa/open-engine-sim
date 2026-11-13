#include <gtest/gtest.h>

#include "../include/latency_trim.h"
#include "../include/step_accumulator.h"

#include <algorithm>
#include <cmath>

namespace {

// Closed loop of the whole pipeline at the application's real pacing: a 1 ms
// tick, physics produced from wall time, audio consumed continuously, long
// frames whose time the 33 ms clamp throws away, and a 0.2 s input cap that
// throws audio away (the source of the audible buzz).
struct Pipeline {
    double frequency = 4500.0;
    double target = 0.14;
    bool trim = true;
    double reserve = 0.0;       // seconds of audio held
    double trimmedSeconds = 0;  // audio discarded by the input cap
    double underrunSeconds = 0; // time the device found nothing to play
    double lossEverySeconds = 1.0;
    double lossSeconds = 0.027; // physics time dropped by the dt clamp
    StepAccumulator accumulator;
    LatencyTrim controller;
    double maxReserve = 0.0;

    void run(double seconds, double biasPerTick = 0.0) {
        const double tick = 0.001;
        double nextLoss = lossEverySeconds;
        for (double t = 0.0; t < seconds; t += tick) {
            const double factor = trim ? controller.update(reserve, target, tick) : 1.0;
            const int steps = accumulator.advance(tick * factor, frequency);
            reserve += steps / frequency * (1.0 + biasPerTick);
            if (reserve > 0.2) { trimmedSeconds += reserve - 0.2; reserve = 0.2; }

            maxReserve = std::max(maxReserve, reserve);
            reserve -= tick;
            if (reserve < 0.0) { underrunSeconds += -reserve; reserve = 0.0; }

            if (lossEverySeconds > 0.0 && t >= nextLoss) {
                reserve = std::max(0.0, reserve - lossSeconds);
                nextLoss += lossEverySeconds;
            }
        }
    }
};

}

TEST(LatencyTrim, OutputIsBoundedAndQuietInsideTheDeadZone) {
    LatencyTrim controller;
    for (int i = 0; i < 100; ++i) EXPECT_EQ(controller.update(0.14, 0.14, 0.001), 1.0);
    for (int i = 0; i < 100; ++i) EXPECT_EQ(controller.update(0.14 + 0.007, 0.14, 0.001), 1.0);

    EXPECT_GT(controller.update(0.05, 0.14, 0.001), 1.0);  // short: speed up
    controller.reset();
    EXPECT_LT(controller.update(0.30, 0.14, 0.001), 1.0);  // long: slow down

    controller.reset();
    for (int i = 0; i < 100000; ++i) {
        const double f = controller.update(0.0, 0.14, 0.001);
        ASSERT_LE(f, 1.0 + LatencyTrim::MaxTrim + 1e-12);
        ASSERT_GE(f, 1.0 - LatencyTrim::MaxTrim - 1e-12);
    }
    EXPECT_LE(std::abs(controller.integral()), LatencyTrim::MaxTrim);
}

TEST(LatencyTrim, RejectsNonFiniteInputAndKeepsItsState) {
    LatencyTrim controller;
    for (int i = 0; i < 1000; ++i) controller.update(0.05, 0.14, 0.001);
    const double learned = controller.integral();
    EXPECT_EQ(controller.update(std::nan(""), 0.14, 0.001), 1.0 + learned);
    EXPECT_EQ(controller.update(0.1, std::nan(""), 0.001), 1.0 + learned);
    EXPECT_EQ(controller.update(0.1, 0.14, 0.0), 1.0 + learned);
    EXPECT_EQ(controller.update(0.1, 0.14, -1.0), 1.0 + learned);
    EXPECT_EQ(controller.integral(), learned);
}

TEST(LatencyTrim, AntiWindupLetsItRecoverQuicklyAfterALongStarvation) {
    LatencyTrim controller;
    // A minute with nothing to be done about an empty reserve (device stalled).
    for (int i = 0; i < 60000; ++i) controller.update(0.0, 0.14, 0.001);
    // Reserve is suddenly full: the output must come off the stop within
    // seconds, not after unwinding a minute of accumulated integral.
    double f = 2.0;
    for (int i = 0; i < 3000; ++i) f = controller.update(0.30, 0.14, 0.001);
    EXPECT_LT(f, 1.0);
}

TEST(LatencyTrim, ExactPacingAloneNeverRefillsAnEmptyReserve) {
    // The reason the trim exists: this is the pipeline after the step-rounding
    // fix, with no correction. After the first underrun it stays near empty,
    // which keeps the presentation guard (needs 80 ms) closed.
    Pipeline p;
    p.trim = false;
    p.reserve = 0.0;
    p.run(20.0);
    EXPECT_LT(p.reserve, 0.02);
    EXPECT_LT(p.maxReserve, 0.03); // never gets near the 80 ms guard
}

TEST(LatencyTrim, RefillsAnEmptyReserveAndHoldsItWithoutDroppingAudio) {
    Pipeline p;
    p.reserve = 0.0;
    p.run(30.0);

    EXPECT_EQ(p.trimmedSeconds, 0.0);
    EXPECT_GT(p.reserve, p.target - 0.03);
    EXPECT_LT(p.reserve, p.target + 0.03);

    // Once settled it stays settled: no underruns and no trimming.
    p.underrunSeconds = 0.0;
    p.run(30.0);
    EXPECT_EQ(p.underrunSeconds, 0.0);
    EXPECT_EQ(p.trimmedSeconds, 0.0);
    EXPECT_GT(p.reserve, 0.10); // always above the 80 ms presentation guard
}

TEST(LatencyTrim, DrainsAnOverfullReserveInsteadOfLettingTheInputCapTrimIt) {
    Pipeline p;
    p.reserve = 0.19;
    p.lossEverySeconds = 0.0;
    p.run(20.0);
    EXPECT_EQ(p.trimmedSeconds, 0.0);
    EXPECT_LT(p.reserve, p.target + 0.03);
}

TEST(LatencyTrim, AbsorbsAPhysicsClockThatRunsFastOrSlow) {
    for (double bias : { -0.02, 0.02 }) {
        Pipeline p;
        p.reserve = 0.14;
        p.lossEverySeconds = 0.0;
        p.run(90.0, bias);
        EXPECT_EQ(p.trimmedSeconds, 0.0) << "bias " << bias;
        EXPECT_EQ(p.underrunSeconds, 0.0) << "bias " << bias;
        EXPECT_GT(p.reserve, p.target - 0.02) << "bias " << bias;
        EXPECT_LT(p.reserve, p.target + 0.02) << "bias " << bias;
    }
}
