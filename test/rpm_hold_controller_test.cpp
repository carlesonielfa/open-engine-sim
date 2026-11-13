#include <gtest/gtest.h>

#include "../include/rpm_hold_controller.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

// A deliberately awkward stand-in for an engine: steep, nonlinear throttle
// response, a manifold-filling lag, rotor inertia, and the application's own
// per-frame smoothing of the speed control.
struct SyntheticEngine {
    double rpm = 900.0;
    double manifold = 0.0;
    double applied = 0.0;
    double gain = 1.0;
    double inertiaSeconds = 0.25;
    double manifoldSeconds = 0.10;
    double rpmCeiling = 1.0e9; // an overloaded engine that cannot rev past this

    double steadyStateRpm(double throttle) const {
        const double rpmAtThrottle = 600.0 + gain * 9000.0 * std::pow(std::max(0.0, throttle), 0.6);
        return std::min(rpmAtThrottle, rpmCeiling);
    }

    // The throttle that would hold `speed` with this engine.
    double throttleFor(double speed) const {
        return std::pow(std::max(0.0, (speed - 600.0) / (gain * 9000.0)), 1.0 / 0.6);
    }

    void step(double dt, double control) {
        // processEngineInput(): m_speedSetting = target * 0.5 + current * 0.5
        applied = control * 0.5 + applied * 0.5;
        manifold += (applied - manifold) * std::min(1.0, dt / manifoldSeconds);
        rpm += (steadyStateRpm(manifold) - rpm) * std::min(1.0, dt / inertiaSeconds);
    }
};

struct Result {
    double settledAtSeconds = -1.0;
    double maxOvershootRpm = 0.0;
    double worstLateErrorRpm = 0.0;
};

// Pseudo-random jitter in [-1, 1], deterministic.
double jitter(std::uint32_t &state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<double>(state >> 8) / static_cast<double>(1u << 23) - 1.0;
}

Result run(double gain, double startRpm, double target, double dt, double measurementNoiseRpm = 0.0) {
    SyntheticEngine engine;
    engine.gain = gain;
    engine.rpm = startRpm;
    engine.manifold = engine.applied = engine.throttleFor(startRpm);

    RpmHoldController controller;
    controller.engage(target, engine.applied, engine.rpm);

    Result result;
    std::uint32_t noise = 12345u;
    const double duration = 40.0;
    double time = 0.0;
    int settledFrames = 0;
    const bool rising = target > startRpm;
    while (time < duration) {
        const double measured = engine.rpm + measurementNoiseRpm * jitter(noise);
        engine.step(dt, controller.update(dt, measured));
        time += dt;
        const double error = engine.rpm - target;
        result.maxOvershootRpm = std::max(result.maxOvershootRpm, rising ? error : -error);
        if (std::abs(error) <= 25.0) {
            if (++settledFrames >= static_cast<int>(1.0 / dt) && result.settledAtSeconds < 0.0) {
                result.settledAtSeconds = time;
            }
        }
        else settledFrames = 0;
        if (time > duration - 5.0) {
            result.worstLateErrorRpm = std::max(result.worstLateErrorRpm, std::abs(error));
        }
    }
    return result;
}

}

TEST(RpmHoldController, ConvergesAcrossEngineGainsAndFrameRates) {
    for (double gain : { 0.3, 0.4, 0.7, 1.0, 1.6, 2.2, 3.0, 4.0 }) {
        for (double dt : { 1.0 / 60.0, 1.0 / 30.0, 1.0 / 20.0 }) {
            for (double start : { 900.0, 4500.0 }) {
                const Result r = run(gain, start, 2000.0, dt);
                const std::string where = "gain " + std::to_string(gain) + " dt "
                    + std::to_string(dt) + " start " + std::to_string(start);
                EXPECT_GT(r.settledAtSeconds, 0.0) << where;
                EXPECT_LT(r.settledAtSeconds, 25.0) << where;
                EXPECT_LT(r.worstLateErrorRpm, 10.0) << where;
                EXPECT_LT(r.maxOvershootRpm, 150.0) << where;
            }
        }
    }
}

TEST(RpmHoldController, StaysStableWithNoisyRpmReadings) {
    // Real engines wobble within every cycle; the hold must not chase it.
    for (double gain : { 0.5, 1.0, 2.5 }) {
        const Result r = run(gain, 900.0, 2000.0, 1.0 / 30.0, 40.0);
        EXPECT_GT(r.settledAtSeconds, 0.0) << "gain " << gain;
        EXPECT_LT(r.worstLateErrorRpm, 60.0) << "gain " << gain;
    }
}

TEST(RpmHoldController, EngagingIsBumplessAndDoesNotJumpTheThrottle) {
    RpmHoldController controller;
    controller.engage(2000.0, 0.12, 2000.0);
    EXPECT_NEAR(controller.update(1.0 / 30.0, 2000.0), 0.12, 1e-9);

    // Engaging at a different speed from the target still starts from the
    // control in use; the ramp then moves it gently.
    controller.engage(3000.0, 0.12, 1000.0);
    EXPECT_LT(controller.update(1.0 / 30.0, 1000.0), 0.2);
}

TEST(RpmHoldController, RecoversAfterLongSaturationAgainstAnOverload) {
    SyntheticEngine engine;
    engine.rpmCeiling = 1500.0; // clutch dragging: cannot reach the target
    engine.rpm = 1500.0;
    RpmHoldController controller;
    controller.engage(6000.0, 0.3, engine.rpm);
    const double dt = 1.0 / 30.0;
    for (int i = 0; i < 60 * 30; ++i) engine.step(dt, controller.update(dt, engine.rpm));
    EXPECT_LE(controller.output(), 1.0);

    // Load removed and a lower target selected: must come down and settle
    // without first unwinding a giant accumulated integral.
    engine.rpmCeiling = 1.0e9;
    controller.setTargetRpm(2000.0);
    double settledAt = -1.0;
    int onTarget = 0;
    for (int i = 0; i < 40 * 30; ++i) {
        engine.step(dt, controller.update(dt, engine.rpm));
        if (std::abs(engine.rpm - 2000.0) <= 25.0) {
            if (++onTarget >= 30 && settledAt < 0.0) settledAt = i * dt;
        }
        else onTarget = 0;
    }
    EXPECT_GT(settledAt, 0.0);
    EXPECT_LT(settledAt, 15.0);
}

TEST(RpmHoldController, RejectsGarbageInput) {
    RpmHoldController controller;
    controller.engage(2000.0, 0.2, 2000.0);
    const double before = controller.output();
    EXPECT_EQ(controller.update(0.0, 2000.0), before);
    EXPECT_EQ(controller.update(-1.0, 2000.0), before);
    EXPECT_EQ(controller.update(1.0 / 30.0, std::nan("")), before);
    controller.setTargetRpm(std::nan(""));
    EXPECT_EQ(controller.targetRpm(), 2000.0);
    controller.setTargetRpm(1.0e9);
    EXPECT_EQ(controller.targetRpm(), RpmHoldController::MaxTargetRpm);
    controller.setTargetRpm(1.0);
    EXPECT_EQ(controller.targetRpm(), RpmHoldController::MinTargetRpm);
}

TEST(RpmHoldController, ReleasedControllerKeepsItsLastOutputAndIgnoresUpdates) {
    RpmHoldController controller;
    controller.engage(2000.0, 0.2, 2000.0);
    controller.release();
    EXPECT_FALSE(controller.enabled());
    EXPECT_EQ(controller.update(1.0 / 30.0, 500.0), 0.2);
}
