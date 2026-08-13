#ifndef ATG_ENGINE_SIM_UI_MANAGER_H
#define ATG_ENGINE_SIM_UI_MANAGER_H

#include "ui_element.h"
#include "desktop_platform.h"

#include <unordered_map>

class EngineSimApplication;
class UiManager {
    public:
        UiManager();
        ~UiManager();

        void initialize(EngineSimApplication *app, DesktopPlatform *platform);
        void destroy();

        void update(float dt);
        void render();

        UiElement *getRoot() { return &m_root; }

    protected:
        struct TouchDrag {
            UiElement *element;
            Point elementPosition;
            Point startPosition;
        };

        UiElement m_root;

        UiElement *m_dragStart;
        UiElement *m_hover;
        Point m_mouse_p0;
        Point m_drag_p0;
        std::unordered_map<std::uint64_t, TouchDrag> m_touchDrags;

    protected:
        EngineSimApplication *m_app;
        DesktopPlatform *m_platform;
};

#endif /* ATG_ENGINE_SIM_UI_MANAGER_H */
