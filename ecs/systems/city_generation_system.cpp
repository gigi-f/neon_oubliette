#include "city_generation_system.h"
#include "../components/broadcast_components.h"
#include "../event_declarations.h"

namespace NeonOubliette {

void CityGenerationSystem::generate_chunk_content(entt::entity zone_entity) {
    auto const& zone = m_registry.get<MacroZoneComponent>(zone_entity);
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.empty()) return;
    auto& config = config_view.get<WorldConfigComponent>(config_view.front());

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

    // Deterministic Seed for this zone
    uint32_t zone_seed = config.world_seed ^ (static_cast<uint32_t>(zone.macro_x) * 73856093) ^ (static_cast<uint32_t>(zone.macro_y) * 19349663);
    std::mt19937 zone_gen(zone_seed);

    std::map<std::pair<int, int>, ArterialType> arterial_map;
    // Use ArterialGrid spatial index to expand segments into per-tile type map
    auto* grid = m_registry.ctx().find<ArterialGrid>();
    if (grid) {
        int cell_size = config.macro_cell_size;
        int sx = zone.macro_x * cell_size;
        int sy = zone.macro_y * cell_size;
        int ex = sx + cell_size - 1;
        int ey = sy + cell_size - 1;
        // Expand layer 0 (surface), layer -1 (sewer), layer 5 (rail)
        grid->expand_to_map(sx, sy, ex, ey, 0, arterial_map);
        grid->expand_to_map(sx, sy, ex, ey, -1, arterial_map);
        grid->expand_to_map(sx, sy, ex, ey, 5, arterial_map);
    }

    generateZoneInterior(zone, config.macro_cell_size, arterial_map, zone_gen, zone_entity, chunk_ent);

    // [O.1] Generate Raw Material Layer
    generateResourceFields(zone, config.macro_cell_size, zone_gen, chunk_ent);

    // [O.2] Generate Factory Content
    spawnFactoryExtras(zone, config.macro_cell_size, zone_gen);

    spawnCommerceHubExtras(zone, config.macro_cell_size, arterial_map, zone_gen);
    
    spawnUndergroundMedia(zone, zone_gen);
    
