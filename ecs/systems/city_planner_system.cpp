#include "city_planner_system.h"
#include "../components/infrastructure_components.h"
#include <algorithm>

namespace NeonOubliette {

CityPlannerSystem::CityPlannerSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    std::random_device rd;
    m_gen = std::mt19937(rd());
}

void CityPlannerSystem::initialize() {}
void CityPlannerSystem::update(double delta_time) { (void)delta_time; }

void CityPlannerSystem::plan_city_layout() {
    auto zone_view = m_registry.view<MacroZoneComponent>();
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.empty()) return;
    int cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    for (auto entity : zone_view) {
        subdivide_zone_into_blocks(entity);
    }

    apply_commerce_hub_upzoning();
}

void CityPlannerSystem::apply_commerce_hub_upzoning() {
    auto hub_view = m_registry.view<PositionComponent, CommerceHubComponent>();
    auto lot_view = m_registry.view<LotComponent>();

    for (auto hub_entity : hub_view) {
        const auto& hub_pos = hub_view.get<PositionComponent>(hub_entity);
        const auto& hub_comp = hub_view.get<CommerceHubComponent>(hub_entity);

        for (auto lot_entity : lot_view) {
            auto& lot = lot_view.get<LotComponent>(lot_entity);
            
            // Calculate Chebyshev distance to the nearest lot corner/edge
            int dx = std::max({0, lot.x - hub_pos.x, hub_pos.x - (lot.x + lot.width - 1)});
            int dy = std::max({0, lot.y - hub_pos.y, hub_pos.y - (lot.y + lot.height - 1)});
            int dist = std::max(dx, dy);

            if (dist <= hub_comp.influence_radius) {
                // Upzone to MIXED_COMMERCIAL
                lot.zone_class = ZoneType::MIXED_COMMERCIAL;
            }
        }
    }
}

void CityPlannerSystem::subdivide_zone_into_blocks(entt::entity zone_entity) {
    const auto& zone = m_registry.get<MacroZoneComponent>(zone_entity);
    auto config_view = m_registry.view<WorldConfigComponent>();
    int cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    int sx = zone.macro_x * cell_size;
    int sy = zone.macro_y * cell_size;
    int ex = sx + cell_size - 1;
    int ey = sy + cell_size - 1;

    // Determine subdivision pattern based on ZoneType
    // For A.1, we'll implement a basic grid subdivision.
    
    std::vector<std::pair<int, int>> block_coords; // {width, height}
    int num_splits_x = 1;
    int num_splits_y = 1;

    switch (zone.type) {
        case ZoneType::URBAN_CORE:
            num_splits_x = 2; num_splits_y = 2;
            break;
        case ZoneType::CORPORATE:
        case ZoneType::COMMERCIAL:
            num_splits_x = 2; num_splits_y = 2;
            break;
        case ZoneType::RESIDENTIAL:
            num_splits_x = 2; num_splits_y = 1;
            break;
        case ZoneType::INDUSTRIAL:
            num_splits_x = 1; num_splits_y = 1;
            break;
        case ZoneType::SLUM:
            num_splits_x = 3; num_splits_y = 2;
            break;
        default:
            num_splits_x = 1; num_splits_y = 1;
            break;
    }

    int block_w = (cell_size - (num_splits_x - 1)) / num_splits_x;
    int block_h = (cell_size - (num_splits_y - 1)) / num_splits_y;

    for (int ix = 0; ix < num_splits_x; ++ix) {
        for (int iy = 0; iy < num_splits_y; ++iy) {
            int bx = sx + ix * (block_w + 1);
            int by = sy + iy * (block_h + 1);
            
            // Create the block
            auto block_entity = create_block(bx, by, block_w, block_h, zone_entity);
            
            // Subdivide block into lots
            subdivide_block_into_lots(block_entity, zone.type);
            
            // Create streets between blocks (secondary roads)
            if (ix < num_splits_x - 1) {
                int street_x = bx + block_w;
                for (int y = by; y < by + block_h; ++y) {
                    auto street = m_registry.create();
                    m_registry.emplace<PositionComponent>(street, street_x, y, 0);
                    m_registry.emplace<InfrastructureArterialComponent>(street, ArterialType::ROAD_SECONDARY, 1.0f, true);
                    auto& field = m_registry.emplace<ConduitFieldComponent>(street);
                    field.radius = 1.0f;
                    field.economic_multiplier = 1.05f;
                    m_registry.get<MacroZoneComponent>(zone_entity).arterial_entities.push_back(street);
                }
            }
            if (iy < num_splits_y - 1) {
                int street_y = by + block_h;
                for (int x = bx; x < bx + block_w; ++x) {
                    auto street = m_registry.create();
                    m_registry.emplace<PositionComponent>(street, x, street_y, 0);
                    m_registry.emplace<InfrastructureArterialComponent>(street, ArterialType::ROAD_SECONDARY, 1.0f, true);
                    auto& field2 = m_registry.emplace<ConduitFieldComponent>(street);
                    field2.radius = 1.0f;
                    field2.economic_multiplier = 1.05f;
                    m_registry.get<MacroZoneComponent>(zone_entity).arterial_entities.push_back(street);
                }
            }
        }
    }
}

