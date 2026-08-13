#include "../include/sdl_audio_util.h"

#include "../include/synthesizer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <vector>

bool loadSdlImpulseResponse(
    Synthesizer &synthesizer, const std::string &path, float volume, int index,
    int sampleRate)
{
    SDL_AudioSpec source = {};
    Uint8 *wavData = nullptr;
    Uint32 wavSize = 0;
    if (!SDL_LoadWAV(path.c_str(), &source, &wavData, &wavSize)) return false;

    const SDL_AudioSpec target = { SDL_AUDIO_S16, 1, sampleRate };
    SDL_AudioStream *converter = SDL_CreateAudioStream(&source, &target);
    if (converter == nullptr) {
        SDL_free(wavData);
        return false;
    }

    const bool converted = SDL_PutAudioStreamData(
        converter, wavData, static_cast<int>(wavSize))
        && SDL_FlushAudioStream(converter);
    SDL_free(wavData);
    const int convertedBytes = converted ? SDL_GetAudioStreamAvailable(converter) : 0;
    std::vector<std::int16_t> samples(
        static_cast<std::size_t>(std::max(0, convertedBytes)) / sizeof(std::int16_t));
    const int received = samples.empty() ? 0 : SDL_GetAudioStreamData(
        converter, samples.data(), static_cast<int>(samples.size() * sizeof(std::int16_t)));
    SDL_DestroyAudioStream(converter);
    if (received <= 0) return false;

    synthesizer.initializeImpulseResponse(
        samples.data(), received / static_cast<int>(sizeof(std::int16_t)), volume, index);
    return true;
}
