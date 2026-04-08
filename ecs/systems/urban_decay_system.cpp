#include "urban_decay_system.h"
#include <algorithm>
#include <random>
#include "../../visual_design/graffiti_visuals.h"
#include "../components/lod_components.h"
#include "../components/simulation_layers.h"

namespace NeonOubliette {

void UrbanDecaySystem::update(double delta_time) {
    (void)delta_time;

    // 1. Get global environmental state
    auto weather_view = m_registry.view<WeatherComponent>();
    if (weather_view.empty()) return;
    const auto& weather = weather_view.get<WeatherComponent>(weather_view.front());

    // [K.2] Reset squatter counts for recounting
    auto health_reset_view = m_registry.view<BuildingHealthComponent>();
    for (auto entity : health_reset_view) {
        health_reset_view.get<BuildingHealthComponent>(entity).num_squatters = 0;
    }

    // [K.2] Count squatters per building
    auto squatter_view = m_registry.view<HomeComponent>();
    for (auto agent : squatter_view) {
        const auto& home = squatter_view.get<HomeComponent>(agent);
        if (home.is_squatting && m_registry.valid(home.building_entity) && m_registry.all_of<BuildingHealthComponent>(home.building_entity)) {
            m_registry.get<BuildingHealthComponent>(home.building_entity).num_squatters++;
        }
    }

    // 2. Process all buildings
    auto building_view = m_registry.view<BuildingHealthComponent, BuildingComponent>();
    
    for (auto entity : building_view) {
        auto& health = building_view.get<BuildingHealthComponent>(entity);
        const auto& building = building_view.get<BuildingComponent>(entity);

        // Calculate local pollution (could be expanded to a spatial lookup)
        float local_pollution = 0.0f;
        auto* pollution_comp = m_registry.try_get<PollutionLevelComponent>(entity);
        if (pollution_comp) {
            local_pollution = pollution_comp->air_pollution;
        } else {
            // Contextual pollution based on zone
            if (building.zone_type == ZoneType::INDUSTRIAL) local_pollution = 0.5f;
            else if (building.zone_type == ZoneType::SLUM) local_pollution = 0.3f;
        }

        // Apply decay
        apply_decay(entity, health, weather, local_pollution);

        // Apply repairs (if owner is assigned and has funds)
        auto* property = m_registry.try_get<PropertyComponent>(entity);
        if (property) {
            apply_repairs(entity, health, *property);
        }

        // [K.2] Update maintenance urgency
        if (health.integrity < health.max_integrity) {
            health.maintenance_urgency = (1.0f - (health.integrity / health.max_integrity));
            // High-value zones prioritize maintenance
            if (building.zone_type == ZoneType::CORPORATE || building.zone_type == ZoneType::URBAN_CORE) {
                health.maintenance_urgency *= 1.5f;
            }
        } else {
            health.maintenance_urgency = 0.0f;
        }

        // Trigger effects based on health thresholds
        trigger_environmental_effects(entity, health);

        // [K.4] Graffiti Processing
        auto* b_pos = m_registry.try_get<PositionComponent>(entity);
        auto* b_size = m_registry.try_get<SizeComponent>(entity);
        if (b_pos && b_size) {
            process_graffiti(entity, health, building, *b_pos, *b_size);
        }
    }
}

void UrbanDecaySystem::process_graffiti(entt::entity building_ent, BuildingHealthComponent& health, const BuildingComponent& building_comp, const PositionComponent& b_pos, const SizeComponent& b_size) {
    (void)building_ent; (void)building_comp;
    // 1. Determine local influence
    int chunk_x_idx = b_pos.x / 40;
    int chunk_y_idx = b_pos.y / 40;
    
    std::string top_faction = "NEUTRAL";
    float max_influence = 0.0f;
    
    auto chunk_view = m_registry.view<ChunkComponent, FactionInfluenceFieldComponent>();
    for (auto ce : chunk_view) {
        const auto& c = chunk_view.get<ChunkComponent>(ce);
        if (c.chunk_x == chunk_x_idx && c.chunk_y == chunk_y_idx) {
            const auto& inf = chunk_view.get<FactionInfluenceFieldComponent>(ce);
            for (auto const& [fid, amount] : inf.influence) {
                if (amount > max_influence) {
                    max_influence = amount;
                    top_faction = fid;
                }
            }
            break;
        }
    }

    // 2. Chance to TAG
    // Pressure increases with low integrity and high local influence
    float integrity_ratio = health.integrity / health.max_integrity;
    float tag_pressure = (1.0f - integrity_ratio) * (0.1f + max_influence / 100.0f);
    
    static std::mt19937 gen(1337);
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    if (dis(gen) < tag_pressure * 0.1f) { 
        // 3. Find a wall/window tile to tag
        auto terrain_view = m_registry.view<PositionComponent, TerrainComponent>();
        std::vector<entt::entity> footprint_tiles;
        for (auto t_ent : terrain_view) {
            const auto& t_pos = terrain_view.get<PositionComponent>(t_ent);
            if (t_pos.layer_id == b_pos.layer_id &&
                t_pos.x >= b_pos.x && t_pos.x < b_pos.x + b_size.width &&
                t_pos.y >= b_pos.y && t_pos.y < b_pos.y + b_size.height) {
                
                const auto& t_type = terrain_view.get<TerrainComponent>(t_ent).type;
                if (t_type == TerrainType::WALL || t_type == TerrainType::WINDOW) {
                    footprint_tiles.push_back(t_ent);
                }
            }
        }
        
        if (!footprint_tiles.empty()) {
            std::uniform_int_distribution<size_t> tile_dis(0, footprint_tiles.size() - 1);
            entt::entity target_tile = footprint_tiles[tile_dis(gen)];
            
            auto& graffiti = m_registry.get_or_emplace<GraffitiComponent>(target_tile);
            
            if (top_faction != "NEUTRAL" && Visuals::FACTION_TAGS.count(top_faction)) {
                const auto& style = Visuals::FACTION_TAGS.at(top_faction);
                graffiti.glyph = style.glyph;
                graffiti.color = style.color;
                graffiti.density = std::min(1.0f, graffiti.density + 0.25f);
                graffiti.type = GraffitiType::TERRITORY;
            } else {
                std::uniform_int_distribution<size_t> g_dis(0, Visuals::GENERIC_TAGS.size() - 1);
                std::uniform_int_distribution<size_t> c_dis(0, Visuals::DECAY_COLORS.size() - 1);
                graffiti.glyph = Visuals::GENERIC_TAGS[g_dis(gen)];
                graffiti.color = Visuals::DECAY_COLORS[c_dis(gen)];
                graffiti.density = std::min(1.0f, graffiti.density + 0.15f);
                graffiti.type = GraffitiType::TAG;
            }
        }
    }

    // 4. Maintenance Cleanup
    if (integrity_ratio > 0.85f) {
        std::vector<entt::entity> to_remove;
        auto terrain_v = m_registry.view<PositionComponent, GraffitiComponent>();
        for (auto te : terrain_v) {
            const auto& p = terrain_v.get<PositionComponent>(te);
            if (p.layer_id == b_pos.layer_id && p.x >= b_pos.x && p.x < b_pos.x + b_size.width && p.y >= b_pos.y && p.y < b_pos.y + b_size.height) {
                 auto& g = terrain_v.get<GraffitiComponent>(te);
                 g.density -= 0.1f;
                 if (g.density <= 0.0f) {
                     to_remove.push_back(te);
                 }
            }
        }
        for (auto te : to_remove) m_registry.remove<GraffitiComponent>(te);
    }
}

void UrbanDecaySystem::apply_decay(entt::entity building, BuildingHealthComponent& health, const WeatherComponent& weather, float local_pollution) {
    float decay_this_tick = health.base_decay_rate;

    // Weather modifiers
    if (weather.state == WeatherState::ACID_RAIN) {
        decay_this_tick += 0.05f * weather.intensity;
    } else if (weather.state == WeatherState::HEAVY_RAIN) {
        decay_this_tick += 0.02f * weather.intensity;
    }

    // [L.3] Environmental Hazard modifiers
    auto hazard_view = m_registry.view<EnvironmentalHazardComponent>();
    if (!hazard_view.empty()) {
        auto& hazard = hazard_view.get<EnvironmentalHazardComponent>(hazard_view.front());
        if (hazard.is_active) {
            decay_this_tick += hazard.acidity_level * 0.15f; // Significant acceleration
        }
    }

    // Pollution modifier
    decay_this_tick += local_pollution * 0.03f;

    // [K.2] Squatter modifier: more squatters = more waste/wear
    if (health.num_squatters > 0) {
        decay_this_tick += 0.02f * health.num_squatters;
    }

    // Macro-Zone modifiers (Slums decay faster, Corporate slower)
    auto* building_comp = m_registry.try_get<BuildingComponent>(building);
    if (building_comp) {
        if (building_comp->zone_type == ZoneType::SLUM) decay_this_tick *= 1.5f;
        if (building_comp->zone_type == ZoneType::CORPORATE || building_comp->zone_type == ZoneType::URBAN_CORE) decay_this_tick *= 0.5f;
    }

    health.integrity = std::max(0.0f, health.integrity - decay_this_tick);
}

void UrbanDecaySystem::apply_repairs(entt::entity building, BuildingHealthComponent& health, PropertyComponent& property) {
    if (health.integrity >= health.max_integrity) return;

    // Maintenance logic: Owners (Factions or Agents) spend credits to maintain integrity.
    // For now, we'll simulate an automatic "maintenance fund" tied to the property's value.
    
    float repair_amount = 0.0f;
    int repair_cost = 0;

    // Simple heuristic: repair 0.1% integrity per tick if funded.
    float target_repair = 0.1f;
    repair_cost = static_cast<int>(target_repair * 10.0f); // 1 credit per 0.01 integrity

    // In a full implementation, we'd check the owner's credits. 
    // For K.1, we assume baseline maintenance for high-value properties.
    if (property.current_market_value > 5000) {
        repair_amount = target_repair;
    } else if (property.current_market_value > 1000) {
        repair_amount = target_repair * 0.5f;
    }

    health.integrity = std::min(health.max_integrity, health.integrity + repair_amount);
}

void UrbanDecaySystem::trigger_environmental_effects(entt::entity building, BuildingHealthComponent& health) {
    // Threshold 1: Condemned
    if (health.integrity < 10.0f && !health.is_condemned) {
        health.is_condemned = true;
        m_dispatcher.trigger(HUDNotificationEvent{"A building has been condemned due to structural failure.", 5.0f, "#FF0000"});
    } else if (health.integrity >= 10.0f && health.is_condemned) {
        health.is_condemned = false;
    }

    // Threshold 2: Demolition [K.3]
    if (health.integrity <= 0.0f) {
        m_dispatcher.trigger(DemolitionEvent{building});
    }

    // Threshold 3: Window Breakage (Phase A.10 / K.1)
    // If integrity < 50%, there's a chance each tick to break a window.
    if (health.integrity < 50.0f) {
        // This would involve finding tiles associated with the building and changing them.
        // For now, we'll just flag the component as "degraded".
    }
}

} // namespace NeonOubliette
