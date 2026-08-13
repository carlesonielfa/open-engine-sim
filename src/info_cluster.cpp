#include "../include/info_cluster.h"

#include "../include/engine_sim_application.h"

#include <sstream>
#include <iomanip>

InfoCluster::InfoCluster() {
    m_engine = nullptr;
    m_fullscreenButton = nullptr;
    m_logMessage = "Started";
}

InfoCluster::~InfoCluster() {
    /* void */
}

void InfoCluster::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);
    m_fullscreenButton = addElement<UiButton>(this);
    m_fullscreenButton->m_text = "Fullscreen";
    m_fullscreenButton->m_fontSize = 16.0f;
}

void InfoCluster::destroy() {
    UiElement::destroy();
}

void InfoCluster::update(float dt) {
    Grid grid = { 6, 4 };
    const Bounds titleBounds = grid.get(m_bounds, 1, 0, 5, 2);
    const Bounds toolbar = titleBounds.verticalSplit(0.0f, 0.24f).inset(7.0f);
    m_fullscreenButton->m_bounds = toolbar.horizontalSplit(0.70f, 1.0f);
    UiElement::update(dt);
}

void InfoCluster::signal(UiElement *element, Event event) {
    if (element == m_fullscreenButton && event == Event::Clicked) {
        m_app->toggleFullscreen();
    }
}

void InfoCluster::render() {
    Grid grid;
    grid.h_cells = 6;
    grid.v_cells = 4;

    const Bounds logoBounds = grid.get(m_bounds, 0, 0, 1, 2);
    drawFrame(logoBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    // The original Blender scene supplies the logo in model-local space.
    // Place it inside the existing dashboard frame using the portable OBJ
    // loader, independent of Blender or Delta at runtime.
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

    // Keep the title compact and reserve a distinct bottom strip for future
    // dashboard actions (currently fullscreen). The explicit proportions
    // avoid wasting a full grid row between the build metadata and controls.
    const Bounds titleTextBounds = titleBounds.verticalSplit(0.62f, 1.0f);
    const Bounds subtitleBounds = titleBounds.verticalSplit(0.44f, 0.62f);
    const Bounds buildBounds = titleBounds.verticalSplit(0.28f, 0.44f);
    const Bounds toolbarBounds = titleBounds.verticalSplit(0.0f, 0.24f);
    const auto fittedHeight = [this](const std::string &text, float requestedHeight) {
        // Reserve a meaningful right margin even when a fork name is longer
        // than the upstream heading that originally occupied this panel.
        const float maximumWidth = m_bounds.width() * 0.80f;
        const float requestedWidth = m_app->getTextRenderer()->CalculateWidth(text, requestedHeight);
        return requestedWidth > maximumWidth
            ? requestedHeight * maximumWidth / requestedWidth
            : requestedHeight;
    };
    drawAlignedText(
        "OPEN ENGINE SIMULATOR",
        titleTextBounds.inset(10.0f).move({ 0.0f, -21.0f }),
        fittedHeight("OPEN ENGINE SIMULATOR", 42.0f),
        Bounds::bl,
        Bounds::bl);
    drawAlignedText(
        "ORIGINAL BY ANGETHEGREAT",
        subtitleBounds.inset(10.0f).move({ 0.0f, 3.0f }),
        fittedHeight("ORIGINAL BY ANGETHEGREAT", 24.0f),
        Bounds::tl,
        Bounds::tl);
    drawAlignedText(
        "BUILD: v" + EngineSimApplication::getBuildVersion() + " // " __DATE__,
        buildBounds.inset(10.0f).move({ 0.0f, 4.0f }),
        16.0f,
        Bounds::tl,
        Bounds::tl);

    drawFrame(toolbarBounds, 1.0f,
        m_app->getForegroundColor(), m_app->getBackgroundColor());

    const Bounds engineInfoBounds = grid.get(m_bounds, 0, 2, 6, 1);
    drawFrame(engineInfoBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    drawAlignedText(
        (m_engine != nullptr) ? m_engine->getName() : "<NO ENGINE>",
        engineInfoBounds.inset(10.0f),
        24.0f,
        Bounds::lm,
        Bounds::lm);

    std::stringstream ss;
    if (m_engine != nullptr) {
        ss << std::fixed;

        if (m_engine->getDisplacement() < units::volume(1.0, units::L)) {
            ss << std::setprecision(0) << units::convert(m_engine->getDisplacement(), units::cc) << " cc -- ";
        }
        else {
            ss << std::setprecision(1) << units::convert(m_engine->getDisplacement(), units::L) << " L -- ";
        }

        ss << std::setprecision(0) << units::convert(m_engine->getDisplacement(), units::cubic_inches) << " CI";
    }
    else {
        ss << "N/A";
    }

    drawAlignedText(
        ss.str(),
        engineInfoBounds.inset(10.0f),
        24.0f,
        Bounds::rm,
        Bounds::rm);

    const Bounds infoMessagesBounds = grid.get(m_bounds, 0, 3, 6, 1);
    drawFrame(infoMessagesBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    drawAlignedText(
        m_logMessage,
        infoMessagesBounds.inset(10.0f),
        24.0f,
        Bounds::lm,
        Bounds::lm);

    UiElement::render();
}
