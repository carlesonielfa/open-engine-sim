#ifndef OPEN_ENGINE_SIM_LATENCY_TRIM_H
#define OPEN_ENGINE_SIM_LATENCY_TRIM_H

#include <algorithm>
#include <cmath>

// Keeps the audio reserve (audio waiting to be rendered + audio ready to play)
// near a target by nudging how fast the physics runs, a few percent at most.
//
// With physics paced exactly to wall time (see StepAccumulator) the reserve
// never changes on its own: any time lost to a long frame, or to the per-frame
// time clamp, stays lost, and after an underrun the reserve can never refill.
// A low reserve then starves everything that waits on it (the presentation
// guard skips rendering while the reserve is short), so the frame rate
// collapses. A small correction fixes this without being audible: the input
// sample rate handed to the synthesizer is unchanged, so the pitch is
// untouched; only how far ahead of playback the simulation runs changes.
//
// It is a PI loop. Proportional action alone parks the reserve far from the
// target whenever time is lost continuously (it needs a large error to produce
// the few percent of extra speed that cancels the loss), so the integral term
// learns that steady offset. The integral stops growing while the output is
// saturated, and the dead zone stops the loop from hunting.
class LatencyTrim {
public:
    static constexpr double MaxTrim = 0.04;           // at most +/-4% physics speed
    static constexpr double DeadZoneSeconds = 0.008;  // no action inside +/-8 ms
    static constexpr double ProportionalPerSecond = 0.8;
    static constexpr double IntegralPerSecondSquared = 0.2;

    void reset() { m_integral = 0.0; }
    double integral() const { return m_integral; }

    // Multiplier for the number of physics steps to run over `dt` seconds.
    double update(double reserveSeconds, double targetSeconds, double dt) {
        if (!std::isfinite(reserveSeconds) || !std::isfinite(targetSeconds)
            || !(dt > 0.0)) {
            return 1.0 + m_integral;
        }
        dt = std::min(dt, 0.1);

        double error = targetSeconds - reserveSeconds;
        if (error > DeadZoneSeconds) error -= DeadZoneSeconds;
        else if (error < -DeadZoneSeconds) error += DeadZoneSeconds;
        else error = 0.0;

        const double proportional = ProportionalPerSecond * error;
        const double unclamped = m_integral + proportional;
        const bool saturated = (unclamped >= MaxTrim && error > 0.0)
            || (unclamped <= -MaxTrim && error < 0.0);
        if (!saturated) {
            m_integral = std::clamp(
                m_integral + IntegralPerSecondSquared * error * dt, -MaxTrim, MaxTrim);
        }

        return 1.0 + std::clamp(m_integral + proportional, -MaxTrim, MaxTrim);
    }

private:
    double m_integral = 0.0;
};

#endif /* OPEN_ENGINE_SIM_LATENCY_TRIM_H */
