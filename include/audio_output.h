#ifndef ATG_ENGINE_SIM_AUDIO_OUTPUT_H
#define ATG_ENGINE_SIM_AUDIO_OUTPUT_H

#include <string>

class Simulator;
class Synthesizer;

class AudioOutput {
public:
    virtual ~AudioOutput() = default;
    virtual bool start(Simulator *simulator) = 0;
    virtual bool loadImpulseResponse(Synthesizer &synthesizer, const std::string &path, float volume, int index) = 0;
    virtual void stop() = 0;
    // Desktop synthesis is historically 44.1 kHz. Browser hosts may expose a
    // different fixed device clock and override this before a simulator is
    // constructed.
    virtual int outputSampleRate() const { return 44100; }
};

#endif
