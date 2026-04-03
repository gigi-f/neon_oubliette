cmake_minimum_required(VERSION 3.15)
project(NeonGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- Find External Dependencies ---

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
)

# --- Executable ---
add_executable(neon_oubliette ${PROJECT_SOURCES})

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
)

# --- Post-Build: Copy Data ---
add_custom_command(TARGET neon_oubliette POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}/bin/data
)

set_target_properties(neon_oubliette PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
