#include "../include/load_simulation_cluster.h"

#include "../include/engine_sim_application.h"
#include "../include/geometry_generator.h"
#include "../include/ui_utilities.h"

#include <sstream>
#include <iomanip>
#include <climits>

LoadSimulationCluster::LoadSimulationCluster() {
    m_torqueGauge = nullptr;
    m_hpGauge = nullptr;
    m_simulator = nullptr;
    m_clutchPressureGauge = nullptr;
    m_filteredHorsepower = 0.0;
    m_filteredTorque = 0.0;
    m_peakHorsepower = 0.0;
    m_peakTorque = 0.0;
    m_peakHorsepowerRpm = 0.0;
    m_peakTorqueRpm = 0.0;
    memset(m_systemStatusLights, 0, sizeof(m_systemStatusLights));
    m_checkMouse = true;
}

LoadSimulationCluster::~LoadSimulationCluster() {
    /* void */
}

void LoadSimulationCluster::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);

    constexpr float shortenAngle = (float)units::angle(1.0, units::deg);

    m_dynoSpeedGauge = addElement<LabeledGauge>();
    m_dynoSpeedGauge->m_title = "DYNO. SPEED";
    m_dynoSpeedGauge->m_unit = "RPM";
    m_dynoSpeedGauge->m_precision = 0;
    m_dynoSpeedGauge->setLocalPosition({ 0, 0 });
    m_dynoSpeedGauge->m_gauge->m_min = 0;
    m_dynoSpeedGauge->m_gauge->m_max = 100;
    m_dynoSpeedGauge->m_gauge->m_minorStep = 500;
    m_dynoSpeedGauge->m_gauge->m_majorStep = 1000;
    m_dynoSpeedGauge->m_gauge->m_maxMinorTick = INT_MAX;
    m_dynoSpeedGauge->m_gauge->m_thetaMin = (float)constants::pi * 0.8f;
    m_dynoSpeedGauge->m_gauge->m_thetaMax = (float)constants::pi * 0.2f;
    m_dynoSpeedGauge->m_gauge->m_needleWidth = 4.0f;
    m_dynoSpeedGauge->m_gauge->m_gamma = 1.0f;
    m_dynoSpeedGauge->m_gauge->m_needleKs = 1000.0f;
    m_dynoSpeedGauge->m_gauge->m_needleKd = 5.0f;
    m_dynoSpeedGauge->m_gauge->setBandCount(0);

    m_torqueGauge = addElement<LabeledGauge>();
    m_torqueGauge->m_title = "TORQUE";
    m_torqueGauge->m_unit = "LB-FT";
    m_torqueGauge->m_precision = 0;
    m_torqueGauge->setLocalPosition({ 0, 0 });
    m_torqueGauge->m_gauge->m_min = 0;
    m_torqueGauge->m_gauge->m_max = 1000;
    m_torqueGauge->m_gauge->m_minorStep = 50;
    m_torqueGauge->m_gauge->m_majorStep = 100;
    m_torqueGauge->m_gauge->m_maxMinorTick = INT_MAX;
    m_torqueGauge->m_gauge->m_thetaMin = (float)constants::pi * 0.8f;
    m_torqueGauge->m_gauge->m_thetaMax = (float)constants::pi * 0.2f;
    m_torqueGauge->m_gauge->m_needleWidth = 4.0f;
    m_torqueGauge->m_gauge->m_gamma = 1.0f;
    m_torqueGauge->m_gauge->m_needleKs = 1000.0f;
    m_torqueGauge->m_gauge->m_needleKd = 5.0f;
    m_torqueGauge->m_gauge->setBandCount(0);

    m_hpGauge = addElement<LabeledGauge>();
    m_hpGauge->m_title = "POWER";
    m_hpGauge->m_unit = "HP";
    m_hpGauge->m_precision = 0;
    m_hpGauge->setLocalPosition({ 0, 0 });
    m_hpGauge->m_gauge->m_min = 0;
    m_hpGauge->m_gauge->m_max = 1000;
    m_hpGauge->m_gauge->m_minorStep = 50;
    m_hpGauge->m_gauge->m_majorStep = 100;
    m_hpGauge->m_gauge->m_maxMinorTick = INT_MAX;
    m_hpGauge->m_gauge->m_thetaMin = (float)constants::pi * 0.8f;
    m_hpGauge->m_gauge->m_thetaMax = (float)constants::pi * 0.2f;
    m_hpGauge->m_gauge->m_needleWidth = 4.0f;
    m_hpGauge->m_gauge->m_gamma = 1.0f;
    m_hpGauge->m_gauge->m_needleKs = 1000.0f;
    m_hpGauge->m_gauge->m_needleKd = 5.0f;
    m_hpGauge->m_gauge->setBandCount(0);

    m_clutchPressureGauge = addElement<LabeledGauge>();
    m_clutchPressureGauge->m_title = "CLUTCH";
    m_clutchPressureGauge->m_unit = "";
    m_clutchPressureGauge->m_spaceBeforeUnit = false;
    m_clutchPressureGauge->m_precision = 0;
    m_clutchPressureGauge->setLocalPosition({ 0, 0 });
    m_clutchPressureGauge->m_gauge->m_min = 0;
    m_clutchPressureGauge->m_gauge->m_max = 100;
    m_clutchPressureGauge->m_gauge->m_minorStep = 10;
    m_clutchPressureGauge->m_gauge->m_majorStep = 50;
    m_clutchPressureGauge->m_gauge->m_maxMinorTick = 200;
    m_clutchPressureGauge->m_gauge->m_thetaMin = (float)constants::pi * 0.8f;
    m_clutchPressureGauge->m_gauge->m_thetaMax = (float)constants::pi * 0.2f;
    m_clutchPressureGauge->m_gauge->m_needleWidth = 4.0f;
    m_clutchPressureGauge->m_gauge->m_gamma = 1.0f;
    m_clutchPressureGauge->m_gauge->m_needleKs = 1000.0f;
    m_clutchPressureGauge->m_gauge->m_needleKd = 5.0f;
    m_clutchPressureGauge->m_gauge->m_needleMaxVelocity = 10.0f;
    m_clutchPressureGauge->m_gauge->setBandCount(0);

    m_torqueUnits = app->getAppSettings()->torqueUnits;
    m_powerUnits = app->getAppSettings()->powerUnits;
    setUnits();
}

