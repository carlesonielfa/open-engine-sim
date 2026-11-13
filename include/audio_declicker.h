#ifndef OPEN_ENGINE_SIM_AUDIO_DECLICKER_H
#define OPEN_ENGINE_SIM_AUDIO_DECLICKER_H

#include <algorithm>
#include <cstdint>

// Smooths the edges of a gap in a PCM stream.
//
// When the device asks for more audio than the synthesizer has ready, the
// missing part is filled with silence. Cutting a waveform off at an arbitrary
// value, and later restarting it at another, is a step discontinuity, which is
// heard as a click or a burst of broadband crackle. This class turns each of
// those steps into a short ramp. It is allocation-free and lock-free, so it is
// safe to run inside a real-time audio callback.
class AudioDeclicker {
public:
    static constexpr int RampFrames = 96;

    // `samples` holds `frames` mono samples. The first `validFrames` are real
    // audio; the rest are silence and are overwritten with a decaying tail.
    void process(std::int16_t *samples, int validFrames, int frames) {
        if (frames <= 0) return;
        validFrames = std::max(0, std::min(validFrames, frames));

        if (validFrames > 0) {
            if (m_inGap) {
                // Resuming: blend from wherever the stream currently sits
                // into the new audio instead of jumping to it.
                const int blend = std::min(RampFrames, validFrames);
                const float start = static_cast<float>(m_lastOutput);
                for (int i = 0; i < blend; ++i) {
                    const float t = static_cast<float>(i + 1) / static_cast<float>(blend + 1);
                    samples[i] = static_cast<std::int16_t>(
                        start * (1.0f - t) + static_cast<float>(samples[i]) * t);
                }
                m_inGap = false;
            }
            m_heldSample = samples[validFrames - 1];
        }

        if (validFrames < frames) {
            if (!m_inGap) {
                m_inGap = true;
                m_decayPosition = 0;
            }
            for (int i = validFrames; i < frames; ++i) {
                if (m_decayPosition < RampFrames) {
                    ++m_decayPosition;
                    const float gain = 1.0f
                        - static_cast<float>(m_decayPosition) / static_cast<float>(RampFrames + 1);
                    samples[i] = static_cast<std::int16_t>(static_cast<float>(m_heldSample) * gain);
                }
                else {
                    samples[i] = 0;
                }
            }
        }

        m_lastOutput = samples[frames - 1];
    }

    void reset() {
        m_heldSample = 0;
        m_lastOutput = 0;
        m_decayPosition = 0;
        m_inGap = false;
    }

private:
    std::int16_t m_heldSample = 0;
    std::int16_t m_lastOutput = 0;
    int m_decayPosition = 0;
    bool m_inGap = false;
};

#endif /* OPEN_ENGINE_SIM_AUDIO_DECLICKER_H */
