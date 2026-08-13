#ifndef OPEN_ENGINE_SIM_RENDERER_H
#define OPEN_ENGINE_SIM_RENDERER_H

#include "render_math.h"

#include <cstdint>

// The UI and visualization layers submit a single, backend-neutral stream of
// indexed triangles. Platform renderers translate that stream to SDL GPU,
// WebGL, or another graphics API without leaking those APIs into the app.
class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void beginFrame(const ysVector &clearColor) = 0;
    virtual void setSceneViewport(float x, float y, float width, float height) = 0;
    virtual void uploadGeometry(
        const EngineSimVertex *vertices,
        int vertexCount,
        const std::uint16_t *indices,
        int indexCount) = 0;
    virtual void submitGeometry(
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
        int layer) = 0;
    virtual void endFrame() = 0;
};

#endif