    for (auto const& [pos_pair, type] : arterial_map) {
        TerrainType t_type = TerrainType::VOID; char glyph = ' '; std::string color = "#000000";
        MaterialType material = MaterialType::CONCRETE; bool is_obstacle = false; bool is_liquid = false;
        int t_layer = 0;

        switch(type) {
            case ArterialType::WATERWAY_RIVER:
                t_type = TerrainType::VOID; glyph = '~'; color = "#4499DD";
                is_obstacle = true; material = MaterialType::WATER; is_liquid = true;
                break;
            case ArterialType::ROAD_PRIMARY:
                t_type = TerrainType::STREET; glyph = '.'; color = "#6688AA";
                material = MaterialType::CONCRETE;
                break;
            case ArterialType::ROAD_SECONDARY:
                t_type = TerrainType::STREET; glyph = '.'; color = "#556688";
                material = MaterialType::CONCRETE;
                break;
            case ArterialType::ROAD_ALLEY:
                t_type = TerrainType::STREET; glyph = '.'; color = "#443355";
                material = MaterialType::CONCRETE;
                break;
            case ArterialType::SIDEWALK:
                t_type = TerrainType::SIDEWALK; glyph = '.'; color = "#BBAA77";
                material = MaterialType::CONCRETE;
                break;
            case ArterialType::RAIL_ELEVATED:
                t_type = TerrainType::RAIL; glyph = '='; color = "#FFCC00";
                material = MaterialType::STEEL; t_layer = 5; 
                break;
            case ArterialType::SEWER:
                t_type = TerrainType::SEWER_WATER; glyph = '~'; color = "#225544";
                material = MaterialType::WATER; t_layer = -1; is_liquid = true;
                break;
            case ArterialType::UNDERGROUND_TUNNEL:
                t_type = TerrainType::SEWER_FLOOR; glyph = '.'; color = "#443322";
                material = MaterialType::CONCRETE; t_layer = -1;
                break;
            default: break;
        }

        // Node check for bridges
        for (auto ne : zone.arterial_entities) {
            if (!m_registry.all_of<InfrastructureNodeComponent>(ne)) continue;
            const auto& n_pos = m_registry.get<PositionComponent>(ne);
            const auto& node = m_registry.get<InfrastructureNodeComponent>(ne);
            if (n_pos.x == pos_pair.first && n_pos.y == pos_pair.second && node.is_bridge) {
                glyph = '='; color = "#AABB88";
                is_obstacle = false;
                material = MaterialType::STEEL;
                break;
            }
        }
        
        auto tile = m_registry.create();
        m_registry.emplace<PositionComponent>(tile, pos_pair.first, pos_pair.second, t_layer);
        m_registry.emplace<TerrainComponent>(tile, t_type);
        m_registry.emplace<RenderableComponent>(tile, glyph, color, t_layer);
        auto& phys = m_registry.emplace<Layer0PhysicsComponent>(tile);
        phys.material = material;
        phys.is_liquid = is_liquid;
        if (is_obstacle) m_registry.emplace<ObstacleComponent>(tile);

        if (type == ArterialType::ROAD_PRIMARY) {
            std::uniform_real_distribution<> v_dis(0.0, 1.0);
            if (v_dis(zone_gen) < 0.05) {
                PersonalVehicleType v_type = PersonalVehicleType::CAR;
                double r = v_dis(zone_gen);
                if (r < 0.3) v_type = PersonalVehicleType::SCOOTER;
                else if (r < 0.6) v_type = PersonalVehicleType::BIKE;
                else if (r < 0.9) v_type = PersonalVehicleType::CAR;
                else v_type = PersonalVehicleType::SCI_FI;
                spawnPersonalVehicle(pos_pair.first, pos_pair.second, t_layer, v_type);
            }
        }

        // [M.3] Sewer Item Spawning
        if (type == ArterialType::SEWER || type == ArterialType::UNDERGROUND_TUNNEL) {
            std::uniform_real_distribution<> item_dis(0.0, 1.0);
            if (item_dis(zone_gen) < 0.03) { // 3% chance per sewer tile
                double r = item_dis(zone_gen);
                auto item = m_registry.create();
                m_registry.emplace<PositionComponent>(item, pos_pair.first, pos_pair.second, -1);
                
                                if (r < 0.5) { // 50% chance for Fungus
                    m_registry.emplace<ItemComponent>(item, 100, "Sewer Fungus");
                    m_registry.emplace<RenderableComponent>(item, '*', "#AAFFAA", -1);
                    m_registry.emplace<ConsumableComponent>(item, 15, 5); // hunger, thirst
                    m_registry.emplace<ItemValueComponent>(item, 2);
                    m_registry.emplace<ItemMarketCategoryComponent>(item, ItemMarketCategory::FOOD);
                    m_registry.emplace<ItemMaterialComponent>(item, RawMaterialType::BIOMASS);
                } else if (r < 0.8) { // 30% chance for Scrap
                    m_registry.emplace<ItemComponent>(item, 101, "Industrial Scrap");
                    m_registry.emplace<RenderableComponent>(item, '%', "#888888", -1);
                    m_registry.emplace<ItemValueComponent>(item, 8);
                    m_registry.emplace<ItemMarketCategoryComponent>(item, ItemMarketCategory::TOOLS);
                    m_registry.emplace<ItemMaterialComponent>(item, RawMaterialType::METAL);
                } else if (r < 0.95) { // 15% chance for Gland
                    m_registry.emplace<ItemComponent>(item, 103, "Cacogen Gland");
                    m_registry.emplace<RenderableComponent>(item, ',', "#00FF00", -1);
                    m_registry.emplace<ItemValueComponent>(item, 60);
                    m_registry.emplace<ItemMarketCategoryComponent>(item, ItemMarketCategory::MEDICAL);
                    m_registry.emplace<ItemMaterialComponent>(item, RawMaterialType::BIOMASS);
                } else { // 5% chance for Syndicate Stash
                    m_registry.emplace<ItemComponent>(item, 102, "Contraband Case");
                    m_registry.emplace<RenderableComponent>(item, '[', "#FF0000", -1);
                    m_registry.emplace<ItemValueComponent>(item, 150);
                    m_registry.emplace<ContrabandComponent>(item);
                    m_registry.emplace<ItemMarketCategoryComponent>(item, ItemMarketCategory::CONTRABAND);
                    m_registry.emplace<ItemMaterialComponent>(item, RawMaterialType::ELECTRONIC);
                }
                m_registry.emplace<NameComponent>(item, m_registry.get<ItemComponent>(item).name);
            }

            // [M.4] Sewer Hazard Spawning
            std::uniform_real_distribution<> hazard_dis(0.0, 1.0);
            if (hazard_dis(zone_gen) < 0.02) { // 2% chance per sewer tile
                double hr = hazard_dis(zone_gen);
                auto haz_ent = m_registry.create();
                m_registry.emplace<PositionComponent>(haz_ent, pos_pair.first, pos_pair.second, -1);
                auto& haz = m_registry.emplace<TileHazardComponent>(haz_ent);
                
                char h_glyph = '&';
                std::string h_color = "#55FF55";
                std::string h_name = "Hazard";

                if (hr < 0.4) { // Toxic Gas
                    haz.type = HazardType::TOXIC_GAS;
                    haz.intensity = 0.3f;
                    h_glyph = '&'; h_color = "#55FF55"; h_name = "Toxic Gas Pocket";
                } else if (hr < 0.7) { // Steam Vent
                    haz.type = HazardType::STEAM_VENT;
                    haz.intensity = 0.8f;
                    haz.is_intermittent = true;
                    haz.pulse_rate = 15;
                    h_glyph = ','; h_color = "#BBBBBB"; h_name = "Steam Vent";
                } else if (hr < 0.9) { // Electrical Arc
                    haz.type = HazardType::ELECTRICAL;
                    haz.intensity = 1.0f;
                    haz.is_intermittent = true;
                    haz.pulse_rate = 8;
                    h_glyph = 'z'; h_color = "#FFFF00"; h_name = "Exposed Wiring";
                } else { // Bio-Hazard
                    haz.type = HazardType::BIO_HAZARD;
                    haz.intensity = 0.5f;
                    h_glyph = '*'; h_color = "#9900CC"; h_name = "Bio-Vats Leak";
                }

                m_registry.emplace<RenderableComponent>(haz_ent, h_glyph, h_color, -1);
                m_registry.emplace<NameComponent>(haz_ent, h_name);
            }
        }
    }

