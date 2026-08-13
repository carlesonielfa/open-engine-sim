#include "../include/desktop_platform_sdl.h"
#include "../include/engine_sim_application.h"
#include "../include/runtime_paths.h"
#include "../include/web_audio_output.h"
#include "../include/web_gl_renderer.h"

#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>

#include <cstdio>

extern "C" EMSCRIPTEN_KEEPALIVE void engine_sim_enable_audio();
extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_state();
extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_context_state();
extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_sample_rate();
extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_has_worklet_thread();
extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_has_node();
extern "C" EMSCRIPTEN_KEEPALIVE double engine_sim_web_audio_callback_count();
extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_peak_milli();

namespace {
struct WebRuntime {
    DesktopPlatformSdl platform;
    WebGlRenderer renderer;
    WebAudioOutput audio;
    EngineSimApplication application;
};

WebRuntime *g_runtime = nullptr;

void showAudioFailure(const char *message) {
    EM_ASM({
        const status = document.getElementById('audio-status');
        if (status) status.textContent = UTF8ToString($0);
    }, message);
}

void browserFrame(void *argument) {
    auto *runtime = static_cast<WebRuntime *>(argument);
    if (runtime->application.tick()) return;

    emscripten_cancel_main_loop();
    runtime->application.destroy();
    runtime->renderer.shutdown();
    runtime->platform.shutdown();
    delete runtime;
    g_runtime = nullptr;
}

}

extern "C" EMSCRIPTEN_KEEPALIVE void engine_sim_enable_audio() {
    if (g_runtime == nullptr) return;
    if (!g_runtime->audio.enable()) {
        showAudioFailure(g_runtime->audio.lastError());
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_state() {
    return g_runtime == nullptr ? -1 : g_runtime->audio.state();
}

extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_context_state() {
    return g_runtime == nullptr ? -1 : g_runtime->audio.contextState();
}

extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_sample_rate() {
    return g_runtime == nullptr ? 0 : g_runtime->audio.outputSampleRate();
}

extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_has_worklet_thread() {
    return g_runtime != nullptr && g_runtime->audio.hasWorkletThread();
}

extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_has_node() {
    return g_runtime != nullptr && g_runtime->audio.hasNode();
}

extern "C" EMSCRIPTEN_KEEPALIVE double engine_sim_web_audio_callback_count() {
    return g_runtime == nullptr ? 0 : static_cast<double>(g_runtime->audio.callbackCount());
}

extern "C" EMSCRIPTEN_KEEPALIVE int engine_sim_web_audio_peak_milli() {
    return g_runtime == nullptr ? 0 : g_runtime->audio.lastPeakMilli();
}

int main() {
    auto *runtime = new WebRuntime;
    if (!runtime->platform.initialize("Open Engine Simulator", 1920, 1080)) {
        std::fprintf(stderr, "SDL initialization failed: %s\n", runtime->platform.lastError().c_str());
        delete runtime;
        return 1;
    }
    if (!runtime->renderer.initialize(static_cast<SDL_Window *>(runtime->platform.nativeWindowHandle()))) {
        std::fprintf(stderr, "WebGL initialization failed: %s\n", runtime->renderer.lastError());
        runtime->platform.shutdown();
        delete runtime;
        return 1;
    }
    if (!runtime->audio.prepare()) {
        std::fprintf(stderr, "Web Audio initialization failed: %s\n", runtime->audio.lastError());
        runtime->renderer.shutdown();
        runtime->platform.shutdown();
        delete runtime;
        return 1;
    }

    const RuntimePaths paths = RuntimePaths::discover("/");
    runtime->application.initialize(&runtime->platform, &runtime->renderer, &runtime->audio, paths);
    g_runtime = runtime;
    emscripten_set_main_loop_arg(browserFrame, runtime, 0, true);
    return 0;
}
