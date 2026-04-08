#include "demolition_system.h"
#include "../components/components.h"
#include "../components/zoning_components.h"
#include "../components/simulation_layers.h"
#include "../event_declarations.h"

namespace NeonOubliette {

DemolitionSystem::DemolitionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    m_dispatcher.sink<DemolitionEvent>().connect<&DemolitionSystem::on_demolition_event>(this);
}

void DemolitionSystem::on_demolition_event(const DemolitionEvent& event) {
    if (!m_registry.valid(event.building_entity)) return;

    // 1. Get building data
    auto* pos_ptr = m_registry.try_get<PositionComponent>(event.building_entity);
    auto* size_ptr = m_registry.try_get<SizeComponent>(event.building_entity);
    auto* building_ptr = m_registry.try_get<BuildingComponent>(event.building_entity);
    
    if (!pos_ptr || !size_ptr || !building_ptr) return;

    const auto& pos = *pos_ptr;
    const auto& size = *size_ptr;
    const auto& building = *building_ptr;

    m_dispatcher.trigger(HUDNotificationEvent{"A building has been demolished at (" + std::to_string(pos.x) + "," + std::to_string(pos.y) + ").", 5.0f, "#FF5555"});

    // 2. Clear tiles in the footprint
    auto tile_view = m_registry.view<PositionComponent, TerrainComponent, RenderableComponent>();
    std::vector<entt::entity> tiles_to_reset;
    for (auto tile_ent : tile_view) {
        const auto& t_pos = tile_view.get<PositionComponent>(tile_ent);
        if (t_pos.x >= pos.x && t_pos.x < pos.x + size.width &&
            t_pos.y >= pos.y && t_pos.y < pos.y + size.height &&
            t_pos.layer_id == 0) {
            tiles_to_reset.push_back(tile_ent);
        }
    }

    for (auto tile_ent : tiles_to_reset) {
        auto& terrain = m_registry.get<TerrainComponent>(tile_ent);
        terrain.type = (building.zone_type == ZoneType::SLUM) ? TerrainType::DIRT : TerrainType::CONCRETE_FLOOR;
        
        auto& render = m_registry.get<RenderableComponent>(tile_ent);
        render.glyph = (terrain.type == TerrainType::DIRT) ? '\'' : '.';
        render.color = (terrain.type == TerrainType::DIRT) ? "#7A4422" : "#333333";
        
        m_registry.remove<ObstacleComponent>(tile_ent);
        
        // Reset physics material
        auto* phys = m_registry.try_get<Layer0PhysicsComponent>(tile_ent);
        if (phys) {
            phys->material = MaterialType::CONCRETE;
        }
    }

    // 3. Remove exterior door entities
    auto door_view = m_registry.view<PositionComponent, BuildingEntranceComponent>();
    std::vector<entt::entity> doors_to_remove;
    for (auto door_ent : door_view) {
        const auto& d_meta = door_view.get<BuildingEntranceComponent>(door_ent);
        if (d_meta.macro_building_id == event.building_entity) {
            doors_to_remove.push_back(door_ent);
        }
    }
    
    for (auto door_ent : doors_to_remove) {
        m_registry.destroy(door_ent);
    }

    // 4. Update the LotComponent and trigger RebuildEvent
    auto lot_view = m_registry.view<LotComponent>();
    entt::entity vacant_lot = entt::null;
    for (auto lot_ent : lot_view) {
        auto& lot = lot_view.get<LotComponent>(lot_ent);
        if (lot.building_entity == event.building_entity) {
            lot.building_entity = entt::null;
            vacant_lot = lot_ent;
            break;
        }
    }

    // 5. Cleanup Interior and related systems (e.g., Household management, Workplace assignments)
    // For now, we assume simple clearing.

    // 6. Finally, destroy the building entity itself
    m_registry.destroy(event.building_entity);

    // 7. Trigger a rebuild event for the vacant lot (this could also be handled by an EconomicSystem)
    if (vacant_lot != entt::null) {
        m_dispatcher.trigger(RebuildEvent{vacant_lot});
    }
}

} // namespace NeonOubliette
