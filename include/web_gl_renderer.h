#ifndef OPEN_ENGINE_SIM_WEB_GL_RENDERER_H
#define OPEN_ENGINE_SIM_WEB_GL_RENDERER_H

#include "renderer.h"

#include <SDL3/SDL_video.h>

#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;

// Emscripten maps this SDL OpenGL ES 3 context to a WebGL 2 context. It uses
// the same renderer contract as the native SDL GPU implementation.
class WebGlRenderer final : public Renderer {
public:
    WebGlRenderer();
    ~WebGlRenderer() override;

    bool initialize(SDL_Window *window);
    void shutdown();
    const char *lastError() const;

    void beginFrame(const ysVector &clearColor) override;
    void setSceneViewport(float x, float y, float width, float height) override;
    void uploadGeometry(const EngineSimVertex *vertices, int vertexCount,
        const std::uint16_t *indices, int indexCount) override;
    void submitGeometry(const EngineSimVertex *vertices, const std::uint16_t *indices,
        int baseVertex, int baseIndex, int faceCount, const ysMatrix &transform,
        const ysMatrix &cameraView, const ysMatrix &projection, const ysVector &color,
        std::uint32_t stage, int layer) override;
    void endFrame() override;

private:
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

    bool createProgram();
    void drawStage(std::uint32_t stage);

    SDL_Window *m_window = nullptr;
    SDL_GLContext m_context = nullptr;
    unsigned int m_program = 0;
    unsigned int m_vertexBuffer = 0;
    unsigned int m_indexBuffer = 0;
    int m_transformLocation = -1;
    int m_cameraLocation = -1;
    int m_projectionLocation = -1;
    int m_colorLocation = -1;
    int m_windowWidth = 0;
    int m_windowHeight = 0;
    float m_sceneViewportX = 0.0f;
    float m_sceneViewportY = 0.0f;
    float m_sceneViewportWidth = 0.0f;
    float m_sceneViewportHeight = 0.0f;
    ysVector m_clearColor;
    const EngineSimVertex *m_vertices = nullptr;
    const std::uint16_t *m_indices = nullptr;
    int m_vertexCount = 0;
    int m_indexCount = 0;
    std::string m_error;
    std::vector<Submission> m_submissions;
};

#endif
