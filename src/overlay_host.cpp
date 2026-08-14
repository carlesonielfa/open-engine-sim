#include "../include/overlay_host.h"

#include "../include/engine_sim_application.h"
#include "../include/ui_utilities.h"

#include <algorithm>
#include <array>

namespace {
// The controls dialog deliberately centralizes all spacing here. Adjust these
// values to retune the rhythm of every section without touching rendering code.
struct ControlsLayout {
    float panelInset = 30.0f;
    float titleHeight = 32.0f;
    float sectionTitleHeight = 20.0f;
    float rowHeight = 28.0f;
    float titleGap = 28.0f;
    float sectionHeaderGap = 16.0f;
    float rowGap = 16.0f;
    float sectionGap = 30.0f;
    float keyHorizontalGap = 7.0f;
    float keyLabelGap = 18.0f;
    float projectColumnStart = 0.58f;
    float projectColumnEnd = 0.98f;
    float projectTopOffset = 155.0f;
    float projectTextGap = 12.0f;
    float footerButtonHeight = 34.0f;
    float footerButtonGap = 24.0f;
    float closeButtonWidth = 0.22f;
    float closeButtonHeight = 54.0f;
};

constexpr ControlsLayout kControlsLayout{};

struct ControlAction {
    std::array<const char *, 4> keys;
    int keyCount;
    const char *label;
    const char *hint;
};

constexpr ControlAction kStartActions[] = {
    {{ "A", nullptr, nullptr, nullptr }, 1, "ENABLE IGNITION", "OR TAP IGNITION"},
    {{ "S", nullptr, nullptr, nullptr }, 1, "HOLD STARTER", "OR HOLD STARTER"},
};
constexpr ControlAction kDriveActions[] = {
    {{ "Q", "W", "E", "R" }, 4, "ADJUST THROTTLE", "OR DRAG THROTTLE"},
    {{ "UP", "DOWN", nullptr, nullptr }, 2, "CHANGE GEAR", "OR TAP ARROWS"},
    {{ "SPACE", nullptr, nullptr, nullptr }, 1, "HOLD CLUTCH", ""},
};

Bounds controlsContent(const Bounds &panel) { return panel.inset(kControlsLayout.panelInset); }

Bounds projectColumn(const Bounds &content) {
    return content.horizontalSplit(kControlsLayout.projectColumnStart, kControlsLayout.projectColumnEnd);
}

class VerticalLayoutCursor {
public:
    VerticalLayoutCursor(const Bounds &column, float y) : m_column(column), m_y(y) { }

    Bounds take(float height) {
        const Bounds result(m_column.width(), height, { m_column.left(), m_y }, Bounds::tl);
        m_y -= height;
        return result;
    }

    void gap(float amount) { m_y -= amount; }

private:
    Bounds m_column;
    float m_y;
};
}

void OverlayHost::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);
    m_closeButton = addElement<UiButton>(this);
    m_closeButton->m_text = "CLOSE";
    m_closeButton->m_fontSize = 16.0f;
    m_closeButton->m_inverted = true;
    m_closeButton->m_drawFrame = false;

    m_githubButton = addElement<UiButton>(this);
    m_githubButton->m_text = "GITHUB";
    m_githubButton->m_fontSize = 14.0f;
    m_githubButton->m_inverted = true;
    m_githubButton->m_drawFrame = false;

    m_issuesButton = addElement<UiButton>(this);
    m_issuesButton->m_text = "ISSUES";
    m_issuesButton->m_fontSize = 14.0f;
    m_issuesButton->m_inverted = true;
    m_issuesButton->m_drawFrame = false;

    m_pickerScrollUpButton = addElement<UiButton>(this);
    m_pickerScrollUpButton->m_text = "UP";
    m_pickerScrollUpButton->m_fontSize = 14.0f;
    m_pickerScrollUpButton->m_inverted = true;

    m_pickerScrollDownButton = addElement<UiButton>(this);
    m_pickerScrollDownButton->m_text = "DOWN";
    m_pickerScrollDownButton->m_fontSize = 14.0f;
    m_pickerScrollDownButton->m_inverted = true;

    for (const EngineCatalogEntry &entry : engineCatalog()) {
        UiButton *button = addElement<UiButton>(this);
        button->m_text = entry.name;
        button->m_fontSize = 15.0f;
        button->m_inverted = true;
        m_engineButtons.push_back(button);
    }
    dismiss();
}

Bounds OverlayHost::viewportBounds() const {
    return Bounds(
        static_cast<float>(m_app->getScreenWidth()),
        static_cast<float>(m_app->getScreenHeight()),
        { 0.0f, static_cast<float>(m_app->getScreenHeight()) });
}

Bounds OverlayHost::dialogBounds() const {
    const Bounds viewport = viewportBounds();
    const float inset = std::max(20.0f, std::min(viewport.width(), viewport.height()) * 0.07f);
    return viewport.inset(inset);
}