void CityPlannerSystem::subdivide_block_into_lots(entt::entity block_entity, ZoneType zone_type) {
    const auto& block = m_registry.get<BlockComponent>(block_entity);
    
    int lot_count_x = 1;
    int lot_count_y = 1;
    bool back_to_back = false;

    switch (zone_type) {
        case ZoneType::URBAN_CORE:
            lot_count_x = 1; lot_count_y = 1; // Skyscraper lots
            break;
        case ZoneType::CORPORATE:
            lot_count_x = 1; lot_count_y = 1; // Large monolithic lots
            break;
        case ZoneType::COMMERCIAL:
            lot_count_x = 2; lot_count_y = 1;
            break;
        case ZoneType::RESIDENTIAL:
            lot_count_x = 4; lot_count_y = 2; // Row houses / Apartments
            back_to_back = true;
            break;
        case ZoneType::SLUM:
            lot_count_x = 6; lot_count_y = 2; // Dense packing
            back_to_back = true;
            break;
        default:
            lot_count_x = 1; lot_count_y = 1;
            break;
    }

    // Overlap for shared walls: side-by-side lots share their boundary tile.
    // Back-to-back lots are separated by a 1-tile alley gap.
    
    int available_w = block.width + (lot_count_x > 1 ? (lot_count_x - 1) : 0);
    int lot_w = available_w / lot_count_x;
    
    int alley_gap = back_to_back ? 1 : 0;
    int available_h = block.height - alley_gap;
    int lot_h = available_h / lot_count_y;

    // Create alleys for back-to-back blocks
    if (back_to_back && lot_count_y > 1) {
        int alley_y = block.y + lot_h;
        for (int x = block.x; x < block.x + block.width; ++x) {
            auto alley = m_registry.create();
            m_registry.emplace<PositionComponent>(alley, x, alley_y, 0);
            m_registry.emplace<InfrastructureArterialComponent>(alley, ArterialType::ROAD_ALLEY, 0.5f, true);
            auto& field = m_registry.emplace<ConduitFieldComponent>(alley);
            field.radius = 0.5f;
            field.crime_modifier = 0.2f;
            m_registry.get<MacroZoneComponent>(block.zone_entity).arterial_entities.push_back(alley);
        }
    }

    for (int ix = 0; ix < lot_count_x; ++ix) {
        for (int iy = 0; iy < lot_count_y; ++iy) {
            int lx = block.x + ix * (lot_w - 1);
            int ly = block.y + iy * lot_h + (iy > 0 ? alley_gap : 0);
            
            // Adjust last lot to fill the block exactly
            int lw = (ix == lot_count_x - 1) ? (block.x + block.width - lx) : lot_w;
            int lh = (iy == lot_count_y - 1) ? (block.y + block.height - ly) : lot_h;

            if (lw > 1 && lh > 1) {
                StreetFacingSide alley_facing = StreetFacingSide::NONE;
                if (back_to_back && lot_count_y > 1) {
                    if (iy == 0) alley_facing = StreetFacingSide::SOUTH;
                    else if (iy == 1) alley_facing = StreetFacingSide::NORTH;
                }

                auto lot_entity = create_lot(lx, ly, lw, lh, zone_type, block_entity, alley_facing);
                m_registry.get<BlockComponent>(block_entity).lots.push_back(lot_entity);
            }
        }
    }
}

entt::entity CityPlannerSystem::create_block(int x, int y, int w, int h, entt::entity zone_entity) {
    auto entity = m_registry.create();
    auto& block = m_registry.emplace<BlockComponent>(entity, x, y, w, h);
    block.zone_entity = zone_entity;
    
    auto& zone = m_registry.get<MacroZoneComponent>(zone_entity);
    zone.block_entities.push_back(entity);
    
    return entity;
}

entt::entity CityPlannerSystem::create_lot(int x, int y, int w, int h, ZoneType zone_class, entt::entity block_entity, StreetFacingSide alley_facing) {
    auto entity = m_registry.create();
    auto& lot = m_registry.emplace<LotComponent>(entity);
    lot.x = x;
    lot.y = y;
    lot.width = w;
    lot.height = h;
    lot.zone_class = zone_class;
    lot.parent_block = block_entity;
    lot.alley_facing = alley_facing;
    
    lot.facing = determine_facing(x, y, w, h, m_registry.get<BlockComponent>(block_entity));
    
    return entity;
}

StreetFacingSide CityPlannerSystem::determine_facing(int x, int y, int w, int h, const BlockComponent& block) {
    uint8_t facing = static_cast<uint8_t>(StreetFacingSide::NONE);
    
    // Check if the lot is on the edge of the block
    if (x == block.x) facing |= static_cast<uint8_t>(StreetFacingSide::WEST);
    if (x + w == block.x + block.width) facing |= static_cast<uint8_t>(StreetFacingSide::EAST);
    if (y == block.y) facing |= static_cast<uint8_t>(StreetFacingSide::NORTH);
    if (y + h == block.y + block.height) facing |= static_cast<uint8_t>(StreetFacingSide::SOUTH);
    
    if (facing == 0) return StreetFacingSide::NORTH; // Default if somehow inside
    return static_cast<StreetFacingSide>(facing);
}

} // namespace NeonOubliette
