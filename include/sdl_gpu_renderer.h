#ifndef ATG_ENGINE_SIM_SDL_GPU_RENDERER_H
#define ATG_ENGINE_SIM_SDL_GPU_RENDERER_H

#include "renderer.h"

#include <cstdint>
#include <string>
#include <vector>

struct SDL_GPUDevice;
struct SDL_GPUBuffer;
struct SDL_GPUTransferBuffer;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUTexture;

class SdlGpuRenderer final : public Renderer {
public:
    SdlGpuRenderer();
    ~SdlGpuRenderer();

    bool initialize(void *nativeWindowHandle, const std::string &shaderDirectory);
    void shutdown();
    void beginFrame(const ysVector &clearColor) override;
    // Coordinates use SDL GPU's top-left window convention. Scene submissions
    // are clipped to this rectangle; UI submissions continue over the window.
    void setSceneViewport(float x, float y, float width, float height) override;
    void uploadGeometry(
        const EngineSimVertex *vertices,
        int vertexCount,
        const std::uint16_t *indices,
        int indexCount) override;
    void submitGeometry(
        const EngineSimVertex *vertices,
        const std::uint16_t *indices,
        int baseVertex,
        int baseIndex,
        int faceCount,
        const ysMatrix &transform,
        const ysMatrix &cameraView,
        const ysMatrix &projection,
        const ysVector &color,
        std::uint32_t stage,
        int layer) override;
    void endFrame() override;
    const char *lastError() const;

private:
    void *m_window;
    SDL_GPUDevice *m_gpuDevice;
    SDL_GPUBuffer *m_vertexBuffer;
    SDL_GPUBuffer *m_indexBuffer;
    SDL_GPUTransferBuffer *m_vertexTransferBuffer;
    SDL_GPUTransferBuffer *m_indexTransferBuffer;
    SDL_GPUGraphicsPipeline *m_scenePipeline;
    SDL_GPUGraphicsPipeline *m_uiPipeline;
    SDL_GPUTexture *m_sceneTexture;
    SDL_GPUTexture *m_depthTexture;
    std::uint32_t m_sceneTextureWidth;
    std::uint32_t m_sceneTextureHeight;
    std::uint32_t m_depthTextureWidth;
    std::uint32_t m_depthTextureHeight;
    const EngineSimVertex *m_vertices;
    const std::uint16_t *m_indices;
    int m_vertexCount;
    int m_indexCount;
    ysVector m_clearColor;
    std::string m_error;
    float m_sceneViewportX;
    float m_sceneViewportY;
    float m_sceneViewportWidth;
    float m_sceneViewportHeight;

    struct Submission {
        int baseVertex;
        int baseIndex;
        int faceCount;
        ysMatrix transform;
        ysMatrix cameraView;
        ysMatrix projection;
        ysVector color;
        std::uint32_t stage;
        int layer;
    };
    std::vector<Submission> m_submissions;
};

#endif /* ATG_ENGINE_SIM_SDL_GPU_RENDERER_H */