void LoadSimulationCluster::destroy() {
    /* void */
}

void LoadSimulationCluster::update(float dt) {
    m_mouseBounds = m_bounds;
    UiElement::update(dt);

    const float systemStatuses[] = {
        isIgnitionOn() ? 1.0f : 0.01f,
        m_simulator->m_starterMotor.m_enabled ? 1.0f : 0.01f,
        m_simulator->m_dyno.m_enabled ? 1.0f : 0.01f,
        m_simulator->m_dyno.m_hold ? (m_simulator->m_dyno.m_enabled ? 1.0f : 0.25f) : 0.01f
    };

    constexpr float RC = 0.08f;
    const float alpha = dt / (dt + RC);

    for (int i = 0; i < 4; ++i) {
        const float next = systemStatuses[i];
        m_systemStatusLights[i] = (1 - alpha) * m_systemStatusLights[i] + alpha * next;
    }

    if (m_app->getPlatform()->wasKeyPressed(DesktopKey::I)) {
        std::stringstream ss;
        ss << std::setprecision(0) << std::fixed;
        if (m_powerUnits == "hp") {
            ss << m_peakHorsepower << "hp @ " << m_peakHorsepowerRpm << "rpm";
        }
        else {
            ss << m_peakHorsepower << "kW @ " << m_peakHorsepowerRpm << "rpm";
        }

        ss << " | ";

        if (m_torqueUnits == "lb-ft") {
            ss << m_peakTorque << "lb-ft @ " << m_peakTorqueRpm << "rpm";
        }
        else {
            ss << m_peakTorque << "Nm @ " << m_peakTorqueRpm << "rpm";
        }
        m_app->getInfoCluster()->setLogMessage(ss.str());
    }

    updateHpAndTorque(dt);
}

