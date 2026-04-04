#include "agent_spawn_system.h"
#include "../components/simulation_layers.h"
#include "../components/lod_components.h"
#include <random>
#include <iostream>

namespace NeonOubliette {

void AgentSpawnSystem::spawnAgentsIntoChunks(int total_count) {
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.begin() == config_view.end()) return;
    auto& config = config_view.get<WorldConfigComponent>(*config_view.begin());

    auto chunk_view = m_registry.view<ChunkComponent>();
    if (chunk_view.begin() == chunk_view.end()) {
        std::cerr << "[AgentSpawnSystem] Cannot spawn into chunks: No chunks found." << std::endl;
        return;
    }

    std::vector<entt::entity> chunks;
    for (auto entity : chunk_view) chunks.push_back(entity);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disChunk(0, (int)chunks.size() - 1);
    std::uniform_int_distribution<> disType(0, 10);
    
    int cell_size = config.macro_cell_size;
    int chunk_size = cell_size * 2;

    for (int i = 0; i < total_count; ++i) {
        auto chunk_ent = chunks[disChunk(gen)];
        auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
        
        MacroAgentRecord record;
        record.name = "Citizen #" + std::to_string(i);
        record.archetype = (disType(gen) < 9) ? "Citizen" : "Guard";
        
        // Species and Xeno roll
        int species_roll = disType(gen);
        if (species_roll < 7) {
            record.species = SpeciesType::HUMAN;
        } else if (species_roll < 9) {
            record.species = SpeciesType::SYNTHETIC;
        } else {
            // Rare rolls (Cacogen, Hierodule)
            int xeno_roll = disType(gen);
            if (xeno_roll < 8) {
                record.species = SpeciesType::CACOGEN;
                record.is_xeno = true;
                record.xeno_type = XenoType::CACOGEN;
                record.name = "Cacogen #" + std::to_string(i);
                record.archetype = "Xeno";
            } else {
                record.species = SpeciesType::HIERODULE;
                record.is_xeno = true;
                record.xeno_type = XenoType::HIERODULE;
                record.name = "Hierodule #" + std::to_string(i);
                record.archetype = "Hierodule";
            }
        }
        
        record.is_autonomous = (record.species == SpeciesType::SYNTHETIC || record.is_xeno) ? (disType(gen) > 1) : true;
        
        // Random position within chunk
        std::uniform_int_distribution<> disX(chunk.chunk_x * chunk_size, (chunk.chunk_x + 1) * chunk_size - 1);
        std::uniform_int_distribution<> disY(chunk.chunk_y * chunk_size, (chunk.chunk_y + 1) * chunk_size - 1);
        
        record.x = disX(gen);
        record.y = disY(gen);
        record.layer_id = 0;
        record.hunger = 100.0f;
        record.thirst = 100.0f;
        record.frustration = 0.0f;
        record.consciousness = 1.0f;
        record.cash_on_hand = 100;
        record.faction_id = (record.archetype == "Guard") ? "GOVERNMENT" : "CITIZEN";

        // [NEW] Social Hierarchy assignment for macro agents
        if (record.is_xeno) {
            record.status = (record.species == SpeciesType::HIERODULE) ? 0.95f : 0.85f;
            record.class_title = (record.species == SpeciesType::HIERODULE) ? "Hierodule" : "Cacogen";
            record.faction_id = (record.species == SpeciesType::HIERODULE) ? "VOID" : "MAW";
        } else if (record.faction_id == "GOVERNMENT") {
            record.status = 0.7f;
            record.class_title = (record.archetype == "Guard") ? "Enforcer" : "Administrator";
        } else if (record.faction_id == "MAW") {
            record.status = (record.species == SpeciesType::SYNTHETIC) ? 0.9f : 0.1f;
            record.class_title = (record.species == SpeciesType::SYNTHETIC) ? "Acolyte" : "Substrate";
        } else if (record.faction_id == "SYNDICATE") {
            record.status = 0.6f;
            record.class_title = "Member";
        } else if (record.faction_id == "REBEL") {
            record.status = 0.2f;
            record.class_title = "Outlaw";
        } else {
            record.status = 0.4f;
            record.class_title = "Citizen";
        }
        
        if (record.species == SpeciesType::SYNTHETIC && record.faction_id != "MAW") {
            record.status -= 0.15f;
        }
        
        if (!record.is_autonomous) {
            record.status = 0.05f;
            record.class_title = "Automaton";
        }

        // --- Persistent Behavior Assignment (Home/Work) ---
        // Find a residential zone for home and a commercial/industrial for work within the same chunk
        entt::entity res_zone = entt::null;
        entt::entity work_zone = entt::null;

        for (auto zone_ent : chunk.macro_zones) {
            if (!m_registry.valid(zone_ent)) continue;
            auto& zone = m_registry.get<MacroZoneComponent>(zone_ent);
            if (zone.type == ZoneType::RESIDENTIAL || zone.type == ZoneType::SLUM) res_zone = zone_ent;
            if (zone.type == ZoneType::CORPORATE || zone.type == ZoneType::INDUSTRIAL || zone.type == ZoneType::COMMERCIAL || zone.type == ZoneType::AIRPORT) work_zone = zone_ent;
            
            if (zone.type == ZoneType::COLOSSEUM) {
                if (disType(gen) < 3) { // 30% chance for gladiators in colosseum zones
                    record.archetype = "Gladiator";
                    record.faction_id = "SYNDICATE";
                    work_zone = zone_ent;
                } else {
                    work_zone = zone_ent; // Others work there as staff
                }
            }
        }

        if (m_registry.valid(res_zone)) {
            auto& zone = m_registry.get<MacroZoneComponent>(res_zone);
            std::uniform_int_distribution<> zX(zone.macro_x * cell_size, (zone.macro_x + 1) * cell_size - 1);
            std::uniform_int_distribution<> zY(zone.macro_y * cell_size, (zone.macro_y + 1) * cell_size - 1);
            record.home_x = zX(gen);
            record.home_y = zY(gen);
        }

        if (m_registry.valid(work_zone)) {
            auto& zone = m_registry.get<MacroZoneComponent>(work_zone);
            if (zone.type == ZoneType::AIRPORT && disType(gen) < 4) { // 40% chance of airport workers being guards
                record.archetype = "Guard";
                record.faction_id = "GOVERNMENT";
            }
            std::uniform_int_distribution<> zX(zone.macro_x * cell_size, (zone.macro_x + 1) * cell_size - 1);
            std::uniform_int_distribution<> zY(zone.macro_y * cell_size, (zone.macro_y + 1) * cell_size - 1);
            record.work_x = zX(gen);
            record.work_y = zY(gen);
        }

        chunk.stored_agents.push_back(record);
    }
}

