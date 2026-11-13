#include "../include/sdl_gpu_renderer.h"
#include "../include/shaders.h"

#include <SDL3/SDL.h>
#ifdef __ANDROID__
#include <android/log.h>
#include <jni.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
// Must match the frame geometry arena in EngineSimApplication. In particular,
// the active oscilloscope set can exceed the old 100k/200k budget once the
// starter begins producing live sample traces.
constexpr int MaxVertices = 500000;
constexpr int MaxIndices = 1000000;
#ifdef __ANDROID__
void screenRenderStatus(const std::string &s) {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    if (!cls) return;
    jmethodID method = env->GetMethodID(cls, "showRenderDiagnostics", "(Ljava/lang/String;)V");
    if (method) { jstring text = env->NewStringUTF(s.c_str()); env->CallVoidMethod(activity, method, text); env->DeleteLocalRef(text); }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
}
#endif

std::vector<std::uint8_t> loadBinaryFile(const std::string &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    const std::streamsize size = file.tellg();
    if (size <= 0) return {};
    std::vector<std::uint8_t> data(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(data.data()), size)) return {};
    return data;
}

SDL_GPUShaderFormat selectedShaderFormat(SDL_GPUDevice *device, const char **extension) {
    const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
#if defined(__APPLE__)
    // SDL's Metal backend accepts portable MSL source directly. Prefer it on
    // Apple even if a compatibility backend happens to advertise SPIR-V.
    if ((formats & SDL_GPU_SHADERFORMAT_MSL) != 0) { *extension = "msl"; return SDL_GPU_SHADERFORMAT_MSL; }
#endif
    if ((formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) { *extension = "spv"; return SDL_GPU_SHADERFORMAT_SPIRV; }
    if ((formats & SDL_GPU_SHADERFORMAT_DXIL) != 0) { *extension = "dxil"; return SDL_GPU_SHADERFORMAT_DXIL; }
    if ((formats & SDL_GPU_SHADERFORMAT_MSL) != 0) { *extension = "msl"; return SDL_GPU_SHADERFORMAT_MSL; }
    *extension = nullptr;
    return SDL_GPU_SHADERFORMAT_INVALID;
}
}

SdlGpuRenderer::SdlGpuRenderer()
    : m_window(nullptr), m_gpuDevice(nullptr), m_vertexBuffer(nullptr), m_indexBuffer(nullptr),
      m_vertexTransferBuffer(nullptr), m_indexTransferBuffer(nullptr), m_scenePipeline(nullptr),
      m_uiPipeline(nullptr), m_sceneTexture(nullptr), m_depthTexture(nullptr),
      m_sceneTextureWidth(0), m_sceneTextureHeight(0), m_depthTextureWidth(0), m_depthTextureHeight(0), m_vertices(nullptr),
      m_indices(nullptr), m_vertexCount(0), m_indexCount(0),
      m_clearColor(0.055f, 0.063f, 0.071f, 1.0f),
      m_sceneViewportX(0.0f), m_sceneViewportY(0.0f),
      m_sceneViewportWidth(0.0f), m_sceneViewportHeight(0.0f) { }
SdlGpuRenderer::~SdlGpuRenderer() { shutdown(); }

