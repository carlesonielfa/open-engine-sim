#ifndef ATG_ENGINE_SIM_DESKTOP_PLATFORM_SDL_H
#define ATG_ENGINE_SIM_DESKTOP_PLATFORM_SDL_H

#include "desktop_platform.h"

#include <array>
#include <string>

struct SDL_Window;

class DesktopPlatformSdl final : public DesktopPlatform {
public:
    DesktopPlatformSdl();
    ~DesktopPlatformSdl() override;

    bool initialize(const std::string &title, int width, int height) override;
    void pumpEvents() override;
    void shutdown() override;

    bool shouldQuit() const override { return m_shouldQuit; }
    bool isKeyDown(DesktopKey key) const override;
    bool wasKeyPressed(DesktopKey key) const override;
    bool wasMouseButtonPressed(DesktopMouseButton button) const override;
    bool wasMouseButtonReleased(DesktopMouseButton button) const override;
    void mousePosition(int *x, int *y) const override;
    const std::vector<DesktopTouchEvent> &touchEvents() const override { return m_touchEvents; }
    float mouseWheelY() const override;

    int windowWidth() const override { return m_windowWidth; }
    int windowHeight() const override { return m_windowHeight; }
    bool isFullscreen() const override { return m_fullscreen; }
    void setFullscreen(bool enabled) override;
    void *nativeWindowHandle() const override { return m_window; }
    std::uint64_t ticks() const override;
    void delay(std::uint32_t milliseconds) const override;
    std::string applicationDirectory() const override;
    std::string lastError() const override;

private:
    static constexpr std::size_t KeyCount = 41;
    static constexpr std::size_t MouseButtonCount = 3;

    static std::size_t keyIndex(DesktopKey key);
    static std::size_t mouseButtonIndex(DesktopMouseButton button);
    static int sdlScancode(DesktopKey key);

    SDL_Window *m_window;
    std::array<bool, KeyCount> m_keysDown;
    std::array<bool, KeyCount> m_keysPressed;
    std::array<bool, MouseButtonCount> m_mousePressed;
    std::array<bool, MouseButtonCount> m_mouseReleased;
    bool m_shouldQuit;
    bool m_fullscreen;
    int m_windowWidth;
    int m_windowHeight;
    int m_windowLogicalWidth;
    int m_windowLogicalHeight;
    int m_mouseX;
    int m_mouseY;
    float m_mouseWheelY;
    std::vector<DesktopTouchEvent> m_touchEvents;
};

#endif /* ATG_ENGINE_SIM_DESKTOP_PLATFORM_SDL_H */
