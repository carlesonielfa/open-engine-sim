include(cmake/EngineSimSDL.cmake)
engine_sim_require_sdl3()

add_executable(engine-sim-desktop
    src/desktop_main.cpp
    src/desktop_platform_sdl.cpp
    src/sdl_audio_util.cpp
    src/sdl_audio_output.cpp
    src/sdl_gpu_renderer.cpp)
target_link_libraries(engine-sim-desktop PRIVATE SDL3::SDL3 engine-sim-visualization)
target_compile_features(engine-sim-desktop PRIVATE cxx_std_17)
target_compile_definitions(engine-sim-desktop PRIVATE
    ENGINE_SIM_SHADER_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}/shaders"
    ENGINE_SIM_SOURCE_ASSET_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/assets")
if(ENGINE_SIM_BUILD_SCRIPTING)
    target_link_libraries(engine-sim-desktop PRIVATE engine-sim-scripting)
    target_compile_definitions(engine-sim-desktop PRIVATE ATG_ENGINE_SIM_PIRANHA_ENABLED)
endif()

engine_sim_add_shader_artifacts(engine-sim-shaders
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/engine_sim.hlsl"
    "${CMAKE_CURRENT_BINARY_DIR}/shaders")
if(TARGET engine-sim-shaders)
    add_dependencies(engine-sim-desktop engine-sim-shaders)
endif()

if(APPLE)
    set_target_properties(engine-sim-desktop PROPERTIES INSTALL_RPATH "@executable_path")
elseif(UNIX)
    set_target_properties(engine-sim-desktop PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

if(APPLE AND ENGINE_SIM_MACOS_APP_BUNDLE)
    set(ENGINE_SIM_MACOS_BUNDLE_IDENTIFIER "org.openenginesim.desktop")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        @ONLY)
    set_target_properties(engine-sim-desktop PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        OUTPUT_NAME "engine-sim")
    set(ENGINE_SIM_INSTALL_ASSET_DIRECTORY "engine-sim.app/Contents/Resources/assets")
    set(ENGINE_SIM_INSTALL_RUNTIME_DIRECTORY "engine-sim.app/Contents/MacOS")
    install(TARGETS engine-sim-desktop BUNDLE DESTINATION .)
else()
    set(ENGINE_SIM_INSTALL_ASSET_DIRECTORY "assets")
    set(ENGINE_SIM_INSTALL_RUNTIME_DIRECTORY "bin")
    install(TARGETS engine-sim-desktop RUNTIME DESTINATION "${ENGINE_SIM_INSTALL_RUNTIME_DIRECTORY}")
endif()

install(FILES "$<TARGET_FILE:SDL3::SDL3>" DESTINATION "${ENGINE_SIM_INSTALL_RUNTIME_DIRECTORY}")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION "${ENGINE_SIM_INSTALL_ASSET_DIRECTORY}"
    PATTERN ".DS_Store" EXCLUDE)
if(TARGET engine-sim-shaders)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.fragment.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.vertex.dxil"
        "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.fragment.dxil"
        "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.vertex.msl"
        "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.fragment.msl"
        DESTINATION "${ENGINE_SIM_INSTALL_ASSET_DIRECTORY}/shaders")
endif()

# Sign only after every bundle component has been installed. This is an ad-hoc
# signature: it makes the archive internally consistent, but Developer ID
# signing and notarization are still required for seamless Gatekeeper launches.
if(APPLE AND ENGINE_SIM_MACOS_APP_BUNDLE)
    install(CODE [[
        execute_process(
            COMMAND codesign --force --deep --sign - "${CMAKE_INSTALL_PREFIX}/engine-sim.app"
            COMMAND_ERROR_IS_FATAL ANY)
    ]])
endif()
