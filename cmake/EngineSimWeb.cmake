if(NOT EMSCRIPTEN)
    message(FATAL_ERROR
        "The web target requires Emscripten. Source emsdk_env.sh, then configure with: emcmake cmake --preset web")
endif()

include(cmake/EngineSimSDL.cmake)
engine_sim_require_sdl3()

add_executable(engine-sim-web
    src/web_main.cpp
    src/desktop_platform_sdl.cpp
    src/sdl_audio_util.cpp
    src/web_audio_output.cpp
    src/web_gl_renderer.cpp)
target_link_libraries(engine-sim-web PRIVATE SDL3::SDL3 engine-sim-visualization)
target_compile_features(engine-sim-web PRIVATE cxx_std_17)
if(ENGINE_SIM_BUILD_SCRIPTING)
    target_link_libraries(engine-sim-web PRIVATE engine-sim-scripting)
    target_compile_definitions(engine-sim-web PRIVATE ATG_ENGINE_SIM_PIRANHA_ENABLED)
endif()

set_target_properties(engine-sim-web PROPERTIES
    OUTPUT_NAME "index"
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/web")
target_link_options(engine-sim-web PRIVATE
    "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
    "SHELL:--exclude-file ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders"
    "SHELL:-sAUDIO_WORKLET"
    "SHELL:-sWASM_WORKERS"
    "SHELL:-pthread"
    "SHELL:-sINITIAL_MEMORY=134217728"
    "SHELL:-sMAX_WEBGL_VERSION=2"
    "SHELL:-sMIN_WEBGL_VERSION=2"
    "SHELL:-sFULL_ES3=1"
    "SHELL:-sEXPORTED_FUNCTIONS=['_main','_engine_sim_enable_audio','_engine_sim_web_audio_state','_engine_sim_web_audio_context_state','_engine_sim_web_audio_sample_rate','_engine_sim_web_audio_has_worklet_thread','_engine_sim_web_audio_has_node','_engine_sim_web_audio_callback_count','_engine_sim_web_audio_peak_milli']"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall']"
    "SHELL:-sINVOKE_RUN=0"
    "SHELL:-sNO_EXIT_RUNTIME=1")
add_custom_command(TARGET engine-sim-web POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/web/_headers"
        "$<TARGET_FILE_DIR:engine-sim-web>/_headers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/web/manifest.webmanifest"
        "$<TARGET_FILE_DIR:engine-sim-web>/manifest.webmanifest")