void OverlayHost::present(Kind kind) {
    if (m_kind == kind) {
        dismiss();
        return;
    }
    m_kind = kind;
    m_pickerScrollOffset = 0.0f;
    setVisible(true);
    setChildrenVisible();
}

void OverlayHost::dismiss() {
    m_kind = Kind::None;
    setVisible(false);
    setChildrenVisible();
}

void OverlayHost::setChildrenVisible() {
    const bool controls = m_kind == Kind::Controls;
    const bool picker = m_kind == Kind::EnginePicker;
    if (m_closeButton != nullptr) m_closeButton->setVisible(controls || picker);
    if (m_githubButton != nullptr) m_githubButton->setVisible(controls);
    if (m_issuesButton != nullptr) m_issuesButton->setVisible(controls);
    if (m_pickerScrollUpButton != nullptr) m_pickerScrollUpButton->setVisible(picker);
    if (m_pickerScrollDownButton != nullptr) m_pickerScrollDownButton->setVisible(picker);
    for (UiButton *button : m_engineButtons) button->setVisible(picker);
}

void OverlayHost::update(float dt) {
    if (m_app->getPlatform()->wasKeyPressed(DesktopKey::F1)) present(Kind::Controls);
    if (m_app->getPlatform()->wasKeyPressed(DesktopKey::F2)) present(Kind::EnginePicker);
    if (m_kind == Kind::None) return;

    const Bounds viewport = viewportBounds();
    const Bounds panel = dialogBounds();
    m_mouseBounds = viewport;
    m_checkMouse = true;
    if (m_kind == Kind::Controls) layoutControls(panel);
    else layoutEnginePicker(panel);
    UiElement::update(dt);
}

void OverlayHost::layoutControls(const Bounds &panel) {
    const Bounds content = controlsContent(panel);
    const float closeWidth = content.width() * kControlsLayout.closeButtonWidth;
    m_closeButton->m_bounds = Bounds(
        closeWidth,
        kControlsLayout.closeButtonHeight,
        { content.right() - closeWidth, content.top() - 8.0f }, Bounds::tl);

    const Bounds project = projectColumn(content);
    const float buttonWidth = (project.width() - kControlsLayout.footerButtonGap) / 2.0f;
    m_githubButton->m_bounds = Bounds(
        buttonWidth, kControlsLayout.footerButtonHeight,
        { project.left(), content.bottom() }, Bounds::bl);
    m_issuesButton->m_bounds = Bounds(
        buttonWidth, kControlsLayout.footerButtonHeight,
        { project.right(), content.bottom() }, Bounds::br);
}

void OverlayHost::layoutEnginePicker(const Bounds &panel) {
    const Bounds content = panel.inset(28.0f);
    const Bounds listBounds = content.verticalSplit(0.12f, 0.84f);
    const int columns = listBounds.width() >= 700.0f ? 3 : 2;
    constexpr float headerHeight = 28.0f;
    constexpr float rowHeight = 44.0f;
    float contentHeight = 0.0f;
    std::string previousGroup;
    int indexInGroup = 0;
    for (const EngineCatalogEntry &entry : engineCatalog()) {
        if (entry.group != previousGroup) {
            contentHeight += headerHeight;
            previousGroup = entry.group;
            indexInGroup = 0;
        }
        if (indexInGroup % columns == 0) contentHeight += rowHeight;
        ++indexInGroup;
    }
    m_pickerMaxScrollOffset = std::max(0.0f, contentHeight - listBounds.height());
    m_pickerScrollOffset = std::min(m_pickerScrollOffset, m_pickerMaxScrollOffset);
    m_closeButton->m_bounds = content.verticalSplit(0.88f, 0.98f).horizontalSplit(0.78f, 1.0f);
    m_pickerScrollUpButton->m_bounds = content.verticalSplit(0.88f, 0.98f).horizontalSplit(0.50f, 0.63f);
    m_pickerScrollDownButton->m_bounds = content.verticalSplit(0.88f, 0.98f).horizontalSplit(0.64f, 0.77f);

    float y = listBounds.top() - m_pickerScrollOffset;
    previousGroup.clear();
    indexInGroup = 0;
    for (size_t i = 0; i < engineCatalog().size(); ++i) {
        const EngineCatalogEntry &entry = engineCatalog()[i];
        if (entry.group != previousGroup) {
            previousGroup = entry.group;
            indexInGroup = 0;
            y -= headerHeight;
        }
        const int column = indexInGroup % columns;
        if (column == 0) y -= rowHeight;
        const float buttonWidth = listBounds.width() / columns;
        m_engineButtons[i]->m_bounds = Bounds(
            buttonWidth - 8.0f, rowHeight - 8.0f,
            { listBounds.left() + column * buttonWidth + 4.0f, y + rowHeight - 4.0f }, Bounds::tl);
        const Bounds &bounds = m_engineButtons[i]->m_bounds;
        m_engineButtons[i]->setVisible(bounds.bottom() >= listBounds.bottom() && bounds.top() <= listBounds.top());
        ++indexInGroup;
    }
}

