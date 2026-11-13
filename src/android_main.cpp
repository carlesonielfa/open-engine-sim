#include "../include/desktop_platform_sdl.h"
#include "../include/engine_sim_application.h"
#include "../include/sdl_audio_output.h"
#include "../include/web_gl_renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <android/log.h>
#include <jni.h>
#include <filesystem>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <dlfcn.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>

volatile sig_atomic_t gCrashSubstage = 0;
volatile sig_atomic_t gCrashDetail = 0;

namespace {
constexpr const char *LogTag = "OpenEngineSim";
char gCrashLogPath[512] = {};
volatile sig_atomic_t gCrashStage = 0;
uintptr_t gMainModuleBase = 0;
pid_t gMainThreadTid = 0;

void nativeCrashHandler(int signalNumber, siginfo_t *info, void *context) {
    uintptr_t pc = 0, lr = 0, sp = 0, fp = 0;
#if defined(__aarch64__)
    if (context != nullptr) {
        const auto *uc = static_cast<const ucontext_t *>(context);
        pc = static_cast<uintptr_t>(uc->uc_mcontext.pc);
        lr = static_cast<uintptr_t>(uc->uc_mcontext.regs[30]);
        sp = static_cast<uintptr_t>(uc->uc_mcontext.sp);
        fp = static_cast<uintptr_t>(uc->uc_mcontext.regs[29]);
    }
#endif
    const uintptr_t faultAddress = (info != nullptr)
        ? reinterpret_cast<uintptr_t>(info->si_addr) : 0;
    const pid_t tid = static_cast<pid_t>(syscall(__NR_gettid));
    const uintptr_t pcOffset = (gMainModuleBase != 0 && pc >= gMainModuleBase) ? pc - gMainModuleBase : 0;
    const uintptr_t lrOffset = (gMainModuleBase != 0 && lr >= gMainModuleBase) ? lr - gMainModuleBase : 0;

    char buffer[768];
    const int length = snprintf(buffer, sizeof(buffer),
        "OPEN ENGINE SIM NATIVE CRASH V2\n"
        "signal=%d\nstage=%d\nsubstage=%d\ndetail=%d\n"
        "tid=%d\nmain_tid=%d\nthread=%s\n"
        "fault_addr=0x%llx\nmodule_base=0x%llx\npc=0x%llx\npc_offset=0x%llx\n"
        "lr=0x%llx\nlr_offset=0x%llx\nsp=0x%llx\nfp=0x%llx\n"
        "render detail: 141=generateGeometry, 142=object render, 143=UI render, 144=render done, "
        "145=beginFrame, 146=layout/reset, 147=render body, 148=uploadGeometry, 149=endFrame\n",
        signalNumber, static_cast<int>(gCrashStage), static_cast<int>(gCrashSubstage),
        static_cast<int>(gCrashDetail), static_cast<int>(tid), static_cast<int>(gMainThreadTid),
        (tid == gMainThreadTid) ? "MAIN" : "WORKER",
        static_cast<unsigned long long>(faultAddress),
        static_cast<unsigned long long>(gMainModuleBase),
        static_cast<unsigned long long>(pc), static_cast<unsigned long long>(pcOffset),
        static_cast<unsigned long long>(lr), static_cast<unsigned long long>(lrOffset),
        static_cast<unsigned long long>(sp), static_cast<unsigned long long>(fp));
    if (gCrashLogPath[0] != '\0') {
        const int fd = open(gCrashLogPath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            if (length > 0) write(fd, buffer, static_cast<size_t>(length));
            close(fd);
        }
    }
    struct sigaction restore {};
    restore.sa_handler = SIG_DFL;
    sigemptyset(&restore.sa_mask);
    sigaction(signalNumber, &restore, nullptr);
    raise(signalNumber);
}

void installNativeCrashHandlers(const char *internalStorage) {
    snprintf(gCrashLogPath, sizeof(gCrashLogPath), "%s/native_crash_last.txt", internalStorage);
    gMainThreadTid = static_cast<pid_t>(syscall(__NR_gettid));

    Dl_info moduleInfo {};
    if (dladdr(reinterpret_cast<void *>(&installNativeCrashHandlers), &moduleInfo) != 0 &&
        moduleInfo.dli_fbase != nullptr) {
        gMainModuleBase = reinterpret_cast<uintptr_t>(moduleInfo.dli_fbase);
    }

    struct sigaction action {};
    action.sa_sigaction = nativeCrashHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGABRT, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGILL, &action, nullptr);
}

