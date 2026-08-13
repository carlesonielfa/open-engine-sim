#ifndef OPEN_ENGINE_SIM_SDL_AUDIO_UTIL_H
#define OPEN_ENGINE_SIM_SDL_AUDIO_UTIL_H

#include <string>

class Synthesizer;

bool loadSdlImpulseResponse(
    Synthesizer &synthesizer, const std::string &path, float volume, int index,
    int sampleRate = 44100);

#endif
