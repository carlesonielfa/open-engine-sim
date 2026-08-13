#include "sdl_audio_output.h"
#include "simulator.h"
#include "synthesizer.h"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>

namespace {
class SilentSimulator final : public Simulator {
public:
    int readAudioOutput(int samples, int16_t *target) override {
        std::fill(target, target + samples, 0);
        return samples;
    }

protected:
    void writeToSynthesizer() override { }
};
}

TEST(SdlAudioOutput, ConvertsWavAndClosesDummyDevice) {
    ASSERT_TRUE(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    ASSERT_TRUE(SDL_Init(SDL_INIT_AUDIO));

    Synthesizer synthesizer;
    Synthesizer::Parameters parameters;
    parameters.inputChannelCount = 1;
    synthesizer.initialize(parameters);

    SdlAudioOutput output;
    const std::filesystem::path wav = std::filesystem::path(ENGINE_SIM_TEST_ASSET_DIRECTORY)
        / "es/sound-library/smooth/smooth_39.wav";
    EXPECT_TRUE(output.loadImpulseResponse(synthesizer, wav.string(), 1.0f, 0));

    SilentSimulator simulator;
    EXPECT_TRUE(output.start(&simulator));
    output.stop();
    output.stop();

    synthesizer.destroy();
    SDL_Quit();
}