void logInfo(const char *message) { __android_log_print(ANDROID_LOG_INFO, LogTag, "%s", message); }
void logError(const char *stage, const char *error) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "%s: %s", stage, error ? error : "(no error)");
}
void callActivityStringMethod(const char *methodName, const std::string &message) {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    if (!cls) return;
    jmethodID method = env->GetMethodID(cls, methodName, "(Ljava/lang/String;)V");
    if (method) {
        jstring text = env->NewStringUTF(message.c_str());
        env->CallVoidMethod(activity, method, text);
        env->DeleteLocalRef(text);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
}
void showStatus(const std::string &message) {
    logInfo(message.c_str());
    callActivityStringMethod("updateNativeStatus", message);
}
void hideDiagnostics() {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    if (!cls) return;
    jmethodID method = env->GetMethodID(cls, "hideNativeDiagnostics", "()V");
    if (method) env->CallVoidMethod(activity, method);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
}
}

int main(int, char **) {
    showStatus("1/6 C++ MAIN: OK\n2/6 SDL: iniciando...");
    DesktopPlatformSdl platform;
    if (!platform.initialize("Open Engine Simulator", 1280, 720)) {
        const std::string error = platform.lastError();
        logError("SDL initialization failed", error.c_str());
        showStatus("1/6 C++ MAIN: OK\n2/6 SDL: FALHOU\n" + error);
        return 1;
    }

    showStatus("1/6 C++ MAIN: OK\n2/6 SDL + JANELA: OK\n3/6 STORAGE: verificando...");
    const char *internalStorage = SDL_GetAndroidInternalStoragePath();
    if (!internalStorage) {
        const std::string error = SDL_GetError();
        showStatus("1/6 C++ MAIN: OK\n2/6 SDL + JANELA: OK\n3/6 STORAGE: FALHOU\n" + error);
        platform.shutdown();
        return 1;
    }

    installNativeCrashHandlers(internalStorage);
    gCrashStage = 10;

    RuntimePaths paths;
    paths.applicationDirectory = internalStorage;
    paths.assetDirectory = std::filesystem::path(internalStorage) / "assets";
    showStatus("1/6 C++ MAIN: OK\n2/6 SDL + JANELA: OK\n3/6 STORAGE: OK\n4/6 OPENGL ES 3: iniciando...");
    WebGlRenderer renderer;
    if (!renderer.initialize(static_cast<SDL_Window *>(platform.nativeWindowHandle()))) {
        const std::string error = renderer.lastError();
        logError("OpenGL ES initialization failed", error.c_str());
        showStatus("1/6 C++ MAIN: OK\n2/6 SDL + JANELA: OK\n3/6 STORAGE: OK\n4/6 OPENGL ES 3: FALHOU\n\nERRO:\n" + error);
        platform.shutdown();
        return 1;
    }

    showStatus("1/6 C++ MAIN: OK\n2/6 SDL + JANELA: OK\n3/6 STORAGE: OK\n4/6 OPENGL ES 3: OK\n5/6 ENGINE SIM: inicializando...");
    EngineSimApplication application;
    SdlAudioOutput audioOutput;
    application.initialize(&platform, &renderer, &audioOutput, paths);

    showStatus("1/6 C++ MAIN: OK\n2/6 SDL + JANELA: OK\n3/6 STORAGE: OK\n4/6 OPENGL ES 3: OK\n5/6 ENGINE SIM: OK\n6/6 LOOP: INICIANDO\n\nRemovendo overlay...");
    SDL_Delay(700);
    if (!application.tick()) {
        showStatus("ENGINE SIM: loop encerrou antes do primeiro frame");
        application.destroy();
        renderer.shutdown();
        platform.shutdown();
        return 1;
    }
    hideDiagnostics();

    application.run();
    application.destroy();
    renderer.shutdown();
    platform.shutdown();
    return 0;
}
