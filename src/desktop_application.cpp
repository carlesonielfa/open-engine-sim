#include "../include/engine_sim_application.h"
#if defined(__ANDROID__)
#include <csignal>
extern volatile sig_atomic_t gCrashSubstage;
extern volatile sig_atomic_t gCrashDetail;
#endif

#include "../include/combustion_chamber_object.h"
#include "../include/connecting_rod_object.h"
#include "../include/crankshaft_object.h"
#include "../include/cylinder_bank_object.h"
#include "../include/cylinder_head_object.h"
#include "../include/piston_object.h"

#if defined(ATG_ENGINE_SIM_PIRANHA_ENABLED)
#include "../scripting/include/compiler.h"
#endif

#include <algorithm>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(__ANDROID__)
#include <jni.h>
extern "C" void *SDL_GetAndroidJNIEnv(void);
extern "C" void *SDL_GetAndroidActivity(void);
#endif

namespace {
#if defined(__ANDROID__)
void openAndroidEnginePicker() {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = cls ? env->GetMethodID(cls, "openEngineFilePicker", "()V") : nullptr;
    if (method) env->CallVoidMethod(activity, method);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
}

std::string consumeAndroidEngineSelection() {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return {};
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = cls ? env->GetMethodID(cls, "consumeSelectedEngineScript", "()Ljava/lang/String;") : nullptr;
    if (!method) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (cls) env->DeleteLocalRef(cls);
        return {};
    }
    jstring value = static_cast<jstring>(env->CallObjectMethod(activity, method));
    std::string result;
    if (value) {
        const char *chars = env->GetStringUTFChars(value, nullptr);
        if (chars) {
            result = chars;
            env->ReleaseStringUTFChars(value, chars);
        }
        env->DeleteLocalRef(value);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
    return result;
}

void showAndroidMrDiagnostics(const std::string &message) {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = cls ? env->GetMethodID(cls, "showMrDiagnostics", "(Ljava/lang/String;)V") : nullptr;
    if (method) {
        jstring text = env->NewStringUTF(message.c_str());
        env->CallVoidMethod(activity, method, text);
        env->DeleteLocalRef(text);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
}

std::string saveAndroidMrLog(const std::string &message) {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return "JNI unavailable";
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = cls ? env->GetMethodID(cls, "writeMrLog", "(Ljava/lang/String;)Ljava/lang/String;") : nullptr;
    std::string result = "writeMrLog unavailable";
    if (method) {
        jstring text = env->NewStringUTF(message.c_str());
        auto returned = static_cast<jstring>(env->CallObjectMethod(activity, method, text));
        env->DeleteLocalRef(text);
        if (returned) {
            const char *chars = env->GetStringUTFChars(returned, nullptr);
            if (chars) { result = chars; env->ReleaseStringUTFChars(returned, chars); }
            env->DeleteLocalRef(returned);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
    return result;
}

void checkpointAndroidMr(const std::string &message) {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = cls ? env->GetMethodID(cls, "checkpointMrLog", "(Ljava/lang/String;)V") : nullptr;
    if (method) {
        jstring text = env->NewStringUTF(message.c_str());
        env->CallVoidMethod(activity, method, text);
        env->DeleteLocalRef(text);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
}

void hideAndroidMrDiagnostics() {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = cls ? env->GetMethodID(cls, "hideNativeDiagnostics", "()V") : nullptr;
    if (method) env->CallVoidMethod(activity, method);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
}
#endif

// Compose the engine below the camera origin independently of user pan state.
constexpr float EngineViewCompositionOffsetY = -0.12f;
}

std::string EngineSimApplication::s_buildVersion = "0.2.2";

EngineSimApplication::EngineSimApplication()
    : m_platform(nullptr), m_renderer(nullptr), m_audioOutput(nullptr), m_iceEngine(nullptr),
      m_vehicle(nullptr), m_transmission(nullptr), m_simulator(nullptr),
      m_engineView(nullptr), m_rightGaugeCluster(nullptr), m_oscCluster(nullptr),
      m_temperatureGauge(nullptr), m_performanceCluster(nullptr),
      m_loadSimulationCluster(nullptr), m_mixerCluster(nullptr), m_infoCluster(nullptr),
      m_paused(false), m_screen(0)
{
    m_background = ysColor::srgbiToLinear(0x0E1012);
    m_foreground = ysColor::srgbiToLinear(0xFFFFFF);
    m_shadow = ysColor::srgbiToLinear(0x0E1012);
    m_highlight1 = ysColor::srgbiToLinear(0xEF4545);
    m_highlight2 = ysColor::srgbiToLinear(0xFFFFFF);
    m_pink = ysColor::srgbiToLinear(0xF394BE);
    m_red = ysColor::srgbiToLinear(0xEE4445);
    m_orange = ysColor::srgbiToLinear(0xF4802A);
    m_yellow = ysColor::srgbiToLinear(0xFDBD2E);
    m_blue = ysColor::srgbiToLinear(0x77CEE0);
    m_green = ysColor::srgbiToLinear(0xBDD869);
    m_displayHeight = 0.6096f;
    m_screenWidth = m_screenHeight = 256;
    m_viewParameters = { 0, 0, 0 };
}

EngineSimApplication::~EngineSimApplication() { destroy(); }

void EngineSimApplication::initialize(
    DesktopPlatform *platform,
    Renderer *renderer,
    AudioOutput *audioOutput,
    const RuntimePaths &runtimePaths)
{
    m_platform = platform;
    m_renderer = renderer;
    m_audioOutput = audioOutput;
    m_assetPath = runtimePaths.assetDirectory.string();
    m_authoredMeshes.load((runtimePaths.assetDirectory / "authored_meshes.obj").string());
    m_textRenderer.loadFont((runtimePaths.assetDirectory / "fonts/slkscr.ttf").string());
    m_screenWidth = platform->windowWidth();
    m_screenHeight = platform->windowHeight();
    // Oscilloscopes can emit many more line segments while the starter is
    // running than during the static dashboard. Keep one shared frame arena
    // large enough that later UI panels are never silently omitted.
    m_geometryGenerator.initialize(500000, 1000000);
    m_textRenderer.setRenderCallback([this](const std::vector<TextRenderer::Run> &runs, const ysVector &color) {
        m_geometryGenerator.startShape();
        for (const TextRenderer::Run &run : runs) {
            m_geometryGenerator.generateLine2d({ run.x, run.y, run.x + run.width, run.y, run.height });
        }
        GeometryGenerator::GeometryIndices indices;
        m_geometryGenerator.endShape(&indices);
        m_shaders.SetBaseColor(color);
        drawGenerated(indices, 0x20 + m_textRenderer.renderLayer(), m_shaders.GetUiFlags());
    });
    initialize();
}

void EngineSimApplication::initialize() {
    // Scripting and audio adapters attach after the desktop lifecycle. This
    // method intentionally contains no window, renderer, or audio-device API.
    loadScript(m_currentScriptPath);
}

void EngineSimApplication::run() {
    while (tick()) {
        m_platform->delay(1);
    }
}

bool EngineSimApplication::tick() {
    if (m_platform == nullptr) return false;
    if (m_lastTick == 0) m_lastTick = m_platform->ticks();
#if defined(__EMSCRIPTEN__)
    constexpr std::uint64_t renderIntervalMs = 0;
#elif defined(__ANDROID__)
    // Present at up to ~60 Hz, but never spend more than about half of the
    // main thread on rendering: the interval follows the measured render time.
    // Physics and audio are paced from wall time, and the audio-reserve guard
    // below still postpones a presentation whenever the audio runs short.
    const std::uint64_t renderIntervalMs = static_cast<std::uint64_t>(
        std::clamp(2.0f * m_averageRenderMs, 16.0f, 50.0f));
#else
    constexpr std::uint64_t renderIntervalMs = 50;
#endif
    m_platform->pumpEvents();
    if (m_platform->shouldQuit() || m_platform->wasKeyPressed(DesktopKey::Escape)) return false;
    const std::uint64_t now = m_platform->ticks();
        // Drive physics from elapsed time so synthesis remains in step with
        // wall time and does not accumulate audio latency.
    #if defined(__ANDROID__)
    // Do not let a slow render frame create a simulation spiral: once a frame
    // takes longer, feeding all of that wall time back into physics makes the
    // next frame even more expensive. Keep audio/physics real-time sized.
    const float dt = std::min(static_cast<float>(now - m_lastTick) / 1000.0f, 1.0f / 30.0f);
#else
    const float dt = std::min(static_cast<float>(now - m_lastTick) / 1000.0f, 0.25f);
#endif
    m_lastTick = now;
    if (dt > 0.0f) {
        m_averageFramerate = 0.9f * m_averageFramerate + 0.1f / dt;
    }

    if (m_platform->wasKeyPressed(DesktopKey::F)) toggleFullscreen();
    if (m_platform->wasKeyPressed(DesktopKey::Tab)) m_screen = (m_screen + 1) % 3;
    if (m_platform->wasKeyPressed(DesktopKey::Return)) loadScript(m_currentScriptPath);
    m_screenWidth = m_platform->windowWidth();
    m_screenHeight = m_platform->windowHeight();

    if (dt > 0.0f) {
#if defined(__ANDROID__)
        if (m_postLoadProbeTicks > 0) {
            ++m_postLoadTickNumber;
            checkpointAndroidMr("POST LOAD WINDOW | tick " + std::to_string(m_postLoadTickNumber) + " | antes input");
        }
#endif
#if defined(__ANDROID__)
        if (m_postLoadProbeTicks > 0) checkpointAndroidMr("POST LOAD WINDOW | entrando processEngineInput");
#endif
#if defined(__ANDROID__)
        gCrashSubstage = 11;
#endif
        processEngineInput(dt);
#if defined(__ANDROID__)
        gCrashSubstage = 15;
#endif
#if defined(__ANDROID__)
        if (m_postLoadProbeTicks > 0 && (m_postLoadTickNumber <= 5 || (m_postLoadTickNumber % 30) == 0))
            checkpointAndroidMr("POST LOAD WINDOW | tick " + std::to_string(m_postLoadTickNumber) + " | input OK");
#endif
#if defined(__ANDROID__)
        const std::uint64_t processStart = m_platform->ticks();
#endif
#if defined(__ANDROID__)
        if (m_postLoadProbeTicks > 0) checkpointAndroidMr("POST LOAD WINDOW | entrando process");
#endif
#if defined(__ANDROID__)
        gCrashSubstage = 12;
#endif
        if (!m_paused || m_platform->wasKeyPressed(DesktopKey::Right)) process(dt);
#if defined(__ANDROID__)
        gCrashSubstage = 15;
#endif
#if defined(__ANDROID__)
        if (m_postLoadProbeTicks > 0 && (m_postLoadTickNumber <= 5 || (m_postLoadTickNumber % 30) == 0))
            checkpointAndroidMr("POST LOAD WINDOW | tick " + std::to_string(m_postLoadTickNumber) + " | process OK");
#endif
#if defined(__ANDROID__)
        m_lastProcessMs = static_cast<float>(m_platform->ticks() - processStart);
        m_processMsThisSecond += m_lastProcessMs;
#endif
    }
#if defined(__ANDROID__)
    if (m_postLoadProbeTicks > 0) checkpointAndroidMr("POST LOAD WINDOW | entrando UI update");
#endif
#if defined(__ANDROID__)
    gCrashSubstage = 13;
#endif
    if (m_engineView != nullptr) m_uiManager.update(dt);
#if defined(__ANDROID__)
    gCrashSubstage = 15;
#endif
#if defined(__ANDROID__)
    if (m_postLoadProbeTicks > 0) checkpointAndroidMr("POST LOAD WINDOW | UI update OK");
#endif
#if defined(__ANDROID__)
    // JNI polling is only needed while the Android document picker is active.
    // The previous unconditional poll crossed Java/native boundaries every tick.
    if (m_externalEnginePickerPending && m_pendingScriptPath.empty()
        && now - m_lastEnginePickerPollTick >= 100) {
        m_lastEnginePickerPollTick = now;
        const std::string importedScript = consumeAndroidEngineSelection();
        if (!importedScript.empty()) {
            m_pendingScriptPath = importedScript;
            m_externalEnginePickerPending = false;
            m_mrDiagnosticMessage = "MR 1/3 SELECTED | " + importedScript;
            if (m_infoCluster != nullptr) m_infoCluster->setLogMessage(m_mrDiagnosticMessage);
            m_mrDiagnosticUntilTick = now + 30000;
        }
    }
#endif
    if (!m_pendingScriptPath.empty()) {
        const std::string selectedScript = m_pendingScriptPath;
        m_pendingScriptPath.clear();
#if defined(__ANDROID__)
        m_mrDiagnosticMessage = "MR 2/3 COMPILING | " + selectedScript;
        if (m_infoCluster != nullptr) m_infoCluster->setLogMessage(m_mrDiagnosticMessage);
        m_mrDiagnosticUntilTick = now + 30000;
#else
        if (m_infoCluster != nullptr) m_infoCluster->setLogMessage("MR loading: " + selectedScript);
#endif
        if (loadScript(selectedScript)) {
            m_currentScriptPath = selectedScript;
#if defined(__ANDROID__)
            m_mrDiagnosticMessage = "MR 3/3 LOADED OK | " + selectedScript;
            if (m_infoCluster != nullptr) m_infoCluster->setLogMessage(m_mrDiagnosticMessage);
            m_mrDiagnosticUntilTick = now + 30000;
#else
            if (m_infoCluster != nullptr) m_infoCluster->setLogMessage("MR loaded: " + selectedScript);
#endif
        }
    }
    // Rendering is substantially more expensive than the audio-producing
    // simulation on the SDL path. Reserve CPU slices for simulation so
    // the audio stream is never starved by presentation work.
    bool renderAllowed = true;
#if defined(__ANDROID__)
    // Audio-first scheduler: rendering is the largest burst of work on mobile.
    // If the convolution worker's ready-to-play reservoir is getting low,
    // postpone this presentation only. The next tight loop iteration continues
    // physics/input production immediately, allowing audio to recover without
    // changing simulation frequency or sound quality.
    if (m_simulator != nullptr) {
        constexpr double audioRenderGuardSeconds = 0.080;
        const double audioReady = m_simulator->getSynthesizerOutputLatency();
        if (audioReady < audioRenderGuardSeconds) renderAllowed = false;
    }
#endif
    if (renderAllowed && now - m_lastRenderTick >= renderIntervalMs) {
#if defined(__ANDROID__)
        const std::uint64_t renderStart = m_platform->ticks();
#endif
#if defined(__ANDROID__)
        if (m_postLoadProbeTicks > 0) checkpointAndroidMr("POST LOAD WINDOW | entrando renderScene");
#endif
#if defined(__ANDROID__)
        gCrashSubstage = 14;
#endif
        renderScene();
#if defined(__ANDROID__)
        gCrashSubstage = 15;
#endif
#if defined(__ANDROID__)
        if (m_postLoadProbeTicks > 0) checkpointAndroidMr("POST LOAD WINDOW | renderScene OK");
#endif
#if defined(__ANDROID__)
        m_lastRenderMs = static_cast<float>(m_platform->ticks() - renderStart);
        m_averageRenderMs = 0.9f * m_averageRenderMs + 0.1f * m_lastRenderMs;
        m_renderMsThisSecond += m_lastRenderMs;
        ++m_rendersThisSecond;
#endif
        m_lastRenderTick = now;
    }
#if defined(__ANDROID__)
    if (m_postLoadProbeTicks > 0) {
        --m_postLoadProbeTicks;
        if (m_postLoadFirstProcessDone && m_postLoadFirstRenderDone &&
            (m_postLoadTickNumber <= 5 || (m_postLoadTickNumber % 30) == 0)) {
            checkpointAndroidMr("POST LOAD WINDOW | tick " + std::to_string(m_postLoadTickNumber) +
                " | process + render continuam OK");
        }
        if (m_postLoadProbeTicks == 0) {
            checkpointAndroidMr("POST LOAD WINDOW | 300 ticks estaveis");
            hideAndroidMrDiagnostics();
        }
    }
        if (m_infoCluster != nullptr && now < m_mrDiagnosticUntilTick && !m_mrDiagnosticMessage.empty()) {
        m_infoCluster->setLogMessage(m_mrDiagnosticMessage);
    }
    if (m_infoCluster != nullptr && now - m_lastPerfReportTick >= 1000 && !m_externalEnginePickerPending && m_pendingScriptPath.empty() && now >= m_mrDiagnosticUntilTick) {
        const float elapsedMs = std::max(1.0f, static_cast<float>(now - m_lastPerfReportTick));
        m_lastPerfReportTick = now;
        const int simHz = m_simulator != nullptr
            ? static_cast<int>(m_simulator->getSimulationFrequency()) : 0;
        const int reserveMs = m_simulator != nullptr
            ? static_cast<int>(1000.0 * (m_simulator->getSynthesizerInputLatency()
                + m_simulator->getSynthesizerOutputLatency())) : 0;
        const int fps = static_cast<int>(std::lround(m_rendersThisSecond * 1000.0f / elapsedMs));
        const int physicsPercent = static_cast<int>(100.0f * m_processMsThisSecond / elapsedMs);
        const int renderPercent = static_cast<int>(100.0f * m_renderMsThisSecond / elapsedMs);
        m_rendersThisSecond = 0;
        m_processMsThisSecond = 0.0f;
        m_renderMsThisSecond = 0.0f;
        m_infoCluster->setLogMessage(
            "ANDROID PERF | " + std::to_string(fps) + " fps | FISICA " + std::to_string(physicsPercent) +
            "% | RENDER " + std::to_string(renderPercent) + "% | AUDIO " + std::to_string(reserveMs) +
            "ms | " + std::to_string(simHz) + "Hz");
    }
#endif
    return true;
}

void EngineSimApplication::destroy() {
    if (m_audioOutput != nullptr) {
        m_audioOutput->stop();
        // The desktop entry point explicitly calls destroy(), then this class'
        // destructor calls it again. The adapter is stack-owned by that entry
        // point, so do not retain a stale pointer across the second pass.
        m_audioOutput = nullptr;
    }
    destroyObjects();
    m_uiManager.destroy();
    m_engineView = nullptr;
    m_rightGaugeCluster = nullptr;
    m_oscCluster = nullptr;
    m_performanceCluster = nullptr;
    m_loadSimulationCluster = nullptr;
    m_mixerCluster = nullptr;
    m_infoCluster = nullptr;
    if (m_simulator != nullptr) { m_simulator->releaseSimulation(); delete m_simulator; m_simulator = nullptr; }
    if (m_iceEngine != nullptr) { m_iceEngine->destroy(); delete m_iceEngine; m_iceEngine = nullptr; }
    delete m_vehicle; m_vehicle = nullptr;
    delete m_transmission; m_transmission = nullptr;
    m_geometryGenerator.destroy();
}

void EngineSimApplication::process(float dt) {
    if (m_simulator == nullptr) return;
#if defined(__ANDROID__)
    const bool probe = m_postLoadProbeTicks > 0 && !m_postLoadFirstProcessDone;
    if (probe) checkpointAndroidMr("POST LOAD | PROCESS 01 entrando startFrame");
#endif
    m_simulator->startFrame(dt);
#if defined(__ANDROID__)
    if (probe) checkpointAndroidMr("POST LOAD | PROCESS 02 startFrame OK; entrando simulateStep");
#endif
    while (m_simulator->simulateStep()) {
        if (m_oscCluster != nullptr) m_oscCluster->sample();
    }
#if defined(__ANDROID__)
    if (probe) checkpointAndroidMr("POST LOAD | PROCESS 03 simulateStep OK; entrando endFrame");
#endif
    m_simulator->endFrame();
#if defined(__ANDROID__)
    if (probe) {
        checkpointAndroidMr("POST LOAD | PROCESS 04 primeiro ciclo de simulacao OK");
        m_postLoadFirstProcessDone = true;
    }
#endif
}

void EngineSimApplication::render() {
#if defined(__ANDROID__)
    gCrashDetail = 141;
#endif
    for (SimulationObject *object : m_objects) object->generateGeometry();
#if defined(__ANDROID__)
    gCrashDetail = 142;
#endif
    for (int sublayer = 0; sublayer < 3; ++sublayer) {
        m_viewParameters.Sublayer = sublayer;
        for (SimulationObject *object : m_objects) object->render(&m_viewParameters);
    }
#if defined(__ANDROID__)
    gCrashDetail = 143;
#endif
    if (m_engineView != nullptr) m_uiManager.render();
#if defined(__ANDROID__)
    gCrashDetail = 144;
#endif
}

void EngineSimApplication::renderScene() {
#if defined(__ANDROID__)
    const bool probe = m_postLoadProbeTicks > 0 && !m_postLoadFirstRenderDone;
    if (probe) checkpointAndroidMr("POST LOAD | RENDER 01 entrando beginFrame");
#endif
#if defined(__ANDROID__)
    gCrashDetail = 145;
#endif
    m_renderer->beginFrame(m_shadow);
#if defined(__ANDROID__)
    if (probe) checkpointAndroidMr("POST LOAD | RENDER 02 beginFrame OK; gerando geometria/UI");
#endif
#if defined(__ANDROID__)
    gCrashDetail = 146;
#endif
    m_geometryGenerator.reset();
    if (m_engineView != nullptr) {
        const Bounds windowBounds(static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight),
            { 0.0f, static_cast<float>(m_screenHeight) });
        if (m_screen == 0) {
            Grid dashboard = { 3, 2 };
            Grid dashboardThirds = { 3, 3 };
            Grid topStack = { 1, 3 };
            m_engineView->setDrawFrame(true);
            m_engineView->setBounds(dashboard.get(windowBounds, 1, 0));
            m_rightGaugeCluster->m_bounds = dashboard.get(windowBounds, 2, 0, 1, 2);
            m_oscCluster->m_bounds = dashboard.get(windowBounds, 1, 1);
            m_performanceCluster->m_bounds = dashboardThirds.get(windowBounds, 0, 1);
            m_loadSimulationCluster->m_bounds = dashboardThirds.get(windowBounds, 0, 2);
            const Bounds upperLeft = dashboardThirds.get(windowBounds, 0, 0);
            m_mixerCluster->m_bounds = topStack.get(upperLeft, 0, 2);
            m_infoCluster->m_bounds = topStack.get(upperLeft, 0, 0, 1, 2);
            m_engineView->setVisible(true);
            m_rightGaugeCluster->setVisible(true);
            m_oscCluster->setVisible(true);
            m_performanceCluster->setVisible(true);
            m_loadSimulationCluster->setVisible(true);
            m_mixerCluster->setVisible(true);
            m_infoCluster->setVisible(true);
        }
        else if (m_screen == 1) {
            m_engineView->setDrawFrame(false);
            m_engineView->setBounds(windowBounds);
            m_engineView->setVisible(true);
            m_rightGaugeCluster->setVisible(false);
            m_oscCluster->setVisible(false);
            m_performanceCluster->setVisible(false);
            m_loadSimulationCluster->setVisible(false);
            m_mixerCluster->setVisible(false);
            m_infoCluster->setVisible(false);
        }
        else {
            Grid split = { 3, 1 };
            m_engineView->setDrawFrame(true);
            m_engineView->setBounds(split.get(windowBounds, 0, 0, 2, 1));
            m_rightGaugeCluster->m_bounds = split.get(windowBounds, 2, 0);
            m_engineView->setVisible(true);
            m_rightGaugeCluster->setVisible(true);
            m_oscCluster->setVisible(false);
            m_performanceCluster->setVisible(false);
            m_loadSimulationCluster->setVisible(false);
            m_mixerCluster->setVisible(false);
            m_infoCluster->setVisible(false);
        }
        const Point cameraPosition = m_engineView->getCameraPosition();
        m_shaders.m_cameraPosition = ysMath::LoadVector(cameraPosition.x, cameraPosition.y, 0.0f, 1.0f);
        m_shaders.CalculateUiCamera(static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight));
        const float cameraAspect = m_engineView->m_bounds.width() / m_engineView->m_bounds.height();
        m_shaders.CalculateCamera(cameraAspect * m_displayHeight / m_engineView->m_zoom,
            m_displayHeight / m_engineView->m_zoom, m_engineView->m_bounds,
            static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight), m_displayAngle);
        // Bounds are expressed from the bottom of the UI coordinate system;
        // SDL GPU viewports are expressed from the top of the drawable.
        m_renderer->setSceneViewport(
            m_engineView->m_bounds.left(),
            static_cast<float>(m_screenHeight) - m_engineView->m_bounds.top(),
            m_engineView->m_bounds.width(),
            m_engineView->m_bounds.height());
#if defined(__ANDROID__)
        gCrashDetail = 147;
#endif
        render();
    }
#if defined(__ANDROID__)
    if (probe) checkpointAndroidMr("POST LOAD | RENDER 03 geometria/UI OK; enviando GPU");
#endif
#if defined(__ANDROID__)
    gCrashDetail = 148;
#endif
    m_renderer->uploadGeometry(m_geometryGenerator.getVertexData(), m_geometryGenerator.getCurrentVertexCount(),
        m_geometryGenerator.getIndexData(), m_geometryGenerator.getCurrentIndexCount());
#if defined(__ANDROID__)
    if (probe) checkpointAndroidMr("POST LOAD | RENDER 04 uploadGeometry OK; entrando endFrame");
#endif
#if defined(__ANDROID__)
    gCrashDetail = 149;
#endif
    m_renderer->endFrame();
#if defined(__ANDROID__)
    if (probe) {
        checkpointAndroidMr("POST LOAD | RENDER 05 primeiro frame OK");
        m_postLoadFirstRenderDone = true;
    }
#endif
}

void EngineSimApplication::drawGenerated(const GeometryGenerator::GeometryIndices &indices, int layer) {
    drawGenerated(indices, layer, m_shaders.GetRegularFlags());
}
void EngineSimApplication::drawGeneratedUi(const GeometryGenerator::GeometryIndices &indices, int layer) {
    drawGenerated(indices, layer, m_shaders.GetUiFlags());
}
void EngineSimApplication::drawGenerated(
    const GeometryGenerator::GeometryIndices &indices, int layer, Shaders::StageEnableFlags flags)
{
    if (indices.FaceCount <= 0) return;
    const Shaders::ScreenVariables &screen = (flags == m_shaders.GetUiFlags())
        ? m_shaders.m_uiScreenVariables : m_shaders.m_screenVariables;
    ysMatrix transform = m_shaders.m_objectVariables.Transform;
    if ((flags & Shaders::SceneStage) != 0) {
        transform = ysMath::MatMult(
            ysMath::TranslationTransform(ysMath::LoadVector(0.0f, EngineViewCompositionOffsetY, 0.0f)),
            transform);
    }
    m_renderer->submitGeometry(m_geometryGenerator.getVertexData(), m_geometryGenerator.getIndexData(),
        indices.BaseVertex, indices.BaseIndex, indices.FaceCount,
        transform, screen.CameraView, screen.Projection,
        m_shaders.m_objectVariables.BaseColor, flags, layer);
}
void EngineSimApplication::drawModel(
    const std::string &modelName,
    Shaders::StageEnableFlags flags,
    int layer)
{
    if (const AuthoredMeshLibrary::Mesh *mesh = m_authoredMeshes.find(modelName)) {
        GeometryGenerator::GeometryIndices imported;
        m_geometryGenerator.startShape();
        m_geometryGenerator.generateRawMesh(mesh->vertices.data(), static_cast<int>(mesh->vertices.size()),
            mesh->indices.data(), static_cast<int>(mesh->indices.size()));
        m_geometryGenerator.endShape(&imported);
        drawGenerated(imported, layer, flags);
    }
}

float EngineSimApplication::pixelsToUnits(float pixels) const {
    return m_engineView == nullptr ? pixels : pixels * m_displayHeight / m_engineView->m_bounds.height();
}
float EngineSimApplication::unitsToPixels(float units) const {
    return m_engineView == nullptr ? units : units * m_engineView->m_bounds.height() / m_displayHeight;
}
const SimulationObject::ViewParameters &EngineSimApplication::getViewParameters() const { return m_viewParameters; }

void EngineSimApplication::configure(const ApplicationSettings &settings) {
    m_applicationSettings = settings;
    m_background = ysColor::srgbiToLinear(settings.colorBackground);
    m_foreground = ysColor::srgbiToLinear(settings.colorForeground);
    m_shadow = ysColor::srgbiToLinear(settings.colorShadow);
    m_highlight1 = ysColor::srgbiToLinear(settings.colorHighlight1);
    m_highlight2 = ysColor::srgbiToLinear(settings.colorHighlight2);
    m_pink = ysColor::srgbiToLinear(settings.colorPink);
    m_red = ysColor::srgbiToLinear(settings.colorRed);
    m_orange = ysColor::srgbiToLinear(settings.colorOrange);
    m_yellow = ysColor::srgbiToLinear(settings.colorYellow);
    m_blue = ysColor::srgbiToLinear(settings.colorBlue);
    m_green = ysColor::srgbiToLinear(settings.colorGreen);
    if (settings.startFullscreen) m_platform->setFullscreen(true);
}

void EngineSimApplication::requestEngineScript(const std::string &relativeScriptPath) {
    if (!relativeScriptPath.empty()) m_pendingScriptPath = relativeScriptPath;
}

bool EngineSimApplication::loadScript(const std::string &relativeScriptPath) {
#if defined(ATG_ENGINE_SIM_PIRANHA_ENABLED)
    Engine *engine = nullptr;
    Vehicle *vehicle = nullptr;
    Transmission *transmission = nullptr;
    const std::filesystem::path scriptPath = std::filesystem::path(m_assetPath) / relativeScriptPath;
#if defined(__ANDROID__)
    if (!std::filesystem::exists(scriptPath)) {
        if (m_infoCluster != nullptr) m_infoCluster->setLogMessage("MR missing: " + scriptPath.string());
        return false;
    }
#endif

    // External .mr files come in two common forms:
    //  1) complete entry points that already call main();
    //  2) engine modules that only define public node main (catalog style).
    // Try the file exactly as authored first. If it does not produce the three
    // simulator objects, fall back to the catalog wrapper used by packaged engines.
#if defined(__ANDROID__)
    std::ostringstream mrTrace;
    mrTrace << "ARQUIVO: " << relativeScriptPath << "\n";
    mrTrace << "CAMINHO: " << scriptPath.string() << "\n";
    mrTrace << "EXISTE: " << (std::filesystem::exists(scriptPath) ? "SIM" : "NAO") << "\n";
    std::error_code mrSizeError;
    const auto mrSize = std::filesystem::file_size(scriptPath, mrSizeError);
    mrTrace << "TAMANHO: " << (mrSizeError ? 0 : mrSize) << " bytes\n\n";
#endif

    auto compileEntry = [&](const std::filesystem::path &entryPoint) -> bool {
        es_script::Compiler compiler;
        compiler.initialize(m_assetPath);
#if defined(__ANDROID__)
        mrTrace << "COMPILANDO: " << entryPoint.string() << "\n";
        checkpointAndroidMr(mrTrace.str() + "\nCHECKPOINT: entrando em compiler.compile()");
#endif
        const bool compiled = compiler.compile(entryPoint.string());
#if defined(__ANDROID__)
        mrTrace << "compile(): " << (compiled ? "OK" : "FALHOU") << "\n";
        if (!compiled) {
            const std::string compilerErrors = compiler.getLastErrorText();
            mrTrace << "ERRO PIRANHA:\n" << (compilerErrors.empty() ? "(sem texto de erro)" : compilerErrors) << "\n";
        }
#endif
        if (compiled) {
#if defined(__ANDROID__)
            checkpointAndroidMr(mrTrace.str() + "\nCHECKPOINT: compile OK; entrando em compiler.execute()");
#endif
            const es_script::Compiler::Output output = compiler.execute();
#if defined(__ANDROID__)
            mrTrace << "execute(): CONCLUIDO\n";
            mrTrace << "ENGINE: " << (output.engine ? "OK" : "NULL") << "\n";
            mrTrace << "VEHICLE: " << (output.vehicle ? "OK" : "NULL") << "\n";
            mrTrace << "TRANSMISSION: " << (output.transmission ? "OK" : "NULL") << "\n";
#endif
#if defined(__ANDROID__)
            checkpointAndroidMr(mrTrace.str() + "\nCHECKPOINT: execute retornou; validando objetos");
#endif
            if (output.engine != nullptr) {
                configure(output.applicationSettings);
                engine = output.engine;
                vehicle = output.vehicle;
                transmission = output.transmission;

#if defined(__ANDROID__)
                // Many community .mr files are engine-only modules. Desktop
                // catalog entries normally provide a vehicle and transmission,
                // but requiring those objects makes otherwise valid engines
                // impossible to import on a phone. Supply conservative host
                // defaults only for the missing pieces.
                if (vehicle == nullptr) {
                    vehicle = new Vehicle;
                    Vehicle::Parameters params{};
                    params.mass = 1400.0;
                    params.dragCoefficient = 0.32;
                    params.crossSectionArea = 2.0;
                    params.diffRatio = 3.73;
                    params.tireRadius = 0.31;
                    params.rollingResistance = 180.0;
                    vehicle->initialize(params);
                }
                if (transmission == nullptr) {
                    static const double ratios[] = { 3.60, 2.19, 1.41, 1.00, 0.83 };
                    transmission = new Transmission;
                    Transmission::Parameters params{};
                    params.GearCount = 5;
                    params.GearRatios = ratios;
                    params.MaxClutchTorque = 680.0;
                    transmission->initialize(params);
                }
#endif
            }
        }
        compiler.destroy();
        return engine != nullptr && vehicle != nullptr && transmission != nullptr;
    };

    // Packaged main.mr is an entry point. Imported engines are catalog-style modules:
    // compiling/executing them directly creates no objects and adds avoidable allocation churn.
    const bool isUserEngine = relativeScriptPath.rfind("engines/user/", 0) == 0;
    bool loaded = isUserEngine ? false : compileEntry(scriptPath);
    std::filesystem::path generatedEntryPoint;
    if (!loaded && relativeScriptPath != "main.mr") {
#if defined(__ANDROID__)
        // Keep the wrapper inside the compiler's asset root and import using
        // the same asset-relative syntax as every bundled engine. Piranha's
        // Android file resolver is rooted at m_assetPath; feeding it a cache
        // file that imports an absolute /data/user/... path makes valid
        // external scripts fail before main() can be executed.
        generatedEntryPoint = std::filesystem::path(m_assetPath) / "user-engine-entry.mr";
        std::ofstream entryPoint(generatedEntryPoint);
        entryPoint << "import \"" << relativeScriptPath << "\"\n\nmain()\n";
#else
        generatedEntryPoint = std::filesystem::temp_directory_path() / "engine-sim-picker-entry.mr";
        std::ofstream entryPoint(generatedEntryPoint);
        entryPoint << "import \"" << scriptPath.generic_string() << "\"\n\nmain()\n";
#endif
        entryPoint.close();
        loaded = compileEntry(generatedEntryPoint);
    }
    if (!generatedEntryPoint.empty()) {
        std::error_code ignored;
        std::filesystem::remove(generatedEntryPoint, ignored);
    }

    if (loaded) {
#if defined(__ANDROID__)
        checkpointAndroidMr(mrTrace.str() + "\nCHECKPOINT: objetos validos; entrando em loadEngine()");
#endif
        loadEngine(engine, vehicle, transmission);
#if defined(__ANDROID__)
        m_postLoadProbeTicks = 300;
        m_postLoadTickNumber = 0;
        m_postLoadFirstProcessDone = false;
        m_postLoadFirstRenderDone = false;
        checkpointAndroidMr(mrTrace.str() + "\nCHECKPOINT: loadEngine() CONCLUIDO; iniciando probe pos-load");
        const std::string mrLogPath = saveAndroidMrLog(mrTrace.str() + "\nLOAD ENGINE: CONCLUIDO\n");
        showAndroidMrDiagnostics(mrTrace.str() + "\nLOAD ENGINE: CONCLUIDO\nLOG: " + mrLogPath);
#endif
        return true;
    }
    if (m_infoCluster != nullptr) {
        std::string detail = "Engine script failed to load";
        std::ifstream errorLog("error_log.log");
        if (errorLog) {
            std::ostringstream errors;
            errors << errorLog.rdbuf();
            std::string message = errors.str();
            if (!message.empty()) {
                std::replace(message.begin(), message.end(), '\n', ' ');
                if (message.size() > 180) message.resize(180);
                detail += " | " + message;
            }
        }
#if defined(__ANDROID__)
        mrTrace << "\nRESULTADO: FALHOU\n";
        mrTrace << "MENSAGEM: " << detail << "\n";
        mrTrace << "\nSe compile() falhou, o proximo passo e capturar o erro interno do Piranha.";
        const std::string mrLogPath = saveAndroidMrLog(mrTrace.str());
        mrTrace << "\nLOG SALVO EM:\n" << mrLogPath << "\n";
        showAndroidMrDiagnostics(mrTrace.str());
        m_mrDiagnosticMessage = "MR ERROR | " + detail;
        m_mrDiagnosticUntilTick = m_platform != nullptr ? m_platform->ticks() + 30000 : 30000;
        m_infoCluster->setLogMessage(m_mrDiagnosticMessage);
#else
        m_infoCluster->setLogMessage(detail);
#endif
    }
#endif
    return false;
}
void EngineSimApplication::processEngineInput(float dt) {
    if (m_iceEngine == nullptr || m_simulator == nullptr) return;

    const bool fineControl = m_platform->isKeyDown(DesktopKey::Space);
    const float wheel = m_platform->mouseWheelY();
    bool consumedWheel = false;
    auto log = [this](const std::string &message) {
        if (m_infoCluster != nullptr) m_infoCluster->setLogMessage(message);
    };
    auto updateAudio = [&](auto update, const char *message) {
        Synthesizer::AudioParameters parameters = m_simulator->synthesizer().getAudioParameters();
        update(parameters);
        m_simulator->synthesizer().setAudioParameters(parameters);
        consumedWheel = true;
        log(message);
    };
    if (m_platform->isKeyDown(DesktopKey::Z)) {
        updateAudio([&](auto &p) { p.volume = clamp(p.volume + wheel * (fineControl ? 0.001 : 0.01) * dt); }, "[Z] - volume");
    } else if (m_platform->isKeyDown(DesktopKey::X)) {
        updateAudio([&](auto &p) { p.convolution = clamp(p.convolution + wheel * (fineControl ? 0.001 : 0.01) * dt); }, "[X] - convolution");
    } else if (m_platform->isKeyDown(DesktopKey::C)) {
        updateAudio([&](auto &p) { p.dF_F_mix = clamp(p.dF_F_mix + wheel * (fineControl ? 0.00001 : 0.001) * dt); }, "[C] - high frequency gain");
    } else if (m_platform->isKeyDown(DesktopKey::V)) {
        updateAudio([&](auto &p) { p.airNoise = clamp(p.airNoise + wheel * (fineControl ? 0.001 : 0.01) * dt); }, "[V] - low frequency noise");
    } else if (m_platform->isKeyDown(DesktopKey::B)) {
        updateAudio([&](auto &p) { p.inputSampleNoise = clamp(p.inputSampleNoise + wheel * (fineControl ? 0.001 : 0.01) * dt); }, "[B] - high frequency noise");
    } else if (m_platform->isKeyDown(DesktopKey::N)) {
        m_simulator->setSimulationFrequency(clamp(m_simulator->getSimulationFrequency()
            + wheel * (fineControl ? 10.0 : 100.0) * dt, 400.0, 400000.0));
        consumedWheel = true;
    } else if (m_platform->isKeyDown(DesktopKey::G) && m_simulator->m_dyno.m_hold) {
        m_dynoSpeed = clamp(m_dynoSpeed + wheel * m_iceEngine->getDynoHoldStep(),
            m_iceEngine->getDynoMinSpeed(), m_iceEngine->getDynoMaxSpeed());
        consumedWheel = true;
    }

    m_targetSpeedSetting = fineControl ? m_targetSpeedSetting : 0.0;
    if (m_platform->isKeyDown(DesktopKey::Q)) m_targetSpeedSetting = 0.01;
    else if (m_platform->isKeyDown(DesktopKey::W)) m_targetSpeedSetting = 0.1;
    else if (m_platform->isKeyDown(DesktopKey::E)) m_targetSpeedSetting = 0.2;
    else if (m_platform->isKeyDown(DesktopKey::R)) m_targetSpeedSetting = 1.0;
    else if (fineControl && !consumedWheel) m_targetSpeedSetting = clamp(m_targetSpeedSetting + wheel * 0.0001);
    if (m_touchThrottleHeld) m_targetSpeedSetting = m_touchThrottle;
    if (m_rpmHold.enabled()) {
        const bool throttleKeyDown = m_platform->isKeyDown(DesktopKey::Q)
            || m_platform->isKeyDown(DesktopKey::W)
            || m_platform->isKeyDown(DesktopKey::E)
            || m_platform->isKeyDown(DesktopKey::R);
        const double rpm = m_iceEngine->getRpm();
        // A stalled engine must not leave the throttle wide open waiting for
        // a speed it can never reach.
        m_rpmHoldStallSeconds = rpm < 120.0 ? m_rpmHoldStallSeconds + dt : 0.0;
        if (throttleKeyDown || m_rpmHoldStallSeconds > 1.5) {
            m_rpmHold.release();
            m_rpmHoldStallSeconds = 0.0;
            log("RPM hold off");
        }
        else {
            m_targetSpeedSetting = m_rpmHold.update(dt, rpm);
        }
    }
    m_speedSetting = m_targetSpeedSetting * 0.5 + m_speedSetting * 0.5;
    m_iceEngine->setSpeedControl(m_speedSetting);

    if (m_platform->wasKeyPressed(DesktopKey::M) && m_viewParameters.Layer0 + 1 < m_iceEngine->getMaxDepth()) ++m_viewParameters.Layer0;
    if (m_platform->wasKeyPressed(DesktopKey::Comma) && m_viewParameters.Layer0 > 0) --m_viewParameters.Layer0;
    if (m_platform->wasKeyPressed(DesktopKey::D)) toggleDynamometer();
    if (m_platform->wasKeyPressed(DesktopKey::H)) toggleDynamometerHold();
    if (m_platform->wasKeyPressed(DesktopKey::A)) toggleIgnition();
    if (m_platform->wasKeyPressed(DesktopKey::Up)) changeGear(1);
    if (m_platform->wasKeyPressed(DesktopKey::Down)) changeGear(-1);

    if (m_platform->isKeyDown(DesktopKey::T)) m_targetClutchPressure -= 0.2 * dt;
    else if (m_platform->isKeyDown(DesktopKey::U)) m_targetClutchPressure += 0.2 * dt;
    else if (m_platform->isKeyDown(DesktopKey::Space)) m_targetClutchPressure = 0.0;
    else if (!m_platform->isKeyDown(DesktopKey::Y)) m_targetClutchPressure = 1.0;
    m_targetClutchPressure = clamp(m_targetClutchPressure);
    const double clutchRC = fineControl ? 1.0 : 0.001;
    const double clutchS = dt / (dt + clutchRC);
    m_clutchPressure = m_clutchPressure * (1 - clutchS) + m_targetClutchPressure * clutchS;
    m_simulator->getTransmission()->setClutchPressure(m_clutchPressure);

    if (m_simulator->m_dyno.m_enabled && !m_simulator->m_dyno.m_hold) {
        m_dynoSpeed = m_simulator->getFilteredDynoTorque() > units::torque(1.0, units::ft_lb)
            ? m_dynoSpeed + units::rpm(500) * dt : m_dynoSpeed / (1 + dt);
    } else if (!m_simulator->m_dyno.m_hold) m_dynoSpeed = units::rpm(0);
    m_simulator->m_dyno.m_rotationSpeed = clamp(m_dynoSpeed,
        m_iceEngine->getDynoMinSpeed(), m_iceEngine->getDynoMaxSpeed());
    m_simulator->m_starterMotor.m_enabled = m_platform->isKeyDown(DesktopKey::S) || m_touchStarterHeld;
}

void EngineSimApplication::toggleIgnition() {
    if (m_simulator == nullptr || m_simulator->getEngine() == nullptr) return;
    auto *ignition = m_simulator->getEngine()->getIgnitionModule();
    ignition->m_enabled = !ignition->m_enabled;
}

void EngineSimApplication::toggleDynamometer() {
    if (m_simulator != nullptr) m_simulator->m_dyno.m_enabled = !m_simulator->m_dyno.m_enabled;
}

void EngineSimApplication::toggleDynamometerHold() {
    if (m_simulator != nullptr) m_simulator->m_dyno.m_hold = !m_simulator->m_dyno.m_hold;
}

void EngineSimApplication::changeGear(int direction) {
    if (m_simulator == nullptr || m_simulator->getTransmission() == nullptr) return;
    Transmission *transmission = m_simulator->getTransmission();
    transmission->changeGear(transmission->getGear() + direction);
}

void EngineSimApplication::toggleRpmHold() {
    if (m_iceEngine == nullptr || m_simulator == nullptr) return;
    if (m_rpmHold.enabled()) {
        m_rpmHold.release();
        if (m_infoCluster != nullptr) m_infoCluster->setLogMessage("RPM hold off");
        return;
    }

    // Hold whatever the engine is doing right now; the target is then
    // adjusted by dragging. Round to the same 25 rpm steps the drag uses.
    const double rpm = m_iceEngine->getRpm();
    const double target = std::round(rpm / 25.0) * 25.0;
    m_rpmHoldStallSeconds = 0.0;
    m_rpmHold.engage(target, m_speedSetting, rpm);
    if (m_infoCluster != nullptr) {
        m_infoCluster->setLogMessage("RPM hold " + std::to_string(static_cast<int>(m_rpmHold.targetRpm())));
    }
}

void EngineSimApplication::setRpmHoldTargetRpm(double rpm) {
    if (m_rpmHold.enabled()) m_rpmHold.setTargetRpm(rpm);
}

void EngineSimApplication::setTouchStarterHeld(bool held) { m_touchStarterHeld = held; }

void EngineSimApplication::setTouchThrottle(double value, bool held) {
    m_touchThrottle = clamp(value);
    m_touchThrottleHeld = held;
}
void EngineSimApplication::createObjects(Engine *engine) {
    for (int i = 0; i < engine->getCylinderCount(); ++i) {
        auto *rod = new ConnectingRodObject;
        rod->initialize(this);
        rod->m_connectingRod = engine->getConnectingRod(i);
        m_objects.push_back(rod);

        auto *piston = new PistonObject;
        piston->initialize(this);
        piston->m_piston = engine->getPiston(i);
        m_objects.push_back(piston);

        auto *chamber = new CombustionChamberObject;
        chamber->initialize(this);
        chamber->m_chamber = engine->getChamber(i);
        m_objects.push_back(chamber);
    }
    for (int i = 0; i < engine->getCrankshaftCount(); ++i) {
        auto *crankshaft = new CrankshaftObject;
        crankshaft->initialize(this);
        crankshaft->m_crankshaft = engine->getCrankshaft(i);
        m_objects.push_back(crankshaft);
    }
    for (int i = 0; i < engine->getCylinderBankCount(); ++i) {
        auto *bank = new CylinderBankObject;
        bank->initialize(this);
        bank->m_bank = engine->getCylinderBank(i);
        bank->m_head = engine->getHead(i);
        m_objects.push_back(bank);

        auto *head = new CylinderHeadObject;
        head->initialize(this);
        head->m_head = engine->getHead(i);
        head->m_engine = engine;
        m_objects.push_back(head);
    }
}
void EngineSimApplication::destroyObjects() { for (auto *object : m_objects) { object->destroy(); delete object; } m_objects.clear(); }

void EngineSimApplication::toggleFullscreen() {
    if (m_platform != nullptr) m_platform->setFullscreen(!m_platform->isFullscreen());
}

void EngineSimApplication::showControlsOverlay() { m_uiManager.showControlsOverlay(); }

void EngineSimApplication::showEnginePickerOverlay() { m_uiManager.showEnginePickerOverlay(); }

void EngineSimApplication::requestExternalEnginePicker() {
#if defined(__ANDROID__)
    m_externalEnginePickerPending = true;
    m_lastEnginePickerPollTick = 0;
    openAndroidEnginePicker();
    if (m_infoCluster != nullptr) m_infoCluster->setLogMessage("Select an engine .mr file");
#else
    if (m_infoCluster != nullptr) m_infoCluster->setLogMessage("External picker is available on Android");
#endif
}

void EngineSimApplication::refreshUserInterface() {
    m_uiManager.destroy();
    m_engineView = nullptr;
    m_rightGaugeCluster = nullptr;
    m_oscCluster = nullptr;
    m_performanceCluster = nullptr;
    m_loadSimulationCluster = nullptr;
    m_mixerCluster = nullptr;
    m_infoCluster = nullptr;
    if (m_simulator == nullptr) return;
    m_uiManager.initialize(this, m_platform);
    m_engineView = m_uiManager.getRoot()->addElement<EngineView>();
    m_rightGaugeCluster = m_uiManager.getRoot()->addElement<RightGaugeCluster>();
    m_oscCluster = m_uiManager.getRoot()->addElement<OscilloscopeCluster>();
    m_performanceCluster = m_uiManager.getRoot()->addElement<PerformanceCluster>();
    m_loadSimulationCluster = m_uiManager.getRoot()->addElement<LoadSimulationCluster>();
    m_mixerCluster = m_uiManager.getRoot()->addElement<MixerCluster>();
    m_infoCluster = m_uiManager.getRoot()->addElement<InfoCluster>();
    m_infoCluster->setEngine(m_iceEngine);
    m_rightGaugeCluster->m_simulator = m_simulator;
    m_rightGaugeCluster->setEngine(m_iceEngine);
    // refreshUserInterface() can be followed by renderScene() in this same
    // tick, before UiManager::update() has ever run. Wire nested widgets now.
    m_rightGaugeCluster->update(0.0f);
    m_oscCluster->setSimulator(m_simulator);
    m_performanceCluster->setSimulator(m_simulator);
    m_loadSimulationCluster->setSimulator(m_simulator);
    m_mixerCluster->setSimulator(m_simulator);
}
void EngineSimApplication::loadEngine(Engine *engine, Vehicle *vehicle, Transmission *transmission) {
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 01 stop audio");
#endif

#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE 1/10 | parando audio antigo");
#endif
    if (m_audioOutput != nullptr) m_audioOutput->stop();
    // Stop and join the synthesizer worker before destroying the simulator or
    // engine it is reading. Hot-loading while that worker is alive can race
    // destruction and crash on Android.
    if (m_simulator != nullptr) m_simulator->endAudioRenderingThread();
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 02 old audio worker stopped");
#endif

#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE 2/10 | audio antigo parado; destruindo UI/objetos");
#endif
    destroyObjects();
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 03 old scene objects destroyed");
#endif

    // refreshUserInterface() below owns the UI teardown/rebuild. Keeping a
    // single lifecycle boundary avoids double-destroying reusable UI hosts.
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 04 UI teardown deferred to refresh");
#endif

    m_engineView = nullptr;
    m_rightGaugeCluster = nullptr;
    m_oscCluster = nullptr;
    m_performanceCluster = nullptr;
    m_loadSimulationCluster = nullptr;
    m_mixerCluster = nullptr;
    m_infoCluster = nullptr;
    if (m_simulator != nullptr) { m_simulator->releaseSimulation(); delete m_simulator; }
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 05 old simulator released");
#endif

    if (m_iceEngine != nullptr) { m_iceEngine->destroy(); delete m_iceEngine; }
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 06 old engine destroyed");
#endif

    delete m_vehicle;
    delete m_transmission;
    // The transmission object starts in neutral, but these controls belong to
    // the host rather than a script. Reset them too so a replacement engine
    // cannot inherit throttle or clutch load from one that was in gear.
    m_speedSetting = 0.0;
    m_targetSpeedSetting = 0.0;
    m_touchThrottle = 0.0;
    m_touchThrottleHeld = false;
    m_touchStarterHeld = false;
    m_rpmHold.release();
    m_rpmHoldStallSeconds = 0.0;
    m_clutchPressure = 0.0;
    m_targetClutchPressure = 0.0;
    m_dynoSpeed = 0.0;
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE 3/10 | estado antigo destruido; instalando novo engine");
#endif
    m_iceEngine = engine;
    m_vehicle = vehicle;
    m_transmission = transmission;
    m_simulator = nullptr;
    if (engine == nullptr || vehicle == nullptr || transmission == nullptr) return;

    const int outputAudioSampleRate = m_audioOutput != nullptr
        ? m_audioOutput->outputSampleRate()
        : 44100;
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE 4/10 | criando Simulator");
#endif
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 07 creating simulator");
#endif
    m_simulator = engine->createSimulator(vehicle, transmission, outputAudioSampleRate);
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 08 simulator created");
#endif

#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE 5/10 | Simulator criado; configurando simulacao");
#endif
    m_viewParameters.Layer1 = engine->getMaxDepth();
    engine->calculateDisplacement();
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 09 displacement calculated");
#endif

#if defined(__ANDROID__)
    // Mobile performance mode. Physics cost scales almost linearly with this
    // frequency (and cylinder count). 2 kHz was audibly coarse in earlier
    // experiments; 6 kHz keeps substantially more temporal resolution while
    // cutting the default 10 kHz LS workload by 55%. 4.5 kHz is the next quality/performance point after the 6 kHz build still dropped below 20 FPS under throttle.
    // Alpha 15's 4.5 kHz cap is the proven stable performance point on Android.
    // Do not raise it per engine: the 6 kHz experiment reduced frame rate and
    // did not fix the malformed-engine issue, so that issue is not a timestep fix.
    constexpr double AndroidMaxSimulationFrequency = 4500.0;
    m_simulator->setSimulationFrequency(
        std::min(engine->getSimulationFrequency(), AndroidMaxSimulationFrequency));
#else
    m_simulator->setSimulationFrequency(engine->getSimulationFrequency());
#endif
    // Hold the audio reserve (queued + ready to play) near 140 ms with a slow
    // PI trim of at most +/-4% on how many physics steps run (latency_trim.h).
    // It replaces the old bang-bang correction (+/-10% every frame), which
    // bent the pitch at low rpm. The reserve must stay above the 80 ms that
    // the presentation guard in tick() requires, or rendering is skipped, and
    // below the 200 ms input cap, which discards audio and is heard as a buzz.
    m_simulator->setTargetSynthesizerLatency(0.14);
    m_simulator->setSynthesizerLatencyCorrectionEnabled(true);
    #if defined(__ANDROID__)
    // Preserve enough simulation history for the audio worker to recover from
    // short scheduler/render spikes instead of deleting useful input.
    m_simulator->setMaximumSynthesizerInputLatency(0.20);
#else
    m_simulator->setMaximumSynthesizerInputLatency(0.03);
#endif
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE 6/10 | simulacao configurada; carregando impulse responses");
#endif
    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        ImpulseResponse *response = engine->getExhaustSystem(i)->getImpulseResponse();
        if (response != nullptr && m_audioOutput != nullptr) {
            m_audioOutput->loadImpulseResponse(m_simulator->synthesizer(), response->getFilename(), response->getVolume(), i);
        }
    }
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE 7/10 | impulse responses OK; iniciando thread de audio");
#endif
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 10 impulse responses loaded; starting synth worker");
#endif
    m_simulator->startAudioRenderingThread();
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 11 synth worker started");
#endif

#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE 8/10 | thread de audio OK; iniciando saida SDL");
#endif
    if (m_audioOutput != nullptr) m_audioOutput->start(m_simulator);
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 12 SDL audio started");
#endif

#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE 9/10 | audio SDL OK; criando objetos visuais");
#endif
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 13 creating scene objects");
#endif
    createObjects(engine);
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 14 scene objects created");
#endif

#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 15 rebuilding UI");
#endif
    refreshUserInterface();
#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE | 16 load complete");
#endif

#if defined(__ANDROID__)
    checkpointAndroidMr("LOAD ENGINE 10/10 | UI pronta");
#endif
}
