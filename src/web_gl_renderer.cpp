#include "../include/web_gl_renderer.h"

#include "../include/shaders.h"

#include <SDL3/SDL.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <cstddef>

namespace {
constexpr int MaxVertices = 500000;
constexpr int MaxIndices = 1000000;

constexpr const char *VertexShader = R"(#version 300 es
layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec2 texcoord;
uniform mat4 transform;
uniform mat4 cameraView;
uniform mat4 projection;
void main() {
    // Match the row-vector multiplication order used by the SDL GPU shader.
    gl_Position = position * transform * cameraView * projection;
}
)";

constexpr const char *FragmentShader = R"(#version 300 es
precision mediump float;
uniform vec4 baseColor;
out vec4 fragmentColor;
void main() {
    fragmentColor = baseColor;
}
)";

unsigned int compileShader(unsigned int type, const char *source, std::string *error) {
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        char log[1024] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        *error = log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
}

WebGlRenderer::WebGlRenderer()
    : m_clearColor(0.055f, 0.063f, 0.071f, 1.0f) { }

WebGlRenderer::~WebGlRenderer() { shutdown(); }

bool WebGlRenderer::initialize(SDL_Window *window) {
    m_error.clear();
    m_window = window;
    if (m_window == nullptr) {
        m_error = "SDL window handle is null";
        return false;
    }
    // The scene renderer uses depth testing for overlapping mesh faces. SDL's
    // browser context does not guarantee a depth attachment unless requested.
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    m_context = SDL_GL_CreateContext(m_window);
    if (m_context == nullptr || !SDL_GL_MakeCurrent(m_window, m_context)) {
        m_error = SDL_GetError();
        shutdown();
        return false;
    }
    if (!createProgram()) {
        shutdown();
        return false;
    }
    glGenBuffers(1, &m_vertexBuffer);
    glGenBuffers(1, &m_indexBuffer);
    if (m_vertexBuffer == 0 || m_indexBuffer == 0) {
        m_error = "WebGL buffer allocation failed";
        shutdown();
        return false;
    }
    return true;
}

void WebGlRenderer::shutdown() {
    if (m_context != nullptr) {
        if (m_vertexBuffer != 0) glDeleteBuffers(1, &m_vertexBuffer);
        if (m_indexBuffer != 0) glDeleteBuffers(1, &m_indexBuffer);
        if (m_program != 0) glDeleteProgram(m_program);
        SDL_GL_DestroyContext(m_context);
    }
    m_context = nullptr;
    m_window = nullptr;
    m_program = 0;
    m_vertexBuffer = 0;
    m_indexBuffer = 0;
}

const char *WebGlRenderer::lastError() const {
    return m_error.empty() ? SDL_GetError() : m_error.c_str();
}

bool WebGlRenderer::createProgram() {
    const unsigned int vertex = compileShader(GL_VERTEX_SHADER, VertexShader, &m_error);
    if (vertex == 0) return false;
    const unsigned int fragment = compileShader(GL_FRAGMENT_SHADER, FragmentShader, &m_error);
    if (fragment == 0) {
        glDeleteShader(vertex);
        return false;
    }
    m_program = glCreateProgram();
    glAttachShader(m_program, vertex);
    glAttachShader(m_program, fragment);
    glLinkProgram(m_program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    int linked = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024] = {};
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        m_error = log;
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }
    m_transformLocation = glGetUniformLocation(m_program, "transform");
    m_cameraLocation = glGetUniformLocation(m_program, "cameraView");
    m_projectionLocation = glGetUniformLocation(m_program, "projection");
    m_colorLocation = glGetUniformLocation(m_program, "baseColor");
    return true;
}

void WebGlRenderer::beginFrame(const ysVector &clearColor) {
    m_clearColor = clearColor;
    m_vertices = nullptr;
    m_indices = nullptr;
    m_vertexCount = 0;
    m_indexCount = 0;
    m_submissions.clear();
}

void WebGlRenderer::setSceneViewport(float x, float y, float width, float height) {
    m_sceneViewportX = std::max(0.0f, x);
    m_sceneViewportY = std::max(0.0f, y);
    m_sceneViewportWidth = std::max(0.0f, width);
    m_sceneViewportHeight = std::max(0.0f, height);
}

void WebGlRenderer::uploadGeometry(const EngineSimVertex *vertices, int vertexCount,
    const std::uint16_t *indices, int indexCount)
{
    if (vertexCount < 0 || vertexCount > MaxVertices || indexCount < 0 || indexCount > MaxIndices) return;
    m_vertices = vertices;
    m_indices = indices;
    m_vertexCount = vertexCount;
    m_indexCount = indexCount;
}