    // [M.1] Generate Sewer Portals (Manholes)
    auto node_view = m_registry.view<PositionComponent, InfrastructureNodeComponent>();
    for (auto node_ent : node_view) {
        const auto& node = node_view.get<InfrastructureNodeComponent>(node_ent);
        const auto& pos = node_view.get<PositionComponent>(node_ent);
        
        // Only process nodes in the current zone's cell
        if (pos.x >= zone.macro_x * config.macro_cell_size && pos.x < (zone.macro_x + 1) * config.macro_cell_size &&
            pos.y >= zone.macro_y * config.macro_cell_size && pos.y < (zone.macro_y + 1) * config.macro_cell_size) {
            
            if (node.node_name == "Sewer Manhole Access") {
                // Create manhole glyph on Layer 0
                auto manhole = m_registry.create();
                m_registry.emplace<PositionComponent>(manhole, pos.x, pos.y, 0);
                m_registry.emplace<RenderableComponent>(manhole, 'O', "#AAAAAA", 0);
                m_registry.emplace<NameComponent>(manhole, "Manhole Cover");
                m_registry.emplace<PortalComponent>(manhole, pos.x, pos.y, -1, true);
                m_registry.emplace<InfrastructureNodeComponent>(manhole).node_name = "Sewer Access (Surface)";
                m_registry.get<MacroZoneComponent>(zone_entity).arterial_entities.push_back(manhole);
                
                // Create landing on Layer -1
                auto landing = m_registry.create();
                m_registry.emplace<PositionComponent>(landing, pos.x, pos.y, -1);
                m_registry.emplace<RenderableComponent>(landing, '>', "#FFFFFF", -1);
                m_registry.emplace<NameComponent>(landing, "Sewer Ladder");
                m_registry.emplace<PortalComponent>(landing, pos.x, pos.y, 0, true);
                m_registry.emplace<InfrastructureNodeComponent>(landing).node_name = "Sewer Ladder (Underground)";
                m_registry.get<MacroZoneComponent>(zone_entity).arterial_entities.push_back(landing);
                
                // Ensure a floor exists under the ladder
                createTile(pos.x, pos.y, -1, TerrainType::SEWER_FLOOR, '.', "#443322", MaterialType::CONCRETE);
            }
        }
    }
}