void OverlayHost::signal(UiElement *element, Event event) {
    if (event != Event::Clicked) return;
    if (element == m_closeButton) dismiss();
    else if (element == m_githubButton) m_app->getPlatform()->openUrl("https://github.com/carlesonielfa/open-engine-sim");
    else if (element == m_issuesButton) m_app->getPlatform()->openUrl("https://github.com/carlesonielfa/open-engine-sim/issues");
    else if (element == m_pickerScrollUpButton) m_pickerScrollOffset = std::max(0.0f, m_pickerScrollOffset - 180.0f);
    else if (element == m_pickerScrollDownButton) m_pickerScrollOffset = std::min(m_pickerMaxScrollOffset, m_pickerScrollOffset + 180.0f);
    else {
        for (size_t i = 0; i < m_engineButtons.size(); ++i) {
            if (element == m_engineButtons[i]) {
                m_app->requestEngineScript(engineCatalog()[i].relativeScriptPath);
                dismiss();
                break;
            }
        }
    }
}

void OverlayHost::onMouseClick(const Point &) { dismiss(); }

void OverlayHost::onMouseScroll(int mouseScroll) {
    if (m_kind != Kind::EnginePicker) return;
    m_pickerScrollOffset = std::clamp(m_pickerScrollOffset - mouseScroll * 45.0f, 0.0f, m_pickerMaxScrollOffset);
}

