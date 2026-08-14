add_library(csv-io STATIC
    "${ENGINE_SIM_SUBMODULE_DIR}/csv-io/src/information.cpp"
    "${ENGINE_SIM_SUBMODULE_DIR}/csv-io/src/csv_data.cpp")
target_include_directories(csv-io
    PUBLIC "${ENGINE_SIM_SUBMODULE_DIR}/csv-io/include")
set_property(TARGET csv-io PROPERTY FOLDER "third_party/csv")

find_package(GTest CONFIG QUIET)
if(NOT TARGET GTest::gtest_main)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG 58d77fa8070e8cec2dc1ed015d66b454c8d78850
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(engine-sim-core-tests
    test/function_test.cpp
    test/gas_system_tests.cpp
    test/geometry_generator_test.cpp
    test/runtime_paths_test.cpp
    test/shaders_test.cpp
    test/synthesizer_tests.cpp
    test/text_renderer_test.cpp
    test/authored_mesh_library_test.cpp
    test/engine_catalog_test.cpp
    src/text_renderer.cpp
    src/authored_mesh_library.cpp
    src/engine_catalog.cpp)
if(ENGINE_SIM_BUILD_SCRIPTING)
    target_sources(engine-sim-core-tests PRIVATE test/script_compile_test.cpp)
    target_link_libraries(engine-sim-core-tests PRIVATE engine-sim-scripting)
    target_compile_definitions(engine-sim-core-tests PRIVATE
        ENGINE_SIM_TEST_ASSET_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ATG_ENGINE_SIM_PIRANHA_ENABLED)
endif()
target_link_libraries(engine-sim-core-tests
    PRIVATE engine-sim::core engine-sim-render-support csv-io gtest_main)
target_include_directories(engine-sim-core-tests
    PRIVATE
        "${ENGINE_SIM_SUBMODULE_DIR}/csv-io/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
        "${ENGINE_SIM_GENERATED_INCLUDE_DIRECTORY}")
target_compile_definitions(engine-sim-core-tests PRIVATE
    ENGINE_SIM_TEST_SOURCE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}")
include(GoogleTest)
gtest_discover_tests(engine-sim-core-tests)

if(ENGINE_SIM_BUILD_DESKTOP)
    # Runs against SDL's dummy device, so it needs no physical audio endpoint.
    add_executable(engine-sim-desktop-audio-tests
        test/sdl_audio_output_test.cpp
        src/sdl_audio_util.cpp
        src/sdl_audio_output.cpp)
    target_link_libraries(engine-sim-desktop-audio-tests
        PRIVATE SDL3::SDL3 engine-sim::core gtest_main)
    target_compile_definitions(engine-sim-desktop-audio-tests PRIVATE
        ENGINE_SIM_TEST_ASSET_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    if(WIN32)
        add_custom_command(TARGET engine-sim-desktop-audio-tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3>"
                "$<TARGET_FILE_DIR:engine-sim-desktop-audio-tests>")
    endif()
    gtest_discover_tests(engine-sim-desktop-audio-tests)
endif()