void LoadSimulationCluster::onMouseDown(const Point &mouseLocal) {
    UiElement::onMouseDown(mouseLocal);
    if (statusRowAt(mouseLocal) == 1) {
        m_starterHeld = true;
        m_app->setTouchStarterHeld(true);
    }
}

void LoadSimulationCluster::onMouseUp(const Point &mouseLocal) {
    UiElement::onMouseUp(mouseLocal);
    if (m_starterHeld) {
        m_starterHeld = false;
        m_app->setTouchStarterHeld(false);
    }
}

void LoadSimulationCluster::onMouseClick(const Point &mouseLocal) {
    const int statusRow = statusRowAt(mouseLocal);
    if (statusRow == 0) m_app->toggleIgnition();
    else if (statusRow == 2) m_app->toggleDynamometer();
    else if (statusRow == 3) m_app->toggleDynamometerHold();

    const Bounds gear = gearBounds().inset(10.0f);
    if (!gear.overlaps(mouseLocal)) return;
    const Bounds up = gear.verticalSplit(0.62f, 0.86f);
    const Bounds down = gear.verticalSplit(0.0f, 0.22f);
    if (up.overlaps(mouseLocal)) m_app->changeGear(1);
    else if (down.overlaps(mouseLocal)) m_app->changeGear(-1);
}

void LoadSimulationCluster::render() {
    Grid grid;
    grid.h_cells = 3;
    grid.v_cells = 2;

    drawCurrentGear(gearBounds());

    const Bounds clutchBounds = grid.get(m_bounds, 1, 0);
    drawClutchPressureGauge(clutchBounds);

    const Bounds systemStatusBounds = grid.get(m_bounds, 0, 0);
    drawSystemStatus(systemStatusBounds);

    const Bounds dynoSpeedBounds = grid.get(m_bounds, 0, 1);
    m_dynoSpeedGauge->m_gauge->m_value = 
       (float)units::toRpm(std::abs(m_simulator->m_dyno.m_rotationSpeed));
    m_dynoSpeedGauge->m_bounds = dynoSpeedBounds;

    Engine *engine = m_simulator->getEngine();

    constexpr float shortenAngle = (float)units::angle(1.0, units::deg);
    const double redline = units::toRpm((engine != nullptr) ? engine->getRedline() : 0);
    const double maxRpm = std::floor(redline / 500.0) * 500.0;
    m_dynoSpeedGauge->m_gauge->m_max = (int)(maxRpm);
    m_dynoSpeedGauge->m_gauge->setBandCount(1);
    m_dynoSpeedGauge->m_gauge->setBand(
        { m_app->getRed(), (float)redline, (float)maxRpm, 3.0f, 6.0f, shortenAngle, -shortenAngle }, 0);

    const Bounds torqueBounds = grid.get(m_bounds, 1, 1);
    m_torqueGauge->m_gauge->m_value = m_simulator->m_dyno.m_enabled
        ? (float)m_filteredTorque
        : (float)m_peakTorque;
    m_torqueGauge->m_bounds = torqueBounds;

    const Bounds horsepowerBounds = grid.get(m_bounds, 2, 1);
    m_hpGauge->m_gauge->m_value = m_simulator->m_dyno.m_enabled
        ? (float)m_filteredHorsepower
        : (float)m_peakHorsepower;
    m_hpGauge->m_bounds = horsepowerBounds;

    UiElement::render();
}

