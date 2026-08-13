# Piranha's upstream project fetches its own test dependencies. Define the
# interpreter here instead so Engine Simulator controls parser generation and
# keeps its builds reproducible. Homebrew deliberately keeps current Flex and
# Bison out of PATH, while macOS ships an obsolete Bison 2.3.
if(CMAKE_HOST_APPLE)
    foreach(prefix IN ITEMS "/opt/homebrew" "/usr/local")
        if(EXISTS "${prefix}/opt/bison/bin/bison")
            set(BISON_EXECUTABLE "${prefix}/opt/bison/bin/bison" CACHE FILEPATH
                "Bison executable" FORCE)
            break()
        endif()
    endforeach()
    foreach(prefix IN ITEMS "/opt/homebrew" "/usr/local")
        if(EXISTS "${prefix}/opt/flex/bin/flex")
            set(FLEX_EXECUTABLE "${prefix}/opt/flex/bin/flex" CACHE FILEPATH
                "Flex executable" FORCE)
            break()
        endif()
    endforeach()
endif()
find_package(FLEX REQUIRED)
find_package(BISON 3.2 REQUIRED)

# Generated scanners must use the FlexLexer.h from the same Flex version.
set(ENGINE_SIM_FLEX_INCLUDE_DIR "${FLEX_INCLUDE_DIRS}")
if(CMAKE_HOST_APPLE)
    foreach(prefix IN ITEMS "/opt/homebrew" "/usr/local")
        if(EXISTS "${prefix}/opt/flex/include/FlexLexer.h")
            set(ENGINE_SIM_FLEX_INCLUDE_DIR "${prefix}/opt/flex/include")
            break()
        endif()
    endforeach()
endif()
if(NOT ENGINE_SIM_FLEX_INCLUDE_DIR
    OR ENGINE_SIM_FLEX_INCLUDE_DIR MATCHES "-NOTFOUND$")
    find_path(ENGINE_SIM_FLEX_INCLUDE_DIR FlexLexer.h
        HINTS
            "/opt/homebrew/opt/flex/include"
            "/usr/local/opt/flex/include"
            "C:/ProgramData/chocolatey/lib/winflexbison3/tools"
        PATHS
            "/usr/include"
            "/usr/local/include"
        NO_CMAKE_FIND_ROOT_PATH)
endif()
if(NOT ENGINE_SIM_FLEX_INCLUDE_DIR
    OR ENGINE_SIM_FLEX_INCLUDE_DIR MATCHES "-NOTFOUND$")
    message(FATAL_ERROR
        "FlexLexer.h was not found. Install the development headers for Flex.")
endif()

set(ENGINE_SIM_PIRANHA_DIR "${ENGINE_SIM_SUBMODULE_DIR}/piranha")
set(ENGINE_SIM_GENERATED_PIRANHA_DIR "${CMAKE_CURRENT_BINARY_DIR}/piranha")
file(MAKE_DIRECTORY "${ENGINE_SIM_GENERATED_PIRANHA_DIR}")
# Flex is a host build tool during cross-compilation. Copy its self-contained
# C++ interface rather than adding the host include directory to target compile
# flags, which could cause the target compiler to select host libc headers.
configure_file(
    "${ENGINE_SIM_FLEX_INCLUDE_DIR}/FlexLexer.h"
    "${ENGINE_SIM_GENERATED_PIRANHA_DIR}/FlexLexer.h"
    COPYONLY)
flex_target(engine_sim_piranha_lexer
    "${ENGINE_SIM_PIRANHA_DIR}/flex-bison/scanner.l"
    "${CMAKE_CURRENT_BINARY_DIR}/piranha/scanner.auto.cpp"
    DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/piranha/scanner.auto.h")
bison_target(engine_sim_piranha_parser
    "${ENGINE_SIM_PIRANHA_DIR}/flex-bison/specification.y"
    "${CMAKE_CURRENT_BINARY_DIR}/piranha/parser.auto.cpp"
    DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/piranha/parser.auto.h")
add_flex_bison_dependency(engine_sim_piranha_lexer engine_sim_piranha_parser)

file(GLOB ENGINE_SIM_PIRANHA_SOURCES CONFIGURE_DEPENDS
    "${ENGINE_SIM_PIRANHA_DIR}/src/*.cpp")
list(REMOVE_ITEM ENGINE_SIM_PIRANHA_SOURCES "${ENGINE_SIM_PIRANHA_DIR}/src/path.cpp")
add_library(piranha STATIC
    ${ENGINE_SIM_PIRANHA_SOURCES}
    src/piranha_path_compat.cpp
    ${BISON_engine_sim_piranha_parser_OUTPUTS}
    ${FLEX_engine_sim_piranha_lexer_OUTPUTS})
target_include_directories(piranha
    PUBLIC
        "${ENGINE_SIM_SUBMODULE_DIR}"
        "${ENGINE_SIM_PIRANHA_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${ENGINE_SIM_GENERATED_PIRANHA_DIR}"
        "${ENGINE_SIM_PIRANHA_DIR}/dependencies/libraries/flex/include")
target_compile_features(piranha PUBLIC cxx_std_17)
if(NOT MSVC)
    target_compile_definitions(piranha PUBLIC __int64=long)
endif()
set_property(TARGET piranha PROPERTY FOLDER "third_party/piranha")

add_library(engine-sim-scripting STATIC
    scripting/src/channel_types.cpp
    scripting/src/compiler.cpp
    scripting/src/engine_context.cpp
    scripting/src/language_rules.cpp)
add_library(engine-sim::scripting ALIAS engine-sim-scripting)
target_include_directories(engine-sim-scripting
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/scripting/include>")
target_link_libraries(engine-sim-scripting PUBLIC engine-sim::core piranha)
target_compile_features(engine-sim-scripting PUBLIC cxx_std_17)

# EngineSimApplication belongs to the platform-neutral visualization target.
target_link_libraries(engine-sim-visualization PUBLIC engine-sim-scripting)
target_compile_definitions(engine-sim-visualization PRIVATE ATG_ENGINE_SIM_PIRANHA_ENABLED)
