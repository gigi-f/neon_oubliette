cmake_minimum_required(VERSION 3.15)
project(NeonGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NEON_ENABLE_COMPILER_CACHE "Use compiler cache launcher when available (ccache/sccache)." ON)
option(NEON_ENABLE_PCH "Enable precompiled headers for neon_oubliette." ON)
option(NEON_ENABLE_CLANG_TIME_TRACE "Enable Clang -ftime-trace for build profiling." OFF)
option(NEON_ENABLE_LTO "Enable Link Time Optimization (ThinLTO for Clang). Recommended for Release builds." OFF)
option(NEON_ENABLE_UNITY_BUILD "Combine source files into batches to reduce compile overhead (CMake >= 3.16)." OFF)
option(NEON_ENABLE_FAST_LINKER "Use lld if available for faster link times." ON)
option(NEON_ENABLE_TRACY "Enable Tracy profiler instrumentation" OFF)
option(NEON_ENABLE_IWYU "Enable include-what-you-use analysis." OFF)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(NEON_ENABLE_COMPILER_CACHE)
    if(NOT CMAKE_CXX_COMPILER_LAUNCHER)
        find_program(NEON_CCACHE_PROGRAM ccache)
        find_program(NEON_SCCACHE_PROGRAM sccache)

        if(NEON_CCACHE_PROGRAM)
            set(CMAKE_C_COMPILER_LAUNCHER "${NEON_CCACHE_PROGRAM}" CACHE STRING "C compiler launcher" FORCE)
            set(CMAKE_CXX_COMPILER_LAUNCHER "${NEON_CCACHE_PROGRAM}" CACHE STRING "CXX compiler launcher" FORCE)

            # Configure ccache for optimal performance with PCH, LTO, and multiple build directories
            set(ENV{CCACHE_SLOPPINESS} "pch_defines,time_macros,include_file_mtime")
            set(ENV{CCACHE_BASEDIR} "${CMAKE_SOURCE_DIR}")
            set(ENV{CCACHE_CONFIGPATH} "${CMAKE_SOURCE_DIR}/ccache.conf")

            message(STATUS "Compiler cache enabled via ccache: ${NEON_CCACHE_PROGRAM}")
        elseif(NEON_SCCACHE_PROGRAM)
            set(CMAKE_C_COMPILER_LAUNCHER "${NEON_SCCACHE_PROGRAM}" CACHE STRING "C compiler launcher" FORCE)
            set(CMAKE_CXX_COMPILER_LAUNCHER "${NEON_SCCACHE_PROGRAM}" CACHE STRING "CXX compiler launcher" FORCE)
            message(STATUS "Compiler cache enabled via sccache: ${NEON_SCCACHE_PROGRAM}")
        else()
            message(STATUS "Compiler cache requested but neither ccache nor sccache was found")
        endif()
    else()
        message(STATUS "Compiler launcher already set: ${CMAKE_CXX_COMPILER_LAUNCHER}")
    endif()
endif()

if(NEON_ENABLE_CLANG_TIME_TRACE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    add_compile_options(-ftime-trace)
endif()

if(NEON_ENABLE_IWYU)
    find_program(IWYU_PROGRAM include-what-you-use
        PATHS /opt/homebrew/bin /usr/local/bin
    )
    if(IWYU_PROGRAM)
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU_PROGRAM};-Xiwyu;--mapping_file=${CMAKE_SOURCE_DIR}/iwyu.imp" CACHE STRING "IWYU" FORCE)
        message(STATUS "IWYU enabled: ${IWYU_PROGRAM}")
    else()
        message(WARNING "IWYU requested but include-what-you-use was not found")
    endif()
endif()


# Always include debug symbols so backward-cpp can resolve source locations
string(APPEND CMAKE_CXX_FLAGS " -g")
string(APPEND CMAKE_C_FLAGS " -g")

# --- Find External Dependencies ---

include(FetchContent)

# Share downloaded FetchContent sources across all build directories
if(NOT FETCHCONTENT_BASE_DIR)
    set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.cmake-fc-cache" CACHE PATH
        "Shared directory for FetchContent downloads" FORCE)
endif()

FetchContent_Declare(
    inkcpp
    GIT_REPOSITORY https://github.com/JBenda/inkcpp.git
    GIT_TAG master
)
FetchContent_MakeAvailable(inkcpp)