void LoadSimulationCluster::drawCurrentGear(const Bounds &bounds) {
    const Bounds insetBounds = bounds.inset(10.0f);
    const Bounds title = insetBounds.verticalSplit(0.86f, 1.0f);
    const Bounds up = insetBounds.verticalSplit(0.62f, 0.86f);
    const Bounds body = insetBounds.verticalSplit(0.22f, 0.62f);
    const Bounds down = insetBounds.verticalSplit(0.0f, 0.22f);

    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    drawCenteredText("Gear", title.inset(10.0f), 24.0f);
    drawGearChevron(up, true);
    drawGearChevron(down, false);

    const int gear = (getTransmission() != nullptr)
        ? getTransmission()->getGear()
        : -1;
    std::stringstream ss;
    
    if (gear != -1) ss << (gear + 1);
    else ss << "N";

    drawCenteredText(ss.str(), body, 64.0f, Bounds::center);
}

void LoadSimulationCluster::drawGearChevron(const Bounds &bounds, bool pointsUp) {
    const Point center = getRenderPoint(bounds.getPosition(Bounds::center));
    const float size = pixelsToUnits(std::fmin(bounds.width(), bounds.height()) * 0.28f);
    const float halfWidth = size;
    const float halfHeight = size * 0.65f;
    const float baseY = center.y + (pointsUp ? -halfHeight : halfHeight);
    const float pointY = center.y + (pointsUp ? halfHeight : -halfHeight);

    GeometryGenerator::Line2dParameters line;
    line.lineWidth = pixelsToUnits(3.0f);

    GeometryGenerator *generator = m_app->getGeometryGenerator();
    GeometryGenerator::GeometryIndices chevron;
    generator->startShape();
    line.x0 = center.x - halfWidth;
    line.y0 = baseY;
    line.x1 = center.x;
    line.y1 = pointY;
    generator->generateLine2d(line);
    line.x0 = center.x;
    line.y0 = pointY;
    line.x1 = center.x + halfWidth;
    line.y1 = baseY;
    generator->generateLine2d(line);
    generator->endShape(&chevron);

    resetShader();
    m_app->getShaders()->SetBaseColor(m_app->getForegroundColor());
    m_app->drawGenerated(chevron, 0x11, m_app->getShaders()->GetUiFlags());
}

Bounds LoadSimulationCluster::gearBounds() const {
    Grid grid = { 3, 2 };
    return grid.get(m_bounds, 2, 0);
}

Bounds LoadSimulationCluster::systemStatusBounds() const {
    Grid grid = { 3, 2 };
    return grid.get(m_bounds, 0, 0);
}

int LoadSimulationCluster::statusRowAt(const Point &mouseLocal) const {
    const Bounds status = systemStatusBounds();
    if (!status.overlaps(mouseLocal)) return -1;
    Grid grid = { 1, 4 };
    for (int row = 0; row < 4; ++row) {
        if (grid.get(status, 0, row).overlaps(mouseLocal)) return row;
    }
    return -1;
}

void LoadSimulationCluster::drawClutchPressureGauge(const Bounds &bounds) {
    m_clutchPressureGauge->m_bounds = bounds;
    m_clutchPressureGauge->m_gauge->m_value = (getTransmission() != nullptr)
        ? (float)getTransmission()->getClutchPressure() * 100.0f
        : 0.0f;
}

void LoadSimulationCluster::drawSystemStatus(const Bounds &bounds) {
    const Bounds left = bounds.horizontalSplit(0.0f, 0.6f);
    const Bounds right = bounds.horizontalSplit(0.6f, 1.0f);

    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    Grid grid;
    grid.v_cells = 4;
    grid.h_cells = 1;

    drawText(
        "Ignition",
        grid.get(left, 0, 0).inset(10.0f),
        20.0,
        Bounds::lm);
    drawText(
        "Starter",
        grid.get(left, 0, 1).inset(10.0f),
        20.0,
        Bounds::lm);
    drawText(
        "Dyno.",
        grid.get(left, 0, 2).inset(10.0f),
        20.0,
        Bounds::lm);
    drawText(
        "Hold",
        grid.get(left, 0, 3).inset(10.0f),
        20.0,
        Bounds::lm);

    for (int i = 0; i < 4; ++i) {
        const Bounds rawBounds = grid.get(right, 0, i);
        const float width = std::fmax(rawBounds.width(), rawBounds.height());
        const Bounds squareBounds(width - 20.0f, 5.0f, rawBounds.getPosition(Bounds::center), Bounds::center);

        drawFrame(
            squareBounds,
            1.0f,
            mix(m_app->getBackgroundColor(), m_app->getForegroundColor(), 0.001f),
            mix(m_app->getBackgroundColor(), m_app->getRed(), m_systemStatusLights[i])
        );
    }
}

