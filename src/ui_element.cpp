#include "../include/ui_element.h"

#include "../include/engine_sim_application.h"

#include <assert.h>

UiElement::UiElement() {
    m_app = nullptr;
    m_parent = nullptr;
    m_signalTarget = nullptr;
    m_checkMouse = false;
    m_disabled = false;
    m_index = -1;

    m_draggable = false;
    m_mouseOver = false;
    m_mouseHeld = false;
    m_visible = true;
    m_renderLayer = 0;
}

UiElement::~UiElement() {
    /* void */
}

void UiElement::initialize(EngineSimApplication *app) {
    m_app = app;
}

void UiElement::destroy() {
    for (UiElement *child : m_children) {
        child->destroy();
        delete child;
    }

    m_children.clear();

    // destroy() is also used as a reset before reinitialization. Interaction
    // state must not survive the child tree it referred to.
    m_mouseOver = false;
    m_mouseHeld = false;
    m_parent = nullptr;
    m_signalTarget = nullptr;
    m_index = -1;
}

void UiElement::update(float dt) {
    for (UiElement *child : m_children) {
        child->update(dt);
    }
}

void UiElement::render() {
    for (UiElement *child : m_children) {
        if (!child->isVisible()) continue;
        child->render();
    }
}

void UiElement::signal(UiElement *element, Event event) {
    /* void */
}

void UiElement::onMouseDown(const Point &mouseLocal) {
    m_mouseHeld = true;
}

void UiElement::onMouseUp(const Point &mouseLocal) {
    m_mouseHeld = false;
}

void UiElement::onMouseClick(const Point &mouseLocal) {
    signal(Event::Clicked);
}

void UiElement::onDrag(const Point &p0, const Point &mouse0, const Point &mouse) {
    if (m_draggable) {
        m_localPosition = p0 + (mouse - mouse0);
    }
}

void UiElement::onMouseOver(const Point &mouseLocal) {
    m_mouseOver = true;
}

void UiElement::onMouseLeave() {
    m_mouseOver = false;
}

void UiElement::onMouseScroll(int mouseScroll) {
    /* void */
}

UiElement *UiElement::mouseOver(const Point &mouseLocal) {
    if (m_disabled) return nullptr;

    const int n = (int)getChildCount();
    for (int i = n - 1; i >= 0; --i) {
        UiElement *child = m_children[i];
        if (!child->isVisible()) continue;
        UiElement *clickedElement = child->mouseOver(mouseLocal - child->m_localPosition);
        if (clickedElement != nullptr) {
            return clickedElement;
        }
    }

    return (m_checkMouse && m_mouseBounds.overlaps(mouseLocal))
        ? this
        : nullptr;
}

Point UiElement::getWorldPosition() const {
    return (m_parent != nullptr)
        ? m_parent->getWorldPosition() + m_localPosition
        : m_localPosition;
}

int UiElement::getEffectiveRenderLayer() const {
    return m_renderLayer + ((m_parent != nullptr) ? m_parent->getEffectiveRenderLayer() : 0);
}

void UiElement::setLocalPosition(const Point &p, const Point &ref) {
    const Point current = m_bounds.getPosition(ref) + m_localPosition;
    m_localPosition += (p - current);
}

void UiElement::bringToFront(UiElement *element) {
    assert(element->m_parent == this);

    m_children.erase(m_children.begin() + element->m_index);
    m_children.push_back(element);

    int i = 0;
    for (UiElement *element : m_children) {
        element->m_index = i++;
    }
}

void UiElement::activate() {
    if (m_parent != nullptr) {
        m_parent->bringToFront(this);
        m_parent->activate();
    }
}

void UiElement::signal(Event event) {
    if (m_signalTarget == nullptr) return;

    m_signalTarget->signal(this, event);
}

float UiElement::pixelsToUnits(float length) const {
    return length;// m_app->pixelsToUnits(length);
}

Point UiElement::pixelsToUnits(const Point &p) const {
    return { pixelsToUnits(p.x), pixelsToUnits(p.y) };
}

float UiElement::unitsToPixels(float x) const {
    return x; // m_app->unitsToPixels(x);
}

Point UiElement::unitsToPixels(const Point &p) const {
    return { unitsToPixels(p.x), unitsToPixels(p.y) };
}

Point UiElement::getRenderPoint(const Point &p) const {
    const Point offset(
            -(float)m_app->getScreenWidth() / 2,
            -(float)m_app->getScreenHeight() / 2);
    const Point posPixels = localToWorld(p) + offset;

    return pixelsToUnits(posPixels);
}

Bounds UiElement::getRenderBounds(const Bounds &b) const {
    return { getRenderPoint(b.m0), getRenderPoint(b.m1) };
}

Bounds UiElement::unitsToPixels(const Bounds &b) const {
    return { unitsToPixels(b.m0), unitsToPixels(b.m1) };
}