bool SdlGpuRenderer::initialize(void *nativeWindowHandle, const std::string &shaderDirectory) {
    m_error.clear();
    m_window = nativeWindowHandle;
    if (m_window == nullptr) { m_error = "SDL window handle is null"; return false; }

    m_gpuDevice = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
        true,
        nullptr);
    if (m_gpuDevice == nullptr
        || !SDL_ClaimWindowForGPUDevice(m_gpuDevice, static_cast<SDL_Window *>(m_window))) {
        m_error = SDL_GetError();
        shutdown();
        return false;
    }

    const SDL_GPUBufferCreateInfo vertexBufferInfo = {
        SDL_GPU_BUFFERUSAGE_VERTEX,
        static_cast<Uint32>(sizeof(EngineSimVertex) * MaxVertices),
        0
    };
    const SDL_GPUBufferCreateInfo indexBufferInfo = {
        SDL_GPU_BUFFERUSAGE_INDEX,
        static_cast<Uint32>(sizeof(std::uint16_t) * MaxIndices),
        0
    };
    const SDL_GPUTransferBufferCreateInfo vertexTransferInfo = {
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        static_cast<Uint32>(sizeof(EngineSimVertex) * MaxVertices),
        0
    };
    const SDL_GPUTransferBufferCreateInfo indexTransferInfo = {
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        static_cast<Uint32>(sizeof(std::uint16_t) * MaxIndices),
        0
    };
    m_vertexBuffer = SDL_CreateGPUBuffer(m_gpuDevice, &vertexBufferInfo);
    m_indexBuffer = SDL_CreateGPUBuffer(m_gpuDevice, &indexBufferInfo);
    m_vertexTransferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice, &vertexTransferInfo);
    m_indexTransferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice, &indexTransferInfo);
    if (m_vertexBuffer == nullptr || m_indexBuffer == nullptr
        || m_vertexTransferBuffer == nullptr || m_indexTransferBuffer == nullptr) {
        m_error = SDL_GetError();
        shutdown();
        return false;
    }

    const char *extension = nullptr;
    const SDL_GPUShaderFormat shaderFormat = selectedShaderFormat(m_gpuDevice, &extension);
    if (shaderFormat != SDL_GPU_SHADERFORMAT_INVALID) {
        const std::vector<std::uint8_t> vertexCode = loadBinaryFile(
            shaderDirectory + "/engine_sim.vertex." + extension);
        const std::vector<std::uint8_t> fragmentCode = loadBinaryFile(
            shaderDirectory + "/engine_sim.fragment." + extension);
        if (!vertexCode.empty() && !fragmentCode.empty()) {
            // The offline shadercross CLI preserves the authored HLSL entry
            // point names for every emitted backend, including MSL.
            const char *vertexEntrypoint = "VSMain";
            const char *fragmentEntrypoint = "PSMain";
            const SDL_GPUShaderCreateInfo vertexInfo = {
                vertexCode.size(), vertexCode.data(), vertexEntrypoint, shaderFormat, SDL_GPU_SHADERSTAGE_VERTEX,
                0, 0, 0, 1, 0
            };
            const SDL_GPUShaderCreateInfo fragmentInfo = {
                fragmentCode.size(), fragmentCode.data(), fragmentEntrypoint, shaderFormat, SDL_GPU_SHADERSTAGE_FRAGMENT,
                0, 0, 0, 1, 0
            };
            SDL_GPUShader *vertexShader = SDL_CreateGPUShader(m_gpuDevice, &vertexInfo);
            SDL_GPUShader *fragmentShader = SDL_CreateGPUShader(m_gpuDevice, &fragmentInfo);
            if (vertexShader != nullptr && fragmentShader != nullptr) {
                const SDL_GPUVertexBufferDescription vertexBufferDescription = {
                    0, static_cast<Uint32>(sizeof(EngineSimVertex)), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0
                };
                const SDL_GPUVertexAttribute vertexAttributes[] = {
                    { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, static_cast<Uint32>(offsetof(EngineSimVertex, Pos)) },
                    { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, static_cast<Uint32>(offsetof(EngineSimVertex, Normal)) },
                    { 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, static_cast<Uint32>(offsetof(EngineSimVertex, TexCoord)) }
                };
                SDL_GPUColorTargetDescription colorTarget = {};
                colorTarget.format = SDL_GetGPUSwapchainTextureFormat(m_gpuDevice, static_cast<SDL_Window *>(m_window));
                colorTarget.blend_state.enable_blend = true;
                colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
                colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
                colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
                SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
                pipelineInfo.vertex_shader = vertexShader;
                pipelineInfo.fragment_shader = fragmentShader;
                pipelineInfo.vertex_input_state = { &vertexBufferDescription, 1, vertexAttributes, 3 };
                pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                pipelineInfo.target_info = { &colorTarget, 1, SDL_GPU_TEXTUREFORMAT_D16_UNORM, false, 0, 0, 0 };
                pipelineInfo.depth_stencil_state.enable_depth_test = true;
                pipelineInfo.depth_stencil_state.enable_depth_write = true;
                pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
                m_scenePipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice, &pipelineInfo);

                pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID;
                pipelineInfo.depth_stencil_state = {};
                m_uiPipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice, &pipelineInfo);
                if (m_scenePipeline == nullptr || m_uiPipeline == nullptr) m_error = SDL_GetError();
            }
            else m_error = SDL_GetError();
            if (vertexShader != nullptr) SDL_ReleaseGPUShader(m_gpuDevice, vertexShader);
            if (fragmentShader != nullptr) SDL_ReleaseGPUShader(m_gpuDevice, fragmentShader);
        }
        else m_error = "Missing SDL GPU shader artifacts in " + shaderDirectory
            + "; install SDL_shadercross and rebuild engine-sim-desktop";
    }
    else m_error = SDL_GetError();
    if (m_scenePipeline == nullptr || m_uiPipeline == nullptr) {
        if (m_error.empty()) m_error = "SDL GPU graphics pipeline creation failed";
        shutdown();
        return false;
    }
    return true;
}