void OverlayHost::render() {
    if (m_kind == Kind::None) return;
    const Bounds viewport = viewportBounds();
    const Bounds panel = dialogBounds();
    const Bounds content = panel.inset(30.0f);
    const ysVector foreground = m_app->getForegroundColor();
    const ysVector background = m_app->getBackgroundColor();
    const ysVector secondary = mix(foreground, background, 0.45f);
    const ysVector scrim(background.x, background.y, background.z, 0.78f);

    // Geometry with a lower layer draws behind text and child controls. Keep
    // this ordering inside the host instead of relying on a dashboard
    // cluster's render-layer side effects.
    drawBox(viewport, scrim, -0x20);
    drawFrame(panel, 2.0f, foreground, background, true, -0x10);

    if (m_kind == Kind::EnginePicker) {
        const Bounds listBounds = content.verticalSplit(0.12f, 0.84f);
        const int columns = listBounds.width() >= 700.0f ? 3 : 2;
        drawAlignedText("SELECT ENGINE", content.verticalSplit(0.90f, 0.98f), 28.0f, Bounds::lm, Bounds::lm);
        m_app->getTextRenderer()->SetColor(secondary);
        drawAlignedText("PACKAGED ENGINES", content.verticalSplit(0.02f, 0.09f), 16.0f, Bounds::lm, Bounds::lm);
        m_app->getTextRenderer()->SetColor(foreground);

        float y = listBounds.top() - m_pickerScrollOffset;
        std::string previousGroup;
        int indexInGroup = 0;
        for (const EngineCatalogEntry &entry : engineCatalog()) {
            if (entry.group != previousGroup) {
                previousGroup = entry.group;
                indexInGroup = 0;
                y -= 28.0f;
                const Bounds heading(listBounds.width(), 28.0f, { listBounds.left(), y + 28.0f }, Bounds::tl);
                if (heading.bottom() >= listBounds.bottom() && heading.top() <= listBounds.top()) {
                    m_app->getTextRenderer()->SetColor(secondary);
                    drawAlignedText(entry.group, heading, 16.0f, Bounds::lm, Bounds::lm);
                    m_app->getTextRenderer()->SetColor(foreground);
                }
            }
            if (indexInGroup % columns == 0) y -= 44.0f;
            ++indexInGroup;
        }
    }
    else {
        const Bounds controls = controlsContent(panel);
        const Bounds leftColumn = controls.horizontalSplit(0.0f, kControlsLayout.projectColumnStart - 0.04f);
        const auto drawKey = [&](const char *key, Point &cursor, float baseline) {
            const float width = m_app->getTextRenderer()->CalculateWidth(key, 14.0f) + 18.0f;
            const Bounds keyBounds(width, kControlsLayout.rowHeight, { cursor.x, baseline }, Bounds::bl);
            drawFrame(keyBounds, 1.0f, foreground, foreground, true, -0x09);
            m_app->getTextRenderer()->SetColor(background);
            drawCenteredText(key, keyBounds, 14.0f, Bounds::center);
            m_app->getTextRenderer()->SetColor(foreground);
            cursor.x += width + kControlsLayout.keyHorizontalGap;
        };
        const auto drawAction = [&](const Bounds &row, const ControlAction &action) {
            Point cursor(row.left(), row.center_v() - kControlsLayout.rowHeight / 2.0f);
            for (int i = 0; i < action.keyCount; ++i) drawKey(action.keys[i], cursor, cursor.y);
            drawAlignedText(action.label, Bounds(row.right() - cursor.x, kControlsLayout.rowHeight, cursor, Bounds::bl),
                16.0f, Bounds::lm, Bounds::lm);
            cursor.x += m_app->getTextRenderer()->CalculateWidth(action.label, 16.0f) + kControlsLayout.keyLabelGap;
            if (action.hint[0] != '\0') {
                m_app->getTextRenderer()->SetColor(secondary);
                drawAlignedText(action.hint, Bounds(row.right() - cursor.x, kControlsLayout.rowHeight, cursor, Bounds::bl),
                    14.0f, Bounds::lm, Bounds::lm);
                m_app->getTextRenderer()->SetColor(foreground);
            }
        };
        const auto drawSection = [&](VerticalLayoutCursor &cursor, const char *title,
                                     const ControlAction *actions, int count) {
            drawAlignedText(title, cursor.take(kControlsLayout.sectionTitleHeight),
                kControlsLayout.sectionTitleHeight, Bounds::lm, Bounds::lm);
            cursor.gap(kControlsLayout.sectionHeaderGap);
            for (int i = 0; i < count; ++i) {
                drawAction(cursor.take(kControlsLayout.rowHeight), actions[i]);
                if (i + 1 < count) cursor.gap(kControlsLayout.rowGap);
            }
            cursor.gap(kControlsLayout.sectionGap);
        };

        drawAlignedText("CONTROLS", Bounds(leftColumn.width(), kControlsLayout.titleHeight,
            { leftColumn.left(), controls.top() }, Bounds::tl), kControlsLayout.titleHeight, Bounds::lm, Bounds::lm);
        VerticalLayoutCursor left(leftColumn, controls.top() - kControlsLayout.titleHeight - kControlsLayout.titleGap);
        drawSection(left, "START", kStartActions, 2);
        drawSection(left, "DRIVE", kDriveActions, 3);

        const ControlAction extras[] = {
            {{ "D", nullptr, nullptr, nullptr }, 1, "DYNO", ""},
            {{ "H", nullptr, nullptr, nullptr }, 1, "HOLD", ""},
            {{ "F", nullptr, nullptr, nullptr }, 1, "FULLSCREEN", ""},
            {{ "F1", nullptr, nullptr, nullptr }, 1, "GUIDE", ""},
            {{ "F2", nullptr, nullptr, nullptr }, 1, "ENGINES", ""},
        };
        drawAlignedText("EXTRAS", left.take(kControlsLayout.sectionTitleHeight),
            kControlsLayout.sectionTitleHeight, Bounds::lm, Bounds::lm);
        left.gap(kControlsLayout.sectionHeaderGap);
        const Bounds extrasRow = left.take(kControlsLayout.rowHeight);
        Point extrasCursor(extrasRow.left(), extrasRow.center_v() - kControlsLayout.rowHeight / 2.0f);
        for (const ControlAction &extra : extras) {
            drawKey(extra.keys[0], extrasCursor, extrasCursor.y);
            drawAlignedText(extra.label, Bounds(extrasRow.right() - extrasCursor.x,
                kControlsLayout.rowHeight, extrasCursor, Bounds::bl), 14.0f, Bounds::lm, Bounds::lm);
            extrasCursor.x += m_app->getTextRenderer()->CalculateWidth(extra.label, 14.0f) + kControlsLayout.keyLabelGap;
        }

        const Bounds project = projectColumn(controls);
        VerticalLayoutCursor projectCursor(project, controls.bottom() + kControlsLayout.projectTopOffset);
        drawAlignedText("PROJECT", projectCursor.take(kControlsLayout.sectionTitleHeight),
            kControlsLayout.sectionTitleHeight, Bounds::lm, Bounds::lm);
        projectCursor.gap(kControlsLayout.projectTextGap);
        m_app->getTextRenderer()->SetColor(secondary);
        drawAlignedText("OPEN-SOURCE, CROSS-PLATFORM ENGINE SOUND SIMULATOR.",
            projectCursor.take(kControlsLayout.rowHeight), 14.0f, Bounds::lm, Bounds::lm);
        projectCursor.gap(kControlsLayout.projectTextGap);
        drawAlignedText("HAVING TROUBLE? REPORT IT ON GITHUB.",
            projectCursor.take(kControlsLayout.rowHeight), 14.0f, Bounds::lm, Bounds::lm);
        m_app->getTextRenderer()->SetColor(foreground);
    }
    UiElement::render();
}