void LoadSimulationCluster::updateHpAndTorque(float dt) {
    constexpr double RC = 0.1;
    const double alpha = dt / (dt + RC);

    const double torque = m_simulator->getFilteredDynoTorque();
    const double power = m_simulator->getDynoPower();
    const double torqueWithUnits = (m_torqueUnits == "Nm")
        ? (units::convert(torque, units::Nm))
        : (units::convert(torque, units::ft_lb));
    const double powerWithUnits = (m_powerUnits == "kW")
        ? (units::convert(power, units::kW))
        : (units::convert(power, units::hp));

    m_filteredTorque = (1 - alpha) * m_filteredTorque + alpha * torqueWithUnits;
    m_filteredHorsepower = (1 - alpha) * m_filteredHorsepower + alpha * powerWithUnits;

    if (m_simulator->getEngine() != nullptr) {
        if (m_filteredTorque > m_peakTorque) {
            m_peakTorque = m_filteredTorque;
            m_peakTorqueRpm = m_simulator->getEngine()->getRpm();
        }

        if (m_filteredHorsepower > m_peakHorsepower) {
            m_peakHorsepower = std::fmax(m_peakHorsepower, m_filteredHorsepower);
            m_peakHorsepowerRpm = m_simulator->getEngine()->getRpm();
        }
    }
}

bool LoadSimulationCluster::isIgnitionOn() const {
    Engine *engine = m_simulator->getEngine();
    return (engine != nullptr)
        ? engine->getIgnitionModule()->m_enabled
        : false;
}

void LoadSimulationCluster::setUnits(){
    if (m_torqueUnits == "lb-ft") {
        m_torqueGauge->m_unit = "lb-ft";
        m_torqueGauge->m_precision = 0;
        m_torqueGauge->m_gauge->m_min = 0;
        m_torqueGauge->m_gauge->m_max = 1000;
        m_torqueGauge->m_gauge->m_minorStep = 50;
        m_torqueGauge->m_gauge->m_majorStep = 100;
    }
    else if (m_torqueUnits == "Nm") {
        m_torqueGauge->m_unit = "Nm";
        m_torqueGauge->m_precision = 1;
        m_torqueGauge->m_gauge->m_min = 0;
        m_torqueGauge->m_gauge->m_max = 1000;
        m_torqueGauge->m_gauge->m_minorStep = 50;
        m_torqueGauge->m_gauge->m_majorStep = 100;
    }

    if (m_powerUnits == "hp") {
        m_hpGauge->m_unit = "hp";
        m_hpGauge->m_precision = 0;

        m_hpGauge->m_gauge->m_min = 0;
        m_hpGauge->m_gauge->m_max = 1000;
        m_hpGauge->m_gauge->m_minorStep = 50;
        m_hpGauge->m_gauge->m_majorStep = 100;
    }
    else if (m_powerUnits == "kW") {
        m_hpGauge->m_unit = "kW";
        m_hpGauge->m_precision = 1;
        m_hpGauge->m_gauge->m_min = 0;
        m_hpGauge->m_gauge->m_max = 1000;
        m_hpGauge->m_gauge->m_minorStep = 50;
        m_hpGauge->m_gauge->m_majorStep = 100;
    }
}
