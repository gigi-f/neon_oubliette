#include "rebuilding_system.h"
#include "city_generation_system.h"
#include "../components/components.h"
#include "../components/zoning_components.h"
#include "../components/infrastructure_components.h"
#include "../event_declarations.h"

namespace NeonOubliette {

RebuildingSystem::RebuildingSystem(entt::registry& registry, entt::dispatcher& dispatcher, CityGenerationSystem& gen_system)
    : m_registry(registry), m_dispatcher(dispatcher), m_gen_system(gen_system) {
    m_dispatcher.sink<RebuildEvent>().connect<&RebuildingSystem::on_rebuild_event>(this);
    std::random_device rd;
    m_gen.seed(rd());
}

void RebuildingSystem::on_rebuild_event(const RebuildEvent& event) {
    if (!m_registry.valid(event.lot_entity)) return;

    auto& lot = m_registry.get<LotComponent>(event.lot_entity);
    if (lot.building_entity != entt::null) return; // Already occupied

    // 1. Determine zone and block context
    if (!m_registry.valid(lot.parent_block)) return;
    const auto& block = m_registry.get<BlockComponent>(lot.parent_block);
    
    // Find macro zone to get arterial context and density
    entt::entity zone_ent = block.zone_entity;
    if (!m_registry.valid(zone_ent)) return;
    const auto& zone = m_registry.get<MacroZoneComponent>(zone_ent);

    // 2. Identify arterials in this lot's area for door validation
    std::map<std::pair<int, int>, ArterialType> arterial_map;
    auto* grid = m_registry.ctx().find<ArterialGrid>();
    if (grid) {
        grid->expand_to_map(lot.x, lot.y, lot.x + lot.width - 1, lot.y + lot.height - 1, 0, arterial_map);
    }

    // 3. Prepare building parameters
    int bx = lot.x; int by = lot.y; int bw = lot.width; int bh = lot.height;
    
    // Check for sidewalks and adjust footprint
    // (Simplification: we assume the lot already has sidewalk buffers if needed, 
    // but we'll re-apply the logic from CityGenerationSystem)
    // Actually, for simplicity in rebuilding, we'll just build on the whole lot.
    
    std::string b_name = "New Structure";
    std::string b_color = "#AAAAAA";
    int floors = 1;
    
    switch(lot.zone_class) {
        case ZoneType::CORPORATE: b_name = "Corporate Office"; b_color = "#4488FF"; floors = 12; break;
        case ZoneType::SLUM: b_name = "Recycled Shanty"; b_color = "#CC7733"; floors = 1; break;
        case ZoneType::INDUSTRIAL: b_name = "Automated Plant"; b_color = "#DD4422"; floors = 2; break;
        case ZoneType::RESIDENTIAL: b_name = "Modern Apartments"; b_color = "#33AA55"; floors = 4; break;
        case ZoneType::URBAN_CORE: b_name = "Urban Tower"; b_color = "#FFFFFF"; floors = 30; break;
        default: break;
    }

    uint8_t shared_sides = m_gen_system.calculateSharedSides(lot, block);
    auto doors = m_gen_system.calculateDoorPositions(lot, bx, by, bw, bh, shared_sides, arterial_map);
    uint32_t stable_id = static_cast<uint32_t>(bx * 10000 + by);

    // Find parent chunk for interior caching
    entt::entity chunk_ent = entt::null;
    auto chunk_view = m_registry.view<ChunkComponent>();
    for (auto ce : chunk_view) {
        const auto& c = chunk_view.get<ChunkComponent>(ce);
        if (c.chunk_x == zone.macro_x / 2 && c.chunk_y == zone.macro_y / 2) {
            chunk_ent = ce;
            break;
        }
    }

    // 4. Create the building shell
    if (lot.zone_class == ZoneType::URBAN_CORE) {
        m_gen_system.createSkyscraperShell(b_name, bx, by, bw, bh, floors, b_color, lot.zone_class, (uint8_t)lot.facing, (uint8_t)lot.alley_facing, shared_sides, doors, stable_id, chunk_ent, m_gen);
    } else {
        m_gen_system.createBuildingShell(b_name, bx, by, bw, bh, floors, b_color, lot.zone_class, (uint8_t)lot.facing, (uint8_t)lot.alley_facing, shared_sides, doors, stable_id, chunk_ent, m_gen);
    }

    // 5. Link the new building entity back to the lot
    // Since createBuildingShell doesn't return the entity, we find it by position and stable_id
    auto building_view = m_registry.view<PositionComponent, BuildingComponent>();
    for (auto b_ent : building_view) {
        const auto& b_pos = building_view.get<PositionComponent>(b_ent);
        const auto& b_comp = building_view.get<BuildingComponent>(b_ent);
        if (b_pos.x == bx && b_pos.y == by && b_comp.building_id == stable_id) {
            lot.building_entity = b_ent;
            break;
        }
    }

    m_dispatcher.trigger(HUDNotificationEvent{"Reconstruction complete at (" + std::to_string(bx) + "," + std::to_string(by) + ").", 5.0f, "#00FF55"});
}

} // namespace NeonOubliette
