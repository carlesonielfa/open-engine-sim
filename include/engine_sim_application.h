#ifndef ATG_ENGINE_SIM_ENGINE_SIM_APPLICATION_H
#define ATG_ENGINE_SIM_ENGINE_SIM_APPLICATION_H

#include "geometry_generator.h"
#include "simulator.h"
#include "engine.h"
#include "simulation_object.h"
#include "ui_manager.h"
#include "dynamometer.h"
#include "oscilloscope.h"
#include "audio_output.h"
#include "convolution_filter.h"
#include "shaders.h"
#include "engine_view.h"
#include "right_gauge_cluster.h"
#include "cylinder_temperature_gauge.h"
#include "synthesizer.h"
#include "oscilloscope_cluster.h"
#include "performance_cluster.h"
#include "load_simulation_cluster.h"
#include "mixer_cluster.h"
#include "info_cluster.h"
#include "application_settings.h"
#include "desktop_platform.h"

#include <string>
#include "runtime_paths.h"
#include "renderer.h"
#include "text_renderer.h"
#include "authored_mesh_library.h"
#include "transmission.h"

#include <vector>

class EngineSimApplication {
    private:
        static std::string s_buildVersion;

    public:
        EngineSimApplication();
        virtual ~EngineSimApplication();

        static std::string getBuildVersion() { return s_buildVersion; }

        void initialize(
            DesktopPlatform *platform,
            Renderer *renderer,
            AudioOutput *audioOutput,
            const RuntimePaths &runtimePaths);
        void run();
        // Runs one platform frame. Returns false once the host should stop.
        // Browser hosts call this from requestAnimationFrame instead of using
        // the blocking native run() loop.
        bool tick();
        void destroy();

        void loadEngine(Engine *engine, Vehicle *vehicle, Transmission *transmission);
        void drawGenerated(
                const GeometryGenerator::GeometryIndices &indices,
                int layer = 0);
        void drawGeneratedUi(
                const GeometryGenerator::GeometryIndices &indices,
                int layer = 0);
        void drawModel(
                const std::string &modelName,
                Shaders::StageEnableFlags flags,
                int layer = 0);
        void drawGenerated(
                const GeometryGenerator::GeometryIndices &indices,
                int layer,
                Shaders::StageEnableFlags flags);
        void configure(const ApplicationSettings &settings);
        GeometryGenerator *getGeometryGenerator() { return &m_geometryGenerator; }

        Shaders *getShaders() { return &m_shaders; }
        TextRenderer *getTextRenderer() { return &m_textRenderer; }

        void createObjects(Engine *engine);
        void destroyObjects();
        DesktopPlatform *getPlatform() { return m_platform; }

        float pixelsToUnits(float pixels) const;
        float unitsToPixels(float units) const;

        ysVector getBackgroundColor() const { return m_background; }
        ysVector getForegroundColor() const { return m_foreground; }
        ysVector getHightlight1Color() const { return m_highlight1; }
        ysVector getPink() const { return m_pink; }
        ysVector getGreen() const { return m_green; }
        ysVector getYellow() const { return m_yellow; }
        ysVector getRed() const { return m_red; }
        ysVector getOrange() const { return m_orange; }
        ysVector getBlue() const { return m_blue; }

        const SimulationObject::ViewParameters &getViewParameters() const;
        void setViewLayer(int view) { m_viewParameters.Layer0 = view; }

        int getScreenWidth() const { return m_screenWidth; }
        int getScreenHeight() const { return m_screenHeight; }
        float getAverageFramerate() const { return m_averageFramerate; }

        Simulator *getSimulator() { return m_simulator; }
        InfoCluster *getInfoCluster() { return m_infoCluster; }
        ApplicationSettings* getAppSettings() { return &m_applicationSettings; }

        void toggleIgnition();
        void toggleDynamometer();
        void toggleDynamometerHold();
        void toggleFullscreen();
        void changeGear(int direction);
        void setTouchStarterHeld(bool held);
        void setTouchThrottle(double value, bool held);

    protected:
        void loadScript();
        void processEngineInput(float dt);
        void renderScene();

        void refreshUserInterface();

    protected:
        double m_speedSetting = 1.0;
        double m_targetSpeedSetting = 1.0;
        double m_touchThrottle = 0.0;
        bool m_touchThrottleHeld = false;
        bool m_touchStarterHeld = false;

        double m_clutchPressure = 1.0;
        double m_targetClutchPressure = 1.0;
        int m_lastMouseWheel = 0;

    protected:
        virtual void initialize();
        virtual void process(float dt);
        virtual void render();

        float m_displayAngle;
        float m_displayHeight;
        int m_screenWidth;
        int m_screenHeight;
        
        ApplicationSettings m_applicationSettings;
        Shaders m_shaders;

        DesktopPlatform *m_platform;
        Renderer *m_renderer;
        AudioOutput *m_audioOutput;

        std::string m_assetPath;

        GeometryGenerator m_geometryGenerator;
        AuthoredMeshLibrary m_authoredMeshes;
        TextRenderer m_textRenderer;

        std::vector<SimulationObject *> m_objects;
        Engine *m_iceEngine;
        Vehicle *m_vehicle;
        Transmission *m_transmission;
        Simulator *m_simulator;
        double m_dynoSpeed;
        double m_torque;

        UiManager m_uiManager;
        EngineView *m_engineView;
        RightGaugeCluster *m_rightGaugeCluster;
        OscilloscopeCluster *m_oscCluster;
        CylinderTemperatureGauge *m_temperatureGauge;
        PerformanceCluster *m_performanceCluster;
        LoadSimulationCluster *m_loadSimulationCluster;
        MixerCluster *m_mixerCluster;
        InfoCluster *m_infoCluster;
        SimulationObject::ViewParameters m_viewParameters;

        bool m_paused;

    protected:
        ysVector m_background;
        ysVector m_foreground;
        ysVector m_shadow;
        ysVector m_highlight1;
        ysVector m_highlight2;

        ysVector m_pink;
        ysVector m_orange;
        ysVector m_yellow;
        ysVector m_red;
        ysVector m_green;
        ysVector m_blue;

        int m_oscillatorSampleOffset;
        int m_screen;
        float m_averageFramerate = 60.0f;
        std::uint64_t m_lastTick = 0;
        std::uint64_t m_lastRenderTick = 0;

#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
        atg_dtv::Encoder m_encoder;
#endif /* ATG_ENGINE_SIM_VIDEO_CAPTURE */
};

#endif /* ATG_ENGINE_SIM_ENGINE_SIM_APPLICATION_H */
