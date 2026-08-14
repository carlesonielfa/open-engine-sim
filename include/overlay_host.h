#ifndef ATG_ENGINE_SIM_OVERLAY_HOST_H
#define ATG_ENGINE_SIM_OVERLAY_HOST_H

#include "engine_catalog.h"
#include "ui_button.h"

#include <vector>

class OverlayHost final : public UiElement {
public:
    enum class Kind {
        None,
        Controls,
        EnginePicker
    };

    void initialize(EngineSimApplication *app) override;
    void update(float dt) override;
    void render() override;
    void signal(UiElement *element, Event event) override;
    void onMouseClick(const Point &mouseLocal) override;
    void onMouseScroll(int mouseScroll) override;

    void present(Kind kind);
    void dismiss();
    Kind kind() const { return m_kind; }

private:
    UiButton *m_closeButton = nullptr;
    UiButton *m_githubButton = nullptr;
    UiButton *m_issuesButton = nullptr;
    UiButton *m_pickerScrollUpButton = nullptr;
    UiButton *m_pickerScrollDownButton = nullptr;
    std::vector<UiButton *> m_engineButtons;
    Kind m_kind = Kind::None;
    float m_pickerScrollOffset = 0.0f;
    float m_pickerMaxScrollOffset = 0.0f;

    Bounds viewportBounds() const;
    Bounds dialogBounds() const;
    void layoutControls(const Bounds &panel);
    void layoutEnginePicker(const Bounds &panel);
    void setChildrenVisible();
};

#endif