void AgentSpawnSystem::spawnAgents(int count, int layer) {
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.begin() == config_view.end()) return;
    
    auto config_entity = *config_view.begin();
    const auto& config = config_view.get<WorldConfigComponent>(config_entity);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(0, config.width - 1);
    std::uniform_int_distribution<> disY(0, config.height - 1);
    
    // Weighted selection: Civilians are common, Guards are rarer
    std::uniform_int_distribution<> disType(0, 10); // 0-8 Civilian, 9-10 Guard

    int spawned = 0;
    int attempts = 0;
    const int max_attempts = count * 20;

    while (spawned < count && attempts < max_attempts) {
        attempts++;
        int x = disX(gen);
        int y = disY(gen);

        if (isWalkable(x, y, layer)) {
            int type = disType(gen);
            SpeciesType species = (disType(gen) < 1) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN; 

            if (type < 4) {
                createAgent(x, y, layer, "Citizen #" + std::to_string(spawned), "Civilian", 'o', "#AAAAAA", "GOVERNMENT", (disType(gen) < 2) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
            } else if (type < 6) {
                createAgent(x, y, layer, "Drifter #" + std::to_string(spawned), "Civilian", 'd', "#AA5555", "REBEL", (disType(gen) < 1) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
            } else if (type < 8) {
                createAgent(x, y, layer, "Inherent #" + std::to_string(spawned), "Civilian", 'i', "#55FF55", "MAW", (disType(gen) < 5) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
            } else if (type < 10) {
                createAgent(x, y, layer, "Acolyte #" + std::to_string(spawned), "Civilian", 'a', "#AA55FF", "VOID", (disType(gen) < 1) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
            } else {
                createAgent(x, y, layer, "Peacekeeper #" + std::to_string(spawned), "Guard", 'G', "#5555FF", "GOVERNMENT", (disType(gen) < 3) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
            }
            spawned++;
        }
    }
}

entt::entity AgentSpawnSystem::createAgent(int x, int y, int layer, const std::string& name, const std::string& archetype, char glyph, const std::string& color, const std::string& faction_id, SpeciesType species) {
    auto config_view = m_registry.view<WorldConfigComponent>();
    int world_w = 200, world_h = 200;
    if (config_view.begin() != config_view.end()) {
        const auto& config = config_view.get<WorldConfigComponent>(*config_view.begin());
        world_w = config.width;
        world_h = config.height;
    }

    auto entity = m_registry.create();
    
    m_registry.emplace<NameComponent>(entity, name);
    m_registry.emplace<PositionComponent>(entity, x, y, layer);
    m_registry.emplace<RenderableComponent>(entity, glyph, color, layer);
    m_registry.emplace<AgentComponent>(entity);
    m_registry.emplace<NPCComponent>(entity, 100, 0); // Health 100, no macro id for now
    m_registry.emplace<AgentTaskComponent>(entity, AgentTaskType::IDLE);
    auto& needs = m_registry.emplace<NeedsComponent>(entity, 100.0f, 100.0f);
    needs.frustration = 0.0f;
    m_registry.emplace<SizeComponent>(entity, 1, 1);
    m_registry.emplace<ScheduleComponent>(entity);

    // Give them all layers so they are inspectable (Phase 1.5 prep)
    auto& phys = m_registry.emplace<Layer0PhysicsComponent>(entity);
    phys.material = (species == SpeciesType::SYNTHETIC) ? MaterialType::STEEL : MaterialType::FLESH;
    phys.temperature_celsius = (species == SpeciesType::SYNTHETIC) ? 20.0f : 37.0f;

    auto& bio = m_registry.emplace<Layer1BiologyComponent>(entity);
    bio.species = species;

    if (species == SpeciesType::CACOGEN || species == SpeciesType::HIERODULE) {
        auto& xeno = m_registry.emplace<XenoComponent>(entity);
        xeno.type = (species == SpeciesType::HIERODULE) ? XenoType::HIERODULE : XenoType::CACOGEN;
        xeno.origin = "Deep Space / Orbital";
        
        auto& influence = m_registry.emplace<XenoInfluenceComponent>(entity);
        if (xeno.type == XenoType::HIERODULE) {
            influence.radius = 15;
            influence.frustration_delta = -1.0f;
        } else {
            influence.radius = 10;
            influence.frustration_delta = 0.5f;
        }
    }

    m_registry.emplace<Layer2CognitiveComponent>(entity);
    
    // [NEW] Social Hierarchy assignment
    auto& hierarchy = m_registry.emplace<SocialHierarchyComponent>(entity);
    hierarchy.is_autonomous = (species == SpeciesType::SYNTHETIC) ? (rand() % 10 > 1) : true;
    
    if (faction_id == "GOVERNMENT") {
        hierarchy.status = 0.7f;
        hierarchy.class_title = (archetype == "Guard") ? "Enforcer" : "Administrator";
    } else if (faction_id == "MAW") {
        hierarchy.status = (species == SpeciesType::SYNTHETIC) ? 0.9f : 0.1f;
        hierarchy.class_title = (species == SpeciesType::SYNTHETIC) ? "Acolyte" : "Substrate";
    } else if (faction_id == "SYNDICATE") {
        hierarchy.status = 0.6f;
        hierarchy.class_title = "Member";
    } else if (faction_id == "REBEL") {
        hierarchy.status = 0.2f;
        hierarchy.class_title = "Outlaw";
    } else {
        hierarchy.status = 0.4f;
        hierarchy.class_title = "Citizen";
    }
    
    // Global species baseline adjust
    if (species == SpeciesType::SYNTHETIC && faction_id != "MAW") {
        hierarchy.status -= 0.15f;
    }
    
    if (!hierarchy.is_autonomous) {
        hierarchy.status = 0.05f;
        hierarchy.class_title = "Automaton";
    }

    auto& econ = m_registry.emplace<Layer3EconomicComponent>(entity);
    econ.cash_on_hand = 100 + (rand() % 400); // Initial capital for market participation

    auto& pol = m_registry.emplace<Layer4PoliticalComponent>(entity);
    pol.primary_faction = faction_id;
    pol.faction_loyalty = 0.5f + (float)(rand() % 50) / 100.0f;

    m_registry.emplace<VisibilityComponent>(entity, 12);

    if (archetype == "Guard") {
        m_registry.emplace<FactionComponent>(entity, "GOVERNMENT", 50, 1.0f);
        
        // Add Patrol waypoints for Guards (Phase 1.3)
        PatrolComponent patrol;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disRel(-5, 5);
        
        // Waypoint 1: near spawn
        patrol.waypoints.push_back({x, y, layer});
        
        // Waypoint 2: random offset
        int wx2 = std::clamp(x + disRel(gen), 0, world_w - 1);
        int wy2 = std::clamp(y + disRel(gen), 0, world_h - 1);
        patrol.waypoints.push_back({wx2, wy2, layer});
        
        m_registry.emplace<PatrolComponent>(entity, patrol);
    } else {
        m_registry.emplace<FactionComponent>(entity, faction_id, 10, 0.1f);
    }

    return entity;
}

bool AgentSpawnSystem::isWalkable(int x, int y, int layer) {
    // Check for terrain type
    bool is_terrain_walkable = false;
    auto terrain_view = m_registry.view<PositionComponent, TerrainComponent>();
    for (auto e : terrain_view) {
        const auto& pos = terrain_view.get<PositionComponent>(e);
        if (pos.x == x && pos.y == y && pos.layer_id == layer) {
            const auto& terrain = terrain_view.get<TerrainComponent>(e);
            // Walkable terrain types:
            if (terrain.type == TerrainType::SIDEWALK || terrain.type == TerrainType::STREET || 
                terrain.type == TerrainType::CONCRETE_FLOOR || terrain.type == TerrainType::GRASS) {
                is_terrain_walkable = true;
                break;
            }
        }
    }

    if (!is_terrain_walkable) return false;

    // Check for single-tile obstacles
    auto obs_view = m_registry.view<PositionComponent, ObstacleComponent>();
    for (auto obstacle : obs_view) {
        const auto& o_pos = obs_view.get<PositionComponent>(obstacle);
        if (o_pos.x == x && o_pos.y == y && o_pos.layer_id == layer) {
            return false;
        }
    }

    // Check for entities with SizeComponent and ObstacleComponent (Buildings, Vehicles)
    auto volumetric_obs = m_registry.view<PositionComponent, ObstacleComponent, SizeComponent>();
    for (auto obstacle : volumetric_obs) {
        const auto& o_pos = volumetric_obs.get<PositionComponent>(obstacle);
        const auto& o_size = volumetric_obs.get<SizeComponent>(obstacle);
        if (layer == o_pos.layer_id &&
            x >= o_pos.x && x < o_pos.x + o_size.width &&
            y >= o_pos.y && y < o_pos.y + o_size.height) {
            return false;
        }
    }

    return true;
}

} // namespace NeonOubliette
