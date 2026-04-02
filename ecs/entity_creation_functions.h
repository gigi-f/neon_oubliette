// # filename:
// /home/gianfiorenzo/Documents/ag2_workspace/ecs/entity_creation_functions.h
#pragma once

#include <entt/entt.hpp>

namespace NeonOubliette {
// Forward declaration of the RegistryManager
class RegistryManager;

// Function to create the player entity
entt::entity createPlayerEntity(RegistryManager &rm, int start_x, int start_y,
                                int start_z, entt::registry &registry);

// Function to create a generic item entity
entt::entity createItemEntity(RegistryManager &rm, int start_x, int start_y,
                              int start_z, const std::string &name, char glyph,
                              uint32_t fg_color, entt::registry &registry);

// Function to create the Inspection Panel UI entity
entt::entity createInspectionPanelEntity(RegistryManager &rm,
                                         entt::registry &registry);
} // namespace NeonOubliette
