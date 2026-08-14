#include "../include/info_cluster.h"

#include "../include/engine_sim_application.h"

#include <iomanip>
#include <sstream>

InfoCluster::InfoCluster()
    : m_engine(nullptr), m_projectInfoButton(nullptr), m_enginePickerButton(nullptr),
      m_fullscreenButton(nullptr), m_logMessage("Started") { }

InfoCluster::~InfoCluster() = default;

void InfoCluster::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);
    m_projectInfoButton = addElement<UiButton>(this);
    m_projectInfoButton->m_text = "INFO";
    m_projectInfoButton->m_fontSize = 16.0f;
    m_projectInfoButton->m_inverted = true;
    m_projectInfoButton->m_drawFrame = false;

    m_enginePickerButton = addElement<UiButton>(this);
    m_enginePickerButton->m_text = "SELECT";
    m_enginePickerButton->m_fontSize = 16.0f;
    m_enginePickerButton->m_inverted = true;
    m_enginePickerButton->m_drawFrame = false;

    m_fullscreenButton = addElement<UiButton>(this);
    m_fullscreenButton->m_text = "Fullscreen";
    m_fullscreenButton->m_fontSize = 16.0f;
    m_fullscreenButton->m_inverted = true;
    m_fullscreenButton->m_drawFrame = false;
}

void InfoCluster::destroy() { UiElement::destroy(); }

void InfoCluster::update(float dt) {
    Grid grid = { 6, 4 };
    const Bounds titleBounds = grid.get(m_bounds, 1, 0, 5, 2);
    const Bounds toolbar = titleBounds.verticalSplit(0.0f, 0.24f);
    m_enginePickerButton->m_bounds = toolbar.horizontalSplit(0.47f, 0.60f);
    m_projectInfoButton->m_bounds = toolbar.horizontalSplit(0.61f, 0.74f);
    m_fullscreenButton->m_bounds = toolbar.horizontalSplit(0.75f, 1.0f);
    UiElement::update(dt);
}

void InfoCluster::signal(UiElement *element, Event event) {
    if (event != Event::Clicked) return;
    if (element == m_fullscreenButton) m_app->toggleFullscreen();
    else if (element == m_projectInfoButton) m_app->showControlsOverlay();
    else if (element == m_enginePickerButton) m_app->showEnginePickerOverlay();
}

void InfoCluster::render() {
    Grid grid = { 6, 4 };
    const Bounds logoBounds = grid.get(m_bounds, 0, 0, 1, 2);
    drawFrame(logoBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    resetShader();
    const Point logoPosition = getRenderPoint(logoBounds.getPosition(Bounds::center));
    m_app->getShaders()->SetObjectTransform(
        ysMath::MatMult(
            ysMath::TranslationTransform(ysMath::LoadVector(logoPosition.x, logoPosition.y, 0.0f)),
            ysMath::ScaleTransform(ysMath::LoadVector(
                logoBounds.width() * 0.58f, logoBounds.height() * 0.72f, 1.0f))));
    m_app->getShaders()->SetBaseColor(m_app->getForegroundColor());
    m_app->drawModel("Logo", m_app->getShaders()->GetUiFlags(), 0x12);

    const Bounds titleBounds = grid.get(m_bounds, 1, 0, 5, 2);
    drawFrame(titleBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    const Bounds titleTextBounds = titleBounds.verticalSplit(0.62f, 1.0f);
    const Bounds subtitleBounds = titleBounds.verticalSplit(0.44f, 0.62f);
    const Bounds buildBounds = titleBounds.verticalSplit(0.28f, 0.44f);
    const Bounds toolbarBounds = titleBounds.verticalSplit(0.0f, 0.24f);
    const auto fittedHeight = [this](const std::string &text, float requestedHeight) {
        const float maximumWidth = m_bounds.width() * 0.80f;
        const float requestedWidth = m_app->getTextRenderer()->CalculateWidth(text, requestedHeight);
        return requestedWidth > maximumWidth ? requestedHeight * maximumWidth / requestedWidth : requestedHeight;
    };
    drawAlignedText("OPEN ENGINE SIMULATOR", titleTextBounds.inset(10.0f).move({ 0.0f, -21.0f }),
        fittedHeight("OPEN ENGINE SIMULATOR", 42.0f), Bounds::bl, Bounds::bl);
    drawAlignedText("ORIGINAL BY ANGETHEGREAT", subtitleBounds.inset(10.0f).move({ 0.0f, 3.0f }),
        fittedHeight("ORIGINAL BY ANGETHEGREAT", 24.0f), Bounds::tl, Bounds::tl);
    drawAlignedText("BUILD: v" + EngineSimApplication::getBuildVersion() + " // " __DATE__,
        buildBounds.inset(10.0f).move({ 0.0f, 4.0f }), 16.0f, Bounds::tl, Bounds::tl);
    drawFrame(toolbarBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    const Bounds engineInfoBounds = grid.get(m_bounds, 0, 2, 6, 1);
    drawFrame(engineInfoBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    drawAlignedText(m_engine != nullptr ? m_engine->getName() : "<NO ENGINE>",
        engineInfoBounds.horizontalSplit(0.0f, 0.66f).inset(10.0f), 24.0f, Bounds::lm, Bounds::lm);

    std::stringstream specs;
    if (m_engine != nullptr) {
        specs << std::fixed;
        if (m_engine->getDisplacement() < units::volume(1.0, units::L))
            specs << std::setprecision(0) << units::convert(m_engine->getDisplacement(), units::cc) << " cc -- ";
        else specs << std::setprecision(1) << units::convert(m_engine->getDisplacement(), units::L) << " L -- ";
        specs << std::setprecision(0) << units::convert(m_engine->getDisplacement(), units::cubic_inches) << " CI";
    }
    else specs << "N/A";
    drawAlignedText(specs.str(), engineInfoBounds.horizontalSplit(0.66f, 1.0f).inset(10.0f),
        24.0f, Bounds::rm, Bounds::rm);

    const Bounds messageBounds = grid.get(m_bounds, 0, 3, 6, 1);
    drawFrame(messageBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    drawAlignedText(m_logMessage, messageBounds.inset(10.0f), 24.0f, Bounds::lm, Bounds::lm);
    UiElement::render();
}
