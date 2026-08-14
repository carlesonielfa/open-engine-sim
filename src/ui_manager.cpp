#include "../include/ui_manager.h"

UiManager::UiManager() {
    m_app = nullptr;
    m_platform = nullptr;
    m_dragStart = nullptr;
    m_hover = nullptr;
}

UiManager::~UiManager() {
    /* void */
}

void UiManager::initialize(EngineSimApplication *app, DesktopPlatform *platform) {
    m_app = app;
    m_platform = platform;
    m_root.initialize(app);
    m_overlayHost.initialize(app);
    m_overlayHost.setRenderLayer(0x100);
}

void UiManager::destroy() {
    m_root.destroy();
    m_overlayHost.destroy();
    m_hover = nullptr;
    m_dragStart = nullptr;
    m_touchDrags.clear();
    m_platform = nullptr;
    m_app = nullptr;
}

void UiManager::showControlsOverlay() { m_overlayHost.present(OverlayHost::Kind::Controls); }

void UiManager::showEnginePickerOverlay() { m_overlayHost.present(OverlayHost::Kind::EnginePicker); }

UiElement *UiManager::hitTest(const Point &position) {
    if (m_overlayHost.isVisible()) {
        if (UiElement *overlay = m_overlayHost.mouseOver(position)) return overlay;
    }
    return m_root.mouseOver(position);
}

void UiManager::update(float dt) {
    m_root.update(dt);
    m_overlayHost.update(dt);

    const auto &touchEvents = m_platform->touchEvents();
    for (const DesktopTouchEvent &touch : touchEvents) {
        const Point position = { static_cast<float>(touch.x), static_cast<float>(touch.y) };
        const auto existing = m_touchDrags.find(touch.fingerId);

        if (touch.type == DesktopTouchEvent::Type::Down) {
            UiElement *element = hitTest(position);
            if (element == nullptr) continue;

            m_touchDrags[touch.fingerId] = {
                element,
                element->getLocalPosition(),
                position
            };
            element->onMouseDown(element->worldToLocal(position));
        }
        else if (existing != m_touchDrags.end()) {
            TouchDrag &drag = existing->second;
            if (touch.type == DesktopTouchEvent::Type::Motion) {
                drag.element->onDrag(drag.elementPosition, drag.startPosition, position);
            }
            else {
                drag.element->onMouseUp(drag.element->worldToLocal(position));
                if (touch.type == DesktopTouchEvent::Type::Up &&
                    hitTest(position) == drag.element)
                {
                    drag.element->onMouseClick(drag.element->worldToLocal(position));
                }
                m_touchDrags.erase(existing);
            }
        }
    }

    int mouse_x, mouse_y;
    m_platform->mousePosition(&mouse_x, &mouse_y);

    Point mousePos = { (float)mouse_x, (float)mouse_y };
    UiElement *newHover = hitTest(mousePos);
    if (newHover != m_hover) {
        if (m_hover != nullptr) m_hover->onMouseLeave();
        if (newHover != nullptr) newHover->onMouseOver(mousePos);
        m_hover = newHover;
    }

    // Handle direct touch above. Suppressing mouse events in a frame that also
    // received touch prevents browsers from applying the same tap twice.
    if (!touchEvents.empty()) return;

    if (m_platform->wasMouseButtonPressed(DesktopMouseButton::Left)) {
        m_dragStart = m_hover;
        m_mouse_p0 = mousePos;
        if (m_dragStart != nullptr) {
            m_drag_p0 = m_dragStart->getLocalPosition();
            m_dragStart->onMouseDown(m_dragStart->worldToLocal(mousePos));
        }
    }
    // Browser event dispatch may deliver a quick press and release in one
    // frame. Handle both transitions rather than discarding the release.
    if (m_platform->wasMouseButtonReleased(DesktopMouseButton::Left)) {
        UiElement *dragRelease = m_hover;

        if (m_dragStart != nullptr) m_dragStart->onMouseUp(m_dragStart->worldToLocal(mousePos));

        if (dragRelease != nullptr && m_dragStart == dragRelease) {
            m_dragStart->onMouseClick(m_dragStart->worldToLocal(mousePos));
        }

        m_dragStart = nullptr;
    }

    const int mouseScroll = static_cast<int>(m_platform->mouseWheelY());
    if (mouseScroll != 0) {
        if (m_hover != nullptr) {
            m_hover->onMouseScroll(mouseScroll);
        }
    }

    if (m_dragStart != nullptr) {
        m_dragStart->onDrag(m_drag_p0, m_mouse_p0, mousePos);
    }
}

void UiManager::render() {
    m_root.render();
    m_overlayHost.render();
}
