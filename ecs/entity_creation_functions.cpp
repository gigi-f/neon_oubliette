// //# filename:
// /home/gianfiorenzo/Documents/ag2_workspace/ecs/entity_creation_functions.cpp
#include "entity_creation_functions.h"
#include "../core/registry_manager.h"
#include "../util/color_constants.h"
#include "component_declarations.h"

namespace NeonOubliette {

entt::entity createPlayerEntity(RegistryManager &rm, int start_x, int start_y,
                                int start_z, entt::registry &registry) {
  auto player_entity = registry.create();
  registry.emplace<PositionComponent>(player_entity, start_x, start_y, start_z);
  registry.emplace<RenderableComponent>(player_entity, '@', COLOR_NEON_GREEN,
                                        COLOR_BLACK);
  registry.emplace<PlayerComponent>(player_entity);
  registry.emplace<PlayerCurrentLayerComponent>(player_entity, start_z);
  registry.emplace<NameComponent>(player_entity, "Player");
  registry.emplace<InventoryComponent>(player_entity);

  rm.setPlayerEntity(player_entity);
  return player_entity;
}

entt::entity createItemEntity(RegistryManager &rm, int start_x, int start_y,
                              int start_z, const std::string &name, char glyph,
                              uint32_t fg_color, entt::registry &registry) {
  auto item_entity = registry.create();
  registry.emplace<PositionComponent>(item_entity, start_x, start_y, start_z);
  registry.emplace<RenderableComponent>(item_entity, glyph, fg_color,
                                        COLOR_BLACK);
  registry.emplace<ItemComponent>(item_entity);
  registry.emplace<NameComponent>(item_entity, name);
  return item_entity;
}

entt::entity createInspectionPanelEntity(RegistryManager &rm,
                                         entt::registry &registry) {
  auto panel_entity = registry.create();
  // Position the inspection panel in the top-right corner of the screen
  // These coordinates will likely be screen-relative and handled by the
  // rendering system
  registry.emplace<PositionComponent>(panel_entity, 60, 2,
                                      999); // z=999 for UI layer
  // Initial state: hidden (e.g., transparent/empty renderable)
  registry.emplace<RenderableComponent>(
      panel_entity, ' ', COLOR_TRANSPARENT,
      COLOR_TRANSPARENT); // Initially invisible
  registry.emplace<InspectionDataComponent>(panel_entity, "");
  registry.emplace<NameComponent>(panel_entity, "Inspection Panel");

  rm.setInspectionPanelEntity(
      panel_entity); // Store this entity ID in RegistryManager
  return panel_entity;
}

} // namespace NeonOubliette
