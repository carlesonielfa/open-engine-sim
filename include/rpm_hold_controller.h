#ifndef OPEN_ENGINE_SIM_RPM_HOLD_CONTROLLER_H
#define OPEN_ENGINE_SIM_RPM_HOLD_CONTROLLER_H

#include <algorithm>
#include <cmath>

// Holds an engine at a target speed by closing the loop on the speed control
// (throttle) setting, so a finger does not have to balance the throttle.
//
// Engines differ enormously in how much rpm one unit of throttle buys (an
// order of magnitude between a small single and a big V8 near idle), so a
// fixed-gain PI either crawls or oscillates. This one adapts its integral
// gain: while the speed sits on the same side of the target it speeds up, and
// each time the speed crosses the target it backs off. The target is also
// approached along a ramp so a large step does not slam the throttle.
//
// Engaging is bumpless: the loop starts from the control value the engine is
// already using, so turning the hold on never jerks the engine.
class RpmHoldController {
public:
    static constexpr double MinTargetRpm = 300.0;
    static constexpr double MaxTargetRpm = 20000.0;

    static constexpr double Kp = 0.012;          // control per 1000 rpm of error
    static constexpr double Ki = 0.030;          // control per 1000 rpm per second
    static constexpr double MinIntegralScale = 0.25;
    static constexpr double MaxIntegralScale = 12.0;
    static constexpr double ScaleGrowthPerSecond = 0.6;
    static constexpr double ScaleShrinkOnCrossing = 0.45;
    static constexpr double TargetSlewRpmPerSecond = 2500.0;
    static constexpr double OnTargetBandRpm = 15.0;

    void setTargetRpm(double rpm) {
        if (std::isfinite(rpm)) m_targetRpm = std::clamp(rpm, MinTargetRpm, MaxTargetRpm);
    }
    double targetRpm() const { return m_targetRpm; }
    bool enabled() const { return m_enabled; }
    double output() const { return m_output; }

    // `currentControl` is the speed-control value in use right now (0..1) and
    // `currentRpm` the present engine speed.
    void engage(double targetRpm, double currentControl, double currentRpm) {
        setTargetRpm(targetRpm);
        m_integral = std::clamp(std::isfinite(currentControl) ? currentControl : 0.0, 0.0, 1.0);
        m_output = m_integral;
        m_rampedTarget = std::isfinite(currentRpm) ? currentRpm : m_targetRpm;
        m_integralScale = 1.0;
        m_side = 0;
        m_enabled = true;
    }

    void release() { m_enabled = false; }

    // Returns the speed-control value (0..1) to apply for this step.
    double update(double dt, double rpm) {
        if (!m_enabled || !(dt > 0.0) || !std::isfinite(rpm)) return m_output;
        dt = std::min(dt, 0.1);

        const double maxStep = TargetSlewRpmPerSecond * dt;
        m_rampedTarget += std::clamp(m_targetRpm - m_rampedTarget, -maxStep, maxStep);

        // Adapt the integral gain from where the speed is relative to the
        // final target, not the ramp, so the ramp itself never counts as a
        // crossing.
        const double finalError = m_targetRpm - rpm;
        const int side = std::abs(finalError) < OnTargetBandRpm ? 0 : (finalError > 0.0 ? 1 : -1);
        if (side != 0) {
            if (side == m_side) {
                m_integralScale = std::min(MaxIntegralScale,
                    m_integralScale * std::exp(ScaleGrowthPerSecond * dt));
            }
            else if (m_side != 0) {
                m_integralScale = std::max(MinIntegralScale, m_integralScale * ScaleShrinkOnCrossing);
            }
            m_side = side;
        }

        const double error = (m_rampedTarget - rpm) / 1000.0;
        const double proportional = Kp * error;
        const double unclamped = m_integral + proportional;
        const bool saturated = (unclamped >= 1.0 && error > 0.0) || (unclamped <= 0.0 && error < 0.0);
        if (!saturated) {
            m_integral = std::clamp(m_integral + Ki * m_integralScale * error * dt, 0.0, 1.0);
        }

        m_output = std::clamp(m_integral + proportional, 0.0, 1.0);
        return m_output;
    }

private:
    double m_targetRpm = 2000.0;
    double m_rampedTarget = 2000.0;
    double m_integral = 0.0;
    double m_integralScale = 1.0;
    double m_output = 0.0;
    int m_side = 0;
    bool m_enabled = false;
};

#endif /* OPEN_ENGINE_SIM_RPM_HOLD_CONTROLLER_H */
