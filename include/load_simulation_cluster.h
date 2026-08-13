#ifndef ATG_ENGINE_SIM_LOAD_SIMULATION_CLUSTER_H
#define ATG_ENGINE_SIM_LOAD_SIMULATION_CLUSTER_H

#include "ui_element.h"

#include "simulator.h"
#include "labeled_gauge.h"

#include <string>

class LoadSimulationCluster : public UiElement {
    public:
        LoadSimulationCluster();
        virtual ~LoadSimulationCluster();

        virtual void initialize(EngineSimApplication *app);
        virtual void destroy();

        virtual void update(float dt);
        virtual void render();
        virtual void onMouseDown(const Point &mouseLocal);
        virtual void onMouseUp(const Point &mouseLocal);
        virtual void onMouseClick(const Point &mouseLocal);
        void setUnits();

        void setSimulator(Simulator *simulator) { m_simulator = simulator; }

    private:
        Transmission *getTransmission() const { return m_simulator->getTransmission(); }

    protected:
        void drawCurrentGear(const Bounds &bounds);
        void drawGearChevron(const Bounds &bounds, bool pointsUp);
        void drawClutchPressureGauge(const Bounds &bounds);
        void drawSystemStatus(const Bounds &bounds);
        void updateHpAndTorque(float dt);
        bool isIgnitionOn() const;
        Bounds gearBounds() const;
        Bounds systemStatusBounds() const;
        int statusRowAt(const Point &mouseLocal) const;

        float m_systemStatusLights[4];
        LabeledGauge *m_dynoSpeedGauge;
        LabeledGauge *m_torqueGauge;
        LabeledGauge *m_hpGauge;
        LabeledGauge *m_clutchPressureGauge;

        double m_filteredHorsepower;
        double m_filteredTorque;

        double m_peakHorsepowerRpm;
        double m_peakHorsepower;
        double m_peakTorqueRpm;
        double m_peakTorque;
        
        std::string m_powerUnits;
        std::string m_torqueUnits;

        Simulator *m_simulator;
        bool m_starterHeld = false;
};

#endif /* ATG_ENGINE_SIM_LOAD_SIMULATION_CLUSTER_H */
