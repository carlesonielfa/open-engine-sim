cmake_minimum_required(VERSION 3.24)

project(engine-sim VERSION 0.2.2 LANGUAGES CXX)

include(CTest)
include(FetchContent)
include(cmake/EngineSimShaders.cmake)
include(cmake/EngineSimEngineCatalog.cmake)

option(ENGINE_SIM_BUILD_DESKTOP "Build the SDL3 desktop host" OFF)
option(ENGINE_SIM_BUILD_WEB "Build the Emscripten/WebGL 2 browser host" OFF)
option(ENGINE_SIM_BUILD_SCRIPTING "Build the Piranha script interpreter" OFF)
option(ENGINE_SIM_FETCH_SDL3 "Fetch pinned SDL3 when it is not installed" ON)
option(ENGINE_SIM_MACOS_APP_BUNDLE "Build the desktop host as a macOS .app bundle" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

set(ENGINE_SIM_SUBMODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/submodules")
foreach(submodule IN ITEMS simple-2d-constraint-solver csv-io piranha)
    if(NOT EXISTS "${ENGINE_SIM_SUBMODULE_DIR}/${submodule}/CMakeLists.txt")
        message(FATAL_ERROR
            "Missing ${submodule} submodule. Run: git submodule update --init --recursive")
    endif()
endforeach()

# Core simulation has no desktop, graphics, or audio-device dependency.
file(GLOB ENGINE_SIM_SCS_SOURCES CONFIGURE_DEPENDS
    "${ENGINE_SIM_SUBMODULE_DIR}/simple-2d-constraint-solver/src/*.cpp")
add_library(simple-2d-constraint-solver STATIC ${ENGINE_SIM_SCS_SOURCES})
target_include_directories(simple-2d-constraint-solver
    PUBLIC "${ENGINE_SIM_SUBMODULE_DIR}/simple-2d-constraint-solver/include")
set_property(TARGET simple-2d-constraint-solver PROPERTY FOLDER "third_party/scs")
if(NOT MSVC)
    target_compile_definitions(simple-2d-constraint-solver PUBLIC __forceinline=inline)
    target_compile_options(simple-2d-constraint-solver PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/cmake/EngineSimLegacySolverCompatibility.h")
endif()

set(ENGINE_SIM_CORE_SOURCES
    src/camshaft.cpp
    src/combustion_chamber.cpp
    src/connecting_rod.cpp
    src/convolution_filter.cpp
    src/crankshaft.cpp
    src/cylinder_bank.cpp
    src/cylinder_head.cpp
    src/delay_filter.cpp
    src/derivative_filter.cpp
    src/direct_throttle_linkage.cpp
    src/dynamometer.cpp
    src/engine.cpp
    src/exhaust_system.cpp
    src/feedback_comb_filter.cpp
    src/filter.cpp
    src/fuel.cpp
    src/function.cpp
    src/gas_system.cpp
    src/gaussian_filter.cpp
    src/governor.cpp
    src/ignition_module.cpp
    src/impulse_response.cpp
    src/intake.cpp
    src/jitter_filter.cpp
    src/leveling_filter.cpp
    src/low_pass_filter.cpp
    src/part.cpp
    src/piston.cpp
    src/piston_engine_simulator.cpp
    src/simulator.cpp
    src/standard_valvetrain.cpp
    src/starter_motor.cpp
    src/synthesizer.cpp
    src/throttle.cpp
    src/transmission.cpp
    src/utilities.cpp
    src/valvetrain.cpp
    src/vehicle.cpp
    src/vehicle_drag_constraint.cpp
    src/vtec_valvetrain.cpp)

add_library(engine-sim-core STATIC ${ENGINE_SIM_CORE_SOURCES})
add_library(engine-sim::core ALIAS engine-sim-core)
target_include_directories(engine-sim-core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
target_link_libraries(engine-sim-core PUBLIC simple-2d-constraint-solver)
target_compile_features(engine-sim-core PUBLIC cxx_std_17)

add_library(engine-sim-render-support STATIC
    src/geometry_generator.cpp
    src/runtime_paths.cpp
    src/shaders.cpp
    src/ui_math.cpp)
target_include_directories(engine-sim-render-support
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(engine-sim-render-support PUBLIC engine-sim::core)
target_compile_features(engine-sim-render-support PUBLIC cxx_std_17)

# Visualization remains host-neutral; the SDL adapter lives in the desktop
# module so an alternate host can reuse the simulation and drawing layers.
add_library(engine-sim-visualization STATIC
    src/afr_cluster.cpp
    src/combustion_chamber_object.cpp
    src/connecting_rod_object.cpp
    src/crankshaft_object.cpp
    src/cylinder_bank_object.cpp
    src/cylinder_head_object.cpp
    src/cylinder_pressure_gauge.cpp
    src/cylinder_temperature_gauge.cpp
    src/desktop_application.cpp
    src/engine_catalog.cpp
    src/engine_view.cpp
    src/firing_order_display.cpp
    src/fuel_cluster.cpp
    src/gauge.cpp
    src/info_cluster.cpp
    src/labeled_gauge.cpp
    src/load_simulation_cluster.cpp
    src/authored_mesh_library.cpp
    src/mixer_cluster.cpp
    src/oscilloscope.cpp
    src/oscilloscope_cluster.cpp
    src/performance_cluster.cpp
    src/piston_object.cpp
    src/right_gauge_cluster.cpp
    src/simulation_object.cpp
    src/throttle_display.cpp
    src/text_renderer.cpp
    src/ui_button.cpp
    src/ui_element.cpp
    src/ui_manager.cpp
    src/ui_utilities.cpp)
target_include_directories(engine-sim-visualization
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
        "${ENGINE_SIM_GENERATED_INCLUDE_DIRECTORY}")
target_link_libraries(engine-sim-visualization PUBLIC engine-sim-render-support)
target_compile_features(engine-sim-visualization PUBLIC cxx_std_17)

if(ENGINE_SIM_BUILD_SCRIPTING)
    include(cmake/EngineSimScripting.cmake)
endif()

if(ENGINE_SIM_BUILD_DESKTOP)
    include(cmake/EngineSimDesktop.cmake)
    include(cmake/EngineSimPackaging.cmake)
endif()

if(ENGINE_SIM_BUILD_WEB)
    include(cmake/EngineSimWeb.cmake)
endif()

if(BUILD_TESTING)
    include(cmake/EngineSimTests.cmake)
endif()
