#ifndef ATG_ENGINE_SIM_RIGHT_GAUGE_CLUSTER_H
#define ATG_ENGINE_SIM_RIGHT_GAUGE_CLUSTER_H

#include "ui_element.h"

#include "engine.h"
#include "simulator.h"
#include "gauge.h"
#include "firing_order_display.h"
#include "labeled_gauge.h"
#include "throttle_display.h"
#include "afr_cluster.h"
#include "fuel_cluster.h"

#include <string>

class RightGaugeCluster : public UiElement {
    public:
        RightGaugeCluster();
        virtual ~RightGaugeCluster();

        virtual void initialize(EngineSimApplication *app);
        virtual void destroy();

        virtual void update(float dt);
        virtual void render();
        virtual void onMouseDown(const Point &mouseLocal);
        virtual void onMouseUp(const Point &mouseLocal);
        virtual void onDrag(const Point &p0, const Point &mouse0, const Point &mouse);

        void setEngine(Engine *engine);
        void setUnits();
        double getManifoldPressureWithUnits(double ambientPressure);

        Simulator *m_simulator;

    private:
        double getRpm() const;
        double getRedline() const;
        double getSpeed() const;
        double getManifoldPressure() const;

    protected:
        Engine *m_engine;

        void renderTachSpeedCluster(const Bounds &bounds);
        void renderFuelAirCluster(const Bounds &bounds);
        Bounds throttleControlBounds() const;
        void setTouchThrottleFromPoint(const Point &mouseLocal);
        void renderTouchThrottleControl();

        LabeledGauge *m_tachometer;
        LabeledGauge *m_speedometer;
        LabeledGauge *m_manifoldVacuumGauge;
        LabeledGauge *m_intakeCfmGauge;
        LabeledGauge *m_volumetricEffGauge;
        FuelCluster *m_fuelCluster;
        ThrottleDisplay *m_throttleDisplay;
        AfrCluster *m_afrCluster;
        FiringOrderDisplay *m_combusionChamberStatus;
        std::string m_speedUnits;
        std::string m_pressureUnits;
        bool m_isAbsolute;
        bool m_throttleHeld = false;
};

#endif /* ATG_ENGINE_SIM_GAUGE_CLUSTER_H */
