#ifndef OPEN_ENGINE_SIM_STEP_ACCUMULATOR_H
#define OPEN_ENGINE_SIM_STEP_ACCUMULATOR_H

#include <algorithm>
#include <cmath>

// Converts variable-length frames of wall time into whole physics steps
// without creating or losing time.
//
// Rounding the step count independently every frame is biased whenever the
// frame length is not an exact multiple of the timestep: at 4.5 kHz a 17 ms
// frame is 76.5 steps and always rounds up, a 33 ms frame is 148.5 steps and
// always rounds down. The synthesizer is fed one batch of samples per step,
// so that bias drifts the audio queue until it overflows (periodic clicks) or
// runs dry (crackle). Carrying the fractional remainder keeps the long-run
// step rate exact.
class StepAccumulator {
public:
    // Returns the number of whole steps that fit in `seconds`, carrying the
    // fractional part into the next call.
    int advance(double seconds, double stepsPerSecond) {
        const double wanted = seconds * stepsPerSecond;
        if (!(wanted > 0.0)) return 0; // also rejects NaN
        m_remainder += wanted;
        const double whole = std::floor(m_remainder);
        m_remainder -= whole;
        return static_cast<int>(std::min(whole, 1.0e9));
    }

    void reset() { m_remainder = 0.0; }
    double remainder() const { return m_remainder; }

private:
    double m_remainder = 0.0;
};

#endif /* OPEN_ENGINE_SIM_STEP_ACCUMULATOR_H */