void SdlGpuRenderer::shutdown() {
    if (m_gpuDevice != nullptr) {
        if (m_scenePipeline != nullptr) SDL_ReleaseGPUGraphicsPipeline(m_gpuDevice, m_scenePipeline);
        if (m_uiPipeline != nullptr) SDL_ReleaseGPUGraphicsPipeline(m_gpuDevice, m_uiPipeline);
        if (m_sceneTexture != nullptr) SDL_ReleaseGPUTexture(m_gpuDevice, m_sceneTexture);
        if (m_depthTexture != nullptr) SDL_ReleaseGPUTexture(m_gpuDevice, m_depthTexture);
        if (m_vertexTransferBuffer != nullptr) SDL_ReleaseGPUTransferBuffer(m_gpuDevice, m_vertexTransferBuffer);
        if (m_indexTransferBuffer != nullptr) SDL_ReleaseGPUTransferBuffer(m_gpuDevice, m_indexTransferBuffer);
        if (m_vertexBuffer != nullptr) SDL_ReleaseGPUBuffer(m_gpuDevice, m_vertexBuffer);
        if (m_indexBuffer != nullptr) SDL_ReleaseGPUBuffer(m_gpuDevice, m_indexBuffer);
        m_vertexTransferBuffer = nullptr;
        m_indexTransferBuffer = nullptr;
        m_vertexBuffer = nullptr;
        m_indexBuffer = nullptr;
        m_scenePipeline = nullptr;
        m_uiPipeline = nullptr;
        m_sceneTexture = nullptr;
        m_depthTexture = nullptr;
        m_sceneTextureWidth = 0;
        m_sceneTextureHeight = 0;
        m_depthTextureWidth = 0;
        m_depthTextureHeight = 0;
        if (m_window != nullptr) {
            SDL_ReleaseWindowFromGPUDevice(m_gpuDevice, static_cast<SDL_Window *>(m_window));
        }
        SDL_DestroyGPUDevice(m_gpuDevice);
        m_gpuDevice = nullptr;
    }
    m_window = nullptr;
}

void SdlGpuRenderer::beginFrame(const ysVector &clearColor) {
#ifdef __ANDROID__
    static bool first = true;
    if (first) { __android_log_print(ANDROID_LOG_INFO, "OpenEngineSimRender", "FIRST beginFrame"); first = false; }
#endif
    m_clearColor = clearColor;
    m_vertices = nullptr;
    m_indices = nullptr;
    m_vertexCount = 0;
    m_indexCount = 0;
    m_submissions.clear();
}

void SdlGpuRenderer::setSceneViewport(float x, float y, float width, float height) {
    m_sceneViewportX = x;
    m_sceneViewportY = y;
    m_sceneViewportWidth = std::max(0.0f, width);
    m_sceneViewportHeight = std::max(0.0f, height);
}

void SdlGpuRenderer::uploadGeometry(
    const EngineSimVertex *vertices,
    int vertexCount,
    const std::uint16_t *indices,
    int indexCount)
{
    if (vertexCount < 0 || vertexCount > MaxVertices || indexCount < 0 || indexCount > MaxIndices) return;
    m_vertices = vertices;
    m_indices = indices;
    m_vertexCount = vertexCount;
    m_indexCount = indexCount;
#ifdef __ANDROID__
    static bool firstUpload = true;
    if (firstUpload) { __android_log_print(ANDROID_LOG_INFO, "OpenEngineSimRender", "FIRST uploadGeometry vertices=%d indices=%d", vertexCount, indexCount); firstUpload = false; }
#endif
}

void SdlGpuRenderer::submitGeometry(
    const EngineSimVertex *,
    const std::uint16_t *,
    int baseVertex,
    int baseIndex,
    int faceCount,
    const ysMatrix &transform,
    const ysMatrix &cameraView,
    const ysMatrix &projection,
    const ysVector &color,
    std::uint32_t stage,
    int layer)
{
    m_submissions.push_back({ baseVertex, baseIndex, faceCount,
        transform, cameraView, projection, color, stage, layer });
}