# Suppress warnings from inkcpp headers by marking them as system includes
get_target_property(_inkcpp_includes inkcpp INTERFACE_INCLUDE_DIRECTORIES)
if(_inkcpp_includes)
    set_target_properties(inkcpp PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_inkcpp_includes}"
    )
endif()

FetchContent_Declare(
    backward
    GIT_REPOSITORY https://github.com/bombela/backward-cpp.git
    GIT_TAG master
)
FetchContent_MakeAvailable(backward)

if(NEON_ENABLE_TRACY)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.13.1
    )
    FetchContent_MakeAvailable(tracy)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED notcurses)

find_path(CEREAL_INCLUDE_DIR
    NAMES cereal/cereal.hpp
    PATHS
        /opt/homebrew/include
        /usr/local/include
        ${CMAKE_SOURCE_DIR}/external/cereal/include
)

if(NOT CEREAL_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find Cereal headers.")
endif()

# --- Source Files ---
set(PROJECT_SOURCES
    feature_flags.cpp
    src/main.cpp
    src/config/ConfigLoader.cpp
    src/config/schema_validator.cpp
    external/json-schema-validator/json-validator.cpp
    external/json-schema-validator/json-uri.cpp
    external/json-schema-validator/json-patch.cpp
    external/json-schema-validator/json-schema-draft7.json.cpp
    external/json-schema-validator/string-format-check.cpp
    external/json-schema-validator/smtp-address-validator.cpp
    ecs/component_registration.cpp
    ecs/system_scheduler.cpp
    ecs/system_registration.cpp
    ecs/simulation_coordinator.cpp
    ecs/systems/input_system.cpp
    ecs/systems/agent_action_system.cpp
    ecs/systems/agent_decision_system.cpp
    ecs/systems/agent_spawn_system.cpp
    ecs/systems/pathfinding_system.cpp
    ecs/systems/movement_system.cpp
    ecs/systems/interaction_system.cpp
    ecs/systems/dialogue_system.cpp
    ecs/systems/grammar_engine.cpp
    ecs/systems/god_mode_system.cpp
    ecs/systems/inspection_system.cpp
    ecs/systems/crafting_system.cpp
    ecs/systems/consumption_system.cpp
    ecs/systems/container_system.cpp
    ecs/systems/item_usage_system.cpp
    ecs/systems/rendering_system.cpp
    ecs/systems/serialization_system.cpp
    ecs/systems/turn_manager_system.cpp
    ecs/systems/logging_system.cpp
    ecs/systems/player_movement_system.cpp
    ecs/systems/player_current_layer_system.cpp
    ecs/systems/traffic_flow_system.cpp
    ecs/systems/activity_system.cpp
    ecs/systems/barter_system.cpp
    ecs/systems/faction_system.cpp
    ecs/systems/religion_system.cpp
    ecs/systems/economic_system.cpp
    ecs/systems/stock_market_system.cpp
    ecs/systems/zoning_solver_system.cpp
    ecs/systems/city_planner_system.cpp
    ecs/systems/infrastructure_network_system.cpp
    ecs/systems/chunk_streaming_system.cpp
    ecs/systems/macro_navigation_system.cpp
    ecs/systems/transit_system.cpp
    ecs/systems/social_interaction_system.cpp
    ecs/systems/information_system.cpp
    ecs/systems/xeno_system.cpp
    ecs/systems/sound_system.cpp
    ecs/systems/conversation_system.cpp
    ecs/systems/crisis_system.cpp
    ecs/systems/crisis_dashboard_system.cpp
    ecs/systems/milestone_system.cpp
    ecs/systems/drug_manufacturing_system.cpp
    ecs/systems/wanted_level_system.cpp
    ecs/systems/guard_response_system.cpp
    ecs/systems/inheritance_system.cpp
    ecs/systems/urban_decay_system.cpp
    ecs/systems/city_generation_system.cpp
    ecs/systems/broadcast_tower_system.cpp
    ecs/systems/underground_media_system.cpp
    ecs/systems/power_grid_system.cpp
    ecs/systems/demolition_system.cpp
    ecs/systems/rebuilding_system.cpp
    ecs/systems/production_system.cpp
    ecs/systems/supply_chain_system.cpp
)

# --- Executable ---
add_executable(neon_oubliette ${PROJECT_SOURCES})
add_backward(neon_oubliette)

if(NEON_ENABLE_PCH)
    if(COMMAND target_precompile_headers)
        target_precompile_headers(neon_oubliette PRIVATE
            # Containers
            <array>
            <map>
            <queue>
            <set>
            <unordered_map>
            <unordered_set>
            <vector>
            # Strings / IO
            <iostream>
            <fstream>
            <sstream>
            <iomanip>
            <string>
            <string_view>
            # Utilities
            <algorithm>
            <chrono>
            <cmath>
            <cstdint>
            <functional>
            <memory>
            <optional>
            <random>
            <utility>
            <variant>
            # Heavy third-party headers — parsed once, shared by all TUs
            # entt: prevents re-instantiation of basic_sigh_mixin/basic_storage per TU
            <entt/entt.hpp>
            # nlohmann/json: prevents re-instantiation of get_to<vector<T>> chains per TU
            <nlohmann/json.hpp>
        )
    else()
        message(WARNING "NEON_ENABLE_PCH is ON but target_precompile_headers is unavailable in this CMake version")
    endif()
endif()

target_include_directories(neon_oubliette PRIVATE
    ${CMAKE_SOURCE_DIR}/external/json-schema-validator
    ${CMAKE_SOURCE_DIR}/external/nlohmann_json
    ${CMAKE_SOURCE_DIR}/external/entt
    ${CEREAL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/ecs
    ${NOTCURSES_INCLUDE_DIRS}
)

# --- Link Directories ---
target_link_directories(neon_oubliette PRIVATE
    ${NOTCURSES_LIBRARY_DIRS}
)

# --- Link Libraries ---
target_link_libraries(neon_oubliette PRIVATE
    ${NOTCURSES_LIBRARIES}
    inkcpp
    inkcpp_compiler
)

if(NEON_ENABLE_TRACY)
    target_link_libraries(neon_oubliette PRIVATE Tracy::TracyClient)
    target_compile_definitions(neon_oubliette PRIVATE TRACY_ENABLE)
endif()

# --- Post-Build: Copy Data ---
add_custom_command(TARGET neon_oubliette POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}/bin/data
)

set_target_properties(neon_oubliette PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# --- Unity Build ---
if(NEON_ENABLE_UNITY_BUILD)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
        set_target_properties(neon_oubliette PROPERTIES
            UNITY_BUILD ON
            UNITY_BUILD_BATCH_SIZE 8
        )
        message(STATUS "Unity build enabled (batch size 8)")
    else()
        message(WARNING "NEON_ENABLE_UNITY_BUILD requires CMake >= 3.16; ignored.")
    endif()
endif()

# --- Link Time Optimization ---
if(NEON_ENABLE_LTO)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        target_compile_options(neon_oubliette PRIVATE -flto=thin)
        target_link_options(neon_oubliette PRIVATE -flto=thin)
        message(STATUS "LTO enabled: ThinLTO (${CMAKE_CXX_COMPILER_ID})")
    else()
        include(CheckIPOSupported)
        check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_output LANGUAGES CXX)
        if(_ipo_supported)
            set_target_properties(neon_oubliette PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
            message(STATUS "LTO enabled: full IPO (${CMAKE_CXX_COMPILER_ID})")
        else()
            message(WARNING "LTO requested but IPO not supported: ${_ipo_output}")
        endif()
    endif()
endif()

# --- Fast Linker Configuration ---
if(NEON_ENABLE_FAST_LINKER)
    if(APPLE)
        # Force Apple's modern ld_prime linker (Xcode 15+)
        # This is heavily multi-threaded and optimized for Apple Silicon.
        add_link_options("-Wl,-ld_new")
        message(STATUS "Fast linker: Apple ld_prime enabled via -Wl,-ld_new")
    else()
        # Fallback to lld for Linux/Other platforms
        find_program(NEON_LLD_PROGRAM
            NAMES lld-18 lld-17 lld-16 lld
            PATHS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin /usr/local/bin /usr/bin
        )
        if(NEON_LLD_PROGRAM)
            target_link_options(neon_oubliette PRIVATE -fuse-ld=lld)
            message(STATUS "Fast linker: lld (${NEON_LLD_PROGRAM})")
        endif()
    endif()
endif()