void CityGenerationSystem::spawnFactoryExtras(const MacroZoneComponent& zone, int cell_size, std::mt19937& gen) {
    if (zone.type != ZoneType::INDUSTRIAL) return;

    // Find buildings in this zone
    auto building_view = m_registry.view<BuildingComponent, PositionComponent>();
    for (auto b_ent : building_view) {
        const auto& b_pos = building_view.get<PositionComponent>(b_ent);
        const auto& b_comp = building_view.get<BuildingComponent>(b_ent);
        
        // Ensure building is in the correct macro-cell range
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (config_view.empty()) continue;
        auto& config = config_view.get<WorldConfigComponent>(config_view.front());
        
        if (b_pos.x >= zone.macro_x * config.macro_cell_size && b_pos.x < (zone.macro_x + 1) * config.macro_cell_size &&
            b_pos.y >= zone.macro_y * config.macro_cell_size && b_pos.y < (zone.macro_y + 1) * config.macro_cell_size) {
            
            // Seed a factory component
            auto& factory = m_registry.emplace_or_replace<FactoryComponent>(b_ent);
            factory.is_active = true;
            
            // Add a default recipe based on some randomness
            std::uniform_real_distribution<> dist(0, 1);
            ProductionRecipe recipe;
                        if (dist(gen) < 0.5) {
                recipe.recipe_name = "Steel Plating";
                recipe.inputs[RawMaterialType::METAL] = 10.0f;
                recipe.output_item_type_id = 200;
                recipe.output_name = "Heavy Steel Plate";
                recipe.output_glyph = '#';
                recipe.output_color = "#999999";
                recipe.work_required = 50.0f;
                recipe.output_category = ItemMarketCategory::TOOLS;
                recipe.output_material = RawMaterialType::METAL;
            } else {
                recipe.recipe_name = "Circuit Assembly";
                recipe.inputs[RawMaterialType::ELECTRONIC] = 5.0f;
                recipe.inputs[RawMaterialType::METAL] = 2.0f;
                recipe.output_item_type_id = 201;
                recipe.output_name = "Control Unit";
                recipe.output_glyph = '[';
                recipe.output_color = "#00FFCC";
                recipe.work_required = 80.0f;
                recipe.output_category = ItemMarketCategory::TECHNOLOGY;
                recipe.output_material = RawMaterialType::ELECTRONIC;
            }
            factory.available_recipes.push_back(recipe);
            
            // Give some starting materials
            factory.input_stockpile[RawMaterialType::METAL] = 50.0f;
            factory.input_stockpile[RawMaterialType::ELECTRONIC] = 20.0f;
            
            // Log it
            m_dispatcher.trigger<LogEvent>({"Factory initialized at " + std::to_string(b_pos.x) + "," + std::to_string(b_pos.y), LogSeverity::INFO, "CityGenerationSystem"});
        }
    }
}

} // namespace NeonOubliette

// End of file marker

// EOF