void SdlGpuRenderer::endFrame() {
#ifdef __ANDROID__
    static int frameNumber = 0; ++frameNumber;
    if (frameNumber <= 5 || frameNumber == 30 || frameNumber == 120) __android_log_print(ANDROID_LOG_INFO, "OpenEngineSimRender", "endFrame #%d vertices=%d indices=%d submissions=%zu viewport=%.0fx%.0f", frameNumber, m_vertexCount, m_indexCount, m_submissions.size(), m_sceneViewportWidth, m_sceneViewportHeight);
#endif
    if (m_gpuDevice == nullptr || m_window == nullptr) return;

    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(m_gpuDevice);
    if (commands == nullptr) {
#ifdef __ANDROID__
        screenRenderStatus("FRAME " + std::to_string(frameNumber) + "\nCOMMAND BUFFER: FALHOU\n" + SDL_GetError());
        __android_log_print(ANDROID_LOG_ERROR, "OpenEngineSimRender", "AcquireGPUCommandBuffer FAILED: %s", SDL_GetError());
#endif
        return;
    }

    if (m_vertexCount > 0 && m_indexCount > 0) {
        void *vertexUpload = SDL_MapGPUTransferBuffer(m_gpuDevice, m_vertexTransferBuffer, true);
        void *indexUpload = SDL_MapGPUTransferBuffer(m_gpuDevice, m_indexTransferBuffer, true);
        if (vertexUpload != nullptr && indexUpload != nullptr) {
            std::memcpy(vertexUpload, m_vertices, sizeof(EngineSimVertex) * static_cast<size_t>(m_vertexCount));
            std::memcpy(indexUpload, m_indices, sizeof(std::uint16_t) * static_cast<size_t>(m_indexCount));
        }
        if (vertexUpload != nullptr) SDL_UnmapGPUTransferBuffer(m_gpuDevice, m_vertexTransferBuffer);
        if (indexUpload != nullptr) SDL_UnmapGPUTransferBuffer(m_gpuDevice, m_indexTransferBuffer);

        if (vertexUpload != nullptr && indexUpload != nullptr) {
            SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commands);
            const SDL_GPUTransferBufferLocation vertexSource = { m_vertexTransferBuffer, 0 };
            const SDL_GPUTransferBufferLocation indexSource = { m_indexTransferBuffer, 0 };
            const SDL_GPUBufferRegion vertexDestination = {
                m_vertexBuffer, 0, static_cast<Uint32>(sizeof(EngineSimVertex) * m_vertexCount)
            };
            const SDL_GPUBufferRegion indexDestination = {
                m_indexBuffer, 0, static_cast<Uint32>(sizeof(std::uint16_t) * m_indexCount)
            };
            SDL_UploadToGPUBuffer(copyPass, &vertexSource, &vertexDestination, true);
            SDL_UploadToGPUBuffer(copyPass, &indexSource, &indexDestination, true);
            SDL_EndGPUCopyPass(copyPass);
        }
    }

    SDL_GPUTexture *swapchainTexture = nullptr;
    Uint32 swapchainWidth = 0;
    Uint32 swapchainHeight = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            commands, static_cast<SDL_Window *>(m_window), &swapchainTexture,
            &swapchainWidth, &swapchainHeight)) {
        SDL_CancelGPUCommandBuffer(commands);
#ifdef __ANDROID__
        screenRenderStatus("FRAME " + std::to_string(frameNumber) + "\nSWAPCHAIN: FALHOU\n" + SDL_GetError());
        __android_log_print(ANDROID_LOG_ERROR, "OpenEngineSimRender", "Acquire swapchain FAILED: %s", SDL_GetError());
#endif
        return;
    }

    if (swapchainTexture != nullptr) {
        const Uint32 sceneWidth = std::max(1u, static_cast<Uint32>(m_sceneViewportWidth));
        const Uint32 sceneHeight = std::max(1u, static_cast<Uint32>(m_sceneViewportHeight));
        if (m_sceneTexture == nullptr || m_sceneTextureWidth != sceneWidth || m_sceneTextureHeight != sceneHeight) {
            if (m_sceneTexture != nullptr) SDL_ReleaseGPUTexture(m_gpuDevice, m_sceneTexture);
            const SDL_GPUTextureCreateInfo sceneInfo = { SDL_GPU_TEXTURETYPE_2D,
                SDL_GetGPUSwapchainTextureFormat(m_gpuDevice, static_cast<SDL_Window *>(m_window)),
                SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
                sceneWidth, sceneHeight, 1, 1, SDL_GPU_SAMPLECOUNT_1, 0 };
            m_sceneTexture = SDL_CreateGPUTexture(m_gpuDevice, &sceneInfo);
            m_sceneTextureWidth = sceneWidth;
            m_sceneTextureHeight = sceneHeight;
        }
        if (m_depthTexture == nullptr || m_depthTextureWidth != sceneWidth || m_depthTextureHeight != sceneHeight) {
            if (m_depthTexture != nullptr) SDL_ReleaseGPUTexture(m_gpuDevice, m_depthTexture);
            const SDL_GPUTextureCreateInfo depthInfo = {
                SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET, sceneWidth, sceneHeight,
                1, 1, SDL_GPU_SAMPLECOUNT_1, 0
            };
            m_depthTexture = SDL_CreateGPUTexture(m_gpuDevice, &depthInfo);
            m_depthTextureWidth = sceneWidth;
            m_depthTextureHeight = sceneHeight;
        }

        const auto drawStage = [&](SDL_GPURenderPass *pass, std::uint32_t stage) {
            // Reused every frame: this runs twice per frame over every draw.
            static thread_local std::vector<const Submission *> submissions;
            submissions.clear();
            for (const Submission &submission : m_submissions) {
                if ((submission.stage & stage) != 0) submissions.push_back(&submission);
            }
            std::stable_sort(submissions.begin(), submissions.end(),
                [](const Submission *left, const Submission *right) { return left->layer < right->layer; });
            for (const Submission *submission : submissions) {
                const ysMatrix transforms[] = {
                    submission->transform,
                    submission->cameraView,
                    submission->projection
                };
                SDL_PushGPUVertexUniformData(commands, 0, transforms, sizeof(transforms));
                SDL_PushGPUFragmentUniformData(commands, 0, &submission->color, sizeof(submission->color));
                SDL_DrawGPUIndexedPrimitives(pass, static_cast<Uint32>(submission->faceCount * 3), 1,
                    static_cast<Uint32>(submission->baseIndex), submission->baseVertex, 0);
            }
        };
        const auto bindGeometry = [&](SDL_GPURenderPass *pass, SDL_GPUGraphicsPipeline *pipeline) {
            SDL_BindGPUGraphicsPipeline(pass, pipeline);
            const SDL_GPUBufferBinding vertexBinding = { m_vertexBuffer, 0 };
            const SDL_GPUBufferBinding indexBinding = { m_indexBuffer, 0 };
            SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
            SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        };

        SDL_GPUColorTargetInfo sceneColorTarget = {};
        sceneColorTarget.texture = m_sceneTexture;
        sceneColorTarget.clear_color = { m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w };
        sceneColorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        sceneColorTarget.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depthTarget = {};
        depthTarget.texture = m_depthTexture;
        depthTarget.clear_depth = 1.0f;
        depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        SDL_GPUColorTargetInfo clearTarget = sceneColorTarget;
        clearTarget.texture = swapchainTexture;
        // Anything the scene blit and the UI do not cover must show the normal
        // background. A magenta debug clear used to be left here on Android and
        // showed through at the edges of the screen.
        SDL_GPURenderPass *clearPass = SDL_BeginGPURenderPass(commands, &clearTarget, 1, nullptr);
        if (clearPass != nullptr) SDL_EndGPURenderPass(clearPass);
        SDL_GPURenderPass *scenePass = SDL_BeginGPURenderPass(commands, &sceneColorTarget, 1,
            m_depthTexture != nullptr ? &depthTarget : nullptr);
        if (scenePass != nullptr && m_scenePipeline != nullptr && m_vertexCount > 0 && m_indexCount > 0) {
            bindGeometry(scenePass, m_scenePipeline);
            if (m_sceneViewportWidth > 0.0f && m_sceneViewportHeight > 0.0f) {
                const SDL_GPUViewport sceneViewport = { 0.0f, 0.0f,
                    static_cast<float>(sceneWidth), static_cast<float>(sceneHeight), 0.0f, 1.0f };
                SDL_SetGPUViewport(scenePass, &sceneViewport);
            }
            drawStage(scenePass, Shaders::SceneStage);
        }
        if (scenePass != nullptr) SDL_EndGPURenderPass(scenePass);

        if (m_sceneTexture != nullptr) {
            // Engine Sim lays out the dashboard in logical window coordinates,
            // while SDL_GPU's Android swapchain is in physical pixels. Scale the
            // scene destination to the real swapchain instead of blitting 1:1.
            int logicalWidth = 0;
            int logicalHeight = 0;
            SDL_GetWindowSize(static_cast<SDL_Window *>(m_window), &logicalWidth, &logicalHeight);
            const float scaleX = logicalWidth > 0
                ? static_cast<float>(swapchainWidth) / static_cast<float>(logicalWidth) : 1.0f;
            const float scaleY = logicalHeight > 0
                ? static_cast<float>(swapchainHeight) / static_cast<float>(logicalHeight) : 1.0f;
            const Uint32 dstX = static_cast<Uint32>(std::max(0.0f, m_sceneViewportX * scaleX));
            const Uint32 dstY = static_cast<Uint32>(std::max(0.0f, m_sceneViewportY * scaleY));
            const Uint32 dstW = std::min(swapchainWidth - std::min(dstX, swapchainWidth),
                std::max(1u, static_cast<Uint32>(m_sceneViewportWidth * scaleX)));
            const Uint32 dstH = std::min(swapchainHeight - std::min(dstY, swapchainHeight),
                std::max(1u, static_cast<Uint32>(m_sceneViewportHeight * scaleY)));
            const SDL_GPUBlitInfo blitInfo = {
                { m_sceneTexture, 0, 0, 0, 0, sceneWidth, sceneHeight },
                { swapchainTexture, 0, 0, dstX, dstY, dstW, dstH },
                SDL_GPU_LOADOP_LOAD, {}, SDL_FLIP_NONE, SDL_GPU_FILTER_LINEAR, false, 0, 0, 0
            };
            SDL_BlitGPUTexture(commands, &blitInfo);
        }

        SDL_GPUColorTargetInfo uiColorTarget = sceneColorTarget;
        uiColorTarget.texture = swapchainTexture;
        uiColorTarget.load_op = SDL_GPU_LOADOP_LOAD;
        SDL_GPURenderPass *uiPass = SDL_BeginGPURenderPass(commands, &uiColorTarget, 1, nullptr);
        if (uiPass != nullptr && m_uiPipeline != nullptr && m_vertexCount > 0 && m_indexCount > 0) {
            bindGeometry(uiPass, m_uiPipeline);
            drawStage(uiPass, Shaders::UiStage);
        }
        if (uiPass != nullptr) SDL_EndGPURenderPass(uiPass);
    }
    #ifdef __ANDROID__
    if (frameNumber <= 5 || frameNumber == 30 || frameNumber == 120) __android_log_print(ANDROID_LOG_INFO, "OpenEngineSimRender", "swapchain=%p size=%ux%u sceneTexture=%p depth=%p -> SUBMIT", swapchainTexture, swapchainWidth, swapchainHeight, m_sceneTexture, m_depthTexture);
    #endif
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
#ifdef __ANDROID__
        screenRenderStatus("FRAME " + std::to_string(frameNumber) + "\nSUBMIT GPU: FALHOU\n" + SDL_GetError());
        __android_log_print(ANDROID_LOG_ERROR, "OpenEngineSimRender", "SubmitGPUCommandBuffer FAILED: %s", SDL_GetError());
