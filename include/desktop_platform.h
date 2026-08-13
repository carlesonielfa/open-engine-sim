#ifndef ATG_ENGINE_SIM_DESKTOP_PLATFORM_H
#define ATG_ENGINE_SIM_DESKTOP_PLATFORM_H

#include <cstdint>
#include <string>
#include <vector>

enum class DesktopKey {
    Escape,
    Return,
    Tab,
    Space,
    Left,
    Right,
    Up,
    Down,
    Insert,
    Comma,
    A, B, C, D, E, F, G, H, I, M, N, Q, R, S, T, U, V, W, X, Y, Z,
    F1, F2, F3, F4, F5,
    Digit1, Digit2, Digit3, Digit4, Digit5,
};

enum class DesktopMouseButton { Left, Middle, Right };

struct DesktopTouchEvent {
    enum class Type { Down, Motion, Up, Canceled };

    std::uint64_t fingerId;
    int x;
    int y;
    Type type;
};

class DesktopPlatform {
public:
    virtual ~DesktopPlatform() = default;

    virtual bool initialize(const std::string &title, int width, int height) = 0;
    virtual void pumpEvents() = 0;
    virtual void shutdown() = 0;

    virtual bool shouldQuit() const = 0;
    virtual bool isKeyDown(DesktopKey key) const = 0;
    virtual bool wasKeyPressed(DesktopKey key) const = 0;
    virtual bool wasMouseButtonPressed(DesktopMouseButton button) const = 0;
    virtual bool wasMouseButtonReleased(DesktopMouseButton button) const = 0;
    virtual void mousePosition(int *x, int *y) const = 0;
    virtual const std::vector<DesktopTouchEvent> &touchEvents() const = 0;
    // Wheel movement is frame-local and may be observed by both application
    // shortcuts and the UI. It is reset by pumpEvents(), not consumed by the
    // first caller.
    virtual float mouseWheelY() const = 0;

    virtual int windowWidth() const = 0;
    virtual int windowHeight() const = 0;
    virtual bool isFullscreen() const = 0;
    virtual void setFullscreen(bool enabled) = 0;
    virtual void *nativeWindowHandle() const = 0;
    virtual std::uint64_t ticks() const = 0;
    virtual void delay(std::uint32_t milliseconds) const = 0;
    virtual std::string applicationDirectory() const = 0;
    virtual std::string lastError() const = 0;
};

#endif /* ATG_ENGINE_SIM_DESKTOP_PLATFORM_H */