void WebGlRenderer::submitGeometry(const EngineSimVertex *, const std::uint16_t *,
    int baseVertex, int baseIndex, int faceCount, const ysMatrix &transform,
    const ysMatrix &cameraView, const ysMatrix &projection, const ysVector &color,
    std::uint32_t stage, int layer)
{
    m_submissions.push_back({ baseVertex, baseIndex, faceCount, transform, cameraView,
        projection, color, stage, layer });
}

void WebGlRenderer::drawStage(std::uint32_t stage) {
    std::vector<const Submission *> submissions;
    for (const Submission &submission : m_submissions) {
        if ((submission.stage & stage) != 0) submissions.push_back(&submission);
    }
    std::stable_sort(submissions.begin(), submissions.end(),
        [](const Submission *left, const Submission *right) { return left->layer < right->layer; });
    for (const Submission *submission : submissions) {
        // Object transforms are stored in the layout expected by the HLSL
        // row-vector path. Camera and projection matrices have already been
        // transposed for that path, so transpose them back before WebGL reads
        // their row-major storage as column-major uniform data.
        ysMatrix cameraView = ysMath::Transpose(submission->cameraView);
        const ysMatrix projection = ysMath::Transpose(submission->projection);
        if ((stage & Shaders::SceneStage) != 0) {
            // The established desktop composition leaves the engine-view pan
            // out of the scene's screen-space placement. Preserve that visual
            // anchor here while keeping the correctly projected mesh depth.
            cameraView.m[0][3] = 0.0f;
            cameraView.m[1][3] = 0.0f;
        }
        glUniformMatrix4fv(m_transformLocation, 1, GL_FALSE, &submission->transform.m[0][0]);
        glUniformMatrix4fv(m_cameraLocation, 1, GL_FALSE, &cameraView.m[0][0]);
        glUniformMatrix4fv(m_projectionLocation, 1, GL_FALSE, &projection.m[0][0]);
        glUniform4f(m_colorLocation, submission->color.x, submission->color.y,
            submission->color.z, submission->color.w);
        const std::size_t vertexOffset = static_cast<std::size_t>(submission->baseVertex)
            * sizeof(EngineSimVertex);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(EngineSimVertex),
            reinterpret_cast<const void *>(vertexOffset + offsetof(EngineSimVertex, Pos)));
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(EngineSimVertex),
            reinterpret_cast<const void *>(vertexOffset + offsetof(EngineSimVertex, Normal)));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(EngineSimVertex),
            reinterpret_cast<const void *>(vertexOffset + offsetof(EngineSimVertex, TexCoord)));
        glDrawElements(GL_TRIANGLES, submission->faceCount * 3, GL_UNSIGNED_SHORT,
            reinterpret_cast<const void *>(static_cast<std::size_t>(submission->baseIndex) * sizeof(std::uint16_t)));
    }
}

void WebGlRenderer::endFrame() {
    if (m_context == nullptr || m_window == nullptr) return;
    SDL_GetWindowSizeInPixels(m_window, &m_windowWidth, &m_windowHeight);
    if (m_windowWidth <= 0 || m_windowHeight <= 0) return;

    glViewport(0, 0, m_windowWidth, m_windowHeight);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_vertexCount > 0 && m_indexCount > 0) {
        glUseProgram(m_program);
        glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(EngineSimVertex) * m_vertexCount),
            m_vertices, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(std::uint16_t) * m_indexCount),
            m_indices, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(EngineSimVertex),
            reinterpret_cast<const void *>(offsetof(EngineSimVertex, Pos)));
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(EngineSimVertex),
            reinterpret_cast<const void *>(offsetof(EngineSimVertex, Normal)));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(EngineSimVertex),
            reinterpret_cast<const void *>(offsetof(EngineSimVertex, TexCoord)));

        const int sceneX = static_cast<int>(m_sceneViewportX);
        const int sceneY = m_windowHeight - static_cast<int>(m_sceneViewportY + m_sceneViewportHeight);
        glViewport(sceneX, sceneY, static_cast<int>(m_sceneViewportWidth),
            static_cast<int>(m_sceneViewportHeight));
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        drawStage(Shaders::SceneStage);

        glViewport(0, 0, m_windowWidth, m_windowHeight);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawStage(Shaders::UiStage);
        glDisable(GL_BLEND);
    }
    SDL_GL_SwapWindow(m_window);
}