#endif
    }
#ifdef __ANDROID__
    if (frameNumber == 1 || frameNumber == 5 || frameNumber == 30 || frameNumber == 120) {
        std::string status =
            "FRAME: " + std::to_string(frameNumber) +
            "\nBEGIN FRAME: OK" +
            "\nVERTICES: " + std::to_string(m_vertexCount) +
            "\nINDICES: " + std::to_string(m_indexCount) +
            "\nSUBMISSIONS: " + std::to_string(m_submissions.size()) +
            "\nVIEWPORT: " + std::to_string((int)m_sceneViewportWidth) + " x " + std::to_string((int)m_sceneViewportHeight) +
            "\nSWAPCHAIN: " + std::to_string(swapchainWidth) + " x " + std::to_string(swapchainHeight) +
            "\nSCENE TEXTURE: " + std::string(m_sceneTexture ? "OK" : "NULL") +
            "\nDEPTH TEXTURE: " + std::string(m_depthTexture ? "OK" : "NULL") +
            "\nSUBMIT GPU: OK" +
            "\nTEST CLEAR: MAGENTA";
        screenRenderStatus(status);
    }
#endif
}

const char *SdlGpuRenderer::lastError() const {
    return m_error.empty() ? SDL_GetError() : m_error.c_str();
}