void UiElement::resetShader() {
    m_app->getShaders()->ResetBaseColor();
    m_app->getShaders()->SetObjectTransform(ysMath::LoadIdentity());
}

void UiElement::drawFrame(
        const Bounds &bounds,
        float thickness,
        const ysVector &frameColor,
        const ysVector &fillColor,
        bool fill,
        int layer)
{
    GeometryGenerator *generator = m_app->getGeometryGenerator();

    const Bounds worldBounds = getRenderBounds(bounds);
    const Point position = worldBounds.getPosition(Bounds::center);

    GeometryGenerator::FrameParameters params;
    params.frameWidth = worldBounds.width();
    params.frameHeight = worldBounds.height();
    params.lineWidth = pixelsToUnits(thickness);
    params.x = position.x;
    params.y = position.y;

    GeometryGenerator::Line2dParameters lineParams;
    lineParams.lineWidth = worldBounds.height();
    lineParams.y0 = lineParams.y1 = worldBounds.getPosition(Bounds::center).y;
    lineParams.x0 = worldBounds.left();
    lineParams.x1 = worldBounds.right();

    GeometryGenerator::GeometryIndices frame, body;

    resetShader();
    if (fill) {
        generator->startShape();
        generator->generateLine2d(lineParams);
        generator->endShape(&body);

        m_app->getShaders()->SetBaseColor(fillColor);
        m_app->drawGenerated(body, layer + getEffectiveRenderLayer(), m_app->getShaders()->GetUiFlags());
    }

    generator->startShape();
    generator->generateFrame(params);
    generator->endShape(&frame);

    m_app->getShaders()->SetBaseColor(frameColor);
    m_app->drawGenerated(frame, layer + getEffectiveRenderLayer(), m_app->getShaders()->GetUiFlags());
}

void UiElement::drawBox(const Bounds &bounds, const ysVector &fillColor, int layer) {
    GeometryGenerator *generator = m_app->getGeometryGenerator();
    const Bounds worldBounds = getRenderBounds(bounds);

    GeometryGenerator::Line2dParameters lineParams;
    lineParams.lineWidth = worldBounds.height();
    lineParams.y0 = lineParams.y1 = worldBounds.getPosition(Bounds::center).y;
    lineParams.x0 = worldBounds.left();
    lineParams.x1 = worldBounds.right();

    GeometryGenerator::GeometryIndices body;
    generator->startShape();
    generator->generateLine2d(lineParams);
    generator->endShape(&body);

    resetShader();
    m_app->getShaders()->SetBaseColor(fillColor);
    m_app->drawGenerated(body, layer + getEffectiveRenderLayer(), m_app->getShaders()->GetUiFlags());
}

void UiElement::drawText(
        const std::string &s,
        const Bounds &bounds,
        float height,
        const Point &ref)
{
    const Bounds renderBounds = unitsToPixels(getRenderBounds(bounds));
    const Point origin = renderBounds.getPosition(ref);

    TextRenderer *textRenderer = m_app->getTextRenderer();
    const int oldLayer = textRenderer->renderLayer();
    textRenderer->setRenderLayer(getEffectiveRenderLayer());
    textRenderer->RenderText(s, origin.x, origin.y - height / 4, height);
    textRenderer->setRenderLayer(oldLayer);
}

void UiElement::drawAlignedText(
        const std::string &s,
        const Bounds &bounds,
        float height,
        const Point &ref,
        const Point &refText)
{
    const Bounds renderBounds = unitsToPixels(getRenderBounds(bounds));
    const Point origin = renderBounds.getPosition(ref);

    const float textWidth = m_app->getTextRenderer()->CalculateWidth(s, height);
    const float textHeight = height;

    const Bounds textBounds(
        textWidth,
        textHeight,
        { 0.0f, textHeight - textHeight * 0.25f },
        Bounds::tl);
    const Point r = textBounds.getPosition(refText);

    TextRenderer *textRenderer = m_app->getTextRenderer();
    const int oldLayer = textRenderer->renderLayer();
    textRenderer->setRenderLayer(getEffectiveRenderLayer());
    textRenderer->RenderText(s, origin.x - r.x, origin.y - r.y, height);
    textRenderer->setRenderLayer(oldLayer);
}

void UiElement::drawCenteredText(
        const std::string &s,
        const Bounds &bounds,
        float height,
        const Point &ref)
{
    const Bounds renderBounds = unitsToPixels(getRenderBounds(bounds));
    const Point origin = renderBounds.getPosition(ref);

    const float width = m_app->getTextRenderer()->CalculateWidth(s, height);
    TextRenderer *textRenderer = m_app->getTextRenderer();
    const int oldLayer = textRenderer->renderLayer();
    textRenderer->setRenderLayer(getEffectiveRenderLayer());
    textRenderer->RenderText(s, origin.x - width / 2, origin.y - height / 4, height);
    textRenderer->setRenderLayer(oldLayer);
}
