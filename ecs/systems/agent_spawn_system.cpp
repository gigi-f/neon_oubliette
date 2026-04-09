#include "agent_spawn_system.h"
#include "../components/simulation_layers.h"
#include "../components/lod_components.h"
#include <random>
#include <iostream>
#include <unordered_set>
#include <cstdlib>

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

    // Weight chunks by zone type for POI clustering
    std::vector<double> weights;
    weights.reserve(chunks.size());
    for (auto chunk_ent : chunks) {
        auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
        double w = 1.0;
        for (auto zone_ent : chunk.macro_zones) {
            if (!m_registry.valid(zone_ent)) continue;
            if (!m_registry.all_of<MacroZoneComponent>(zone_ent)) continue;
            auto& zone = m_registry.get<MacroZoneComponent>(zone_ent);
            switch (zone.type) {
                case ZoneType::URBAN_CORE:  w += 5.0; break;
                case ZoneType::COMMERCIAL:  w += 4.0; break;
                case ZoneType::CORPORATE:   w += 3.0; break;
                case ZoneType::COLOSSEUM:   w += 3.0; break;
                case ZoneType::RESIDENTIAL: w += 2.0; break;
                case ZoneType::INDUSTRIAL:  w += 2.0; break;
                case ZoneType::SLUM:        w += 2.0; break;
                case ZoneType::AIRPORT:     w += 2.0; break;
                default: break;
            }
        }
        weights.push_back(w);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::discrete_distribution<> disChunk(weights.begin(), weights.end());
    std::uniform_int_distribution<> disType(0, 10);
    
    int cell_size = config.macro_cell_size;
    int chunk_size = cell_size * 2;

    for (int i = 0; i < total_count; ++i) {
        auto chunk_ent = chunks[disChunk(gen)];
        auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
        
        MacroAgentRecord record;
        record.macro_id = config.next_macro_id++;
        record.name = "Citizen #" + std::to_string(record.macro_id);
        
        int arch_roll = disType(gen);
        if (arch_roll < 8) record.archetype = "Citizen";
        else if (arch_roll < 9) record.archetype = "Guard";
        else record.archetype = "Mule";
        
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
                record.name = "Cacogen #" + std::to_string(record.macro_id);
                record.archetype = "Xeno";
            } else {
                record.species = SpeciesType::HIERODULE;
                record.is_xeno = true;
                record.xeno_type = XenoType::HIERODULE;
                record.name = "Hierodule #" + std::to_string(record.macro_id);
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

        // [M.3] Sewer Spawning: 5% chance of sewer-exclusive agents if sewer exists in chunk
        bool has_sewer = false;
        for (auto zone_ent : chunk.macro_zones) {
            if (!m_registry.valid(zone_ent)) continue;
            const auto& zone = m_registry.get<MacroZoneComponent>(zone_ent);
            for (auto art_ent : zone.arterial_entities) {
                if (m_registry.all_of<InfrastructureSegmentComponent>(art_ent)) {
                    auto type = m_registry.get<InfrastructureSegmentComponent>(art_ent).type;
                    if (type == ArterialType::SEWER || type == ArterialType::UNDERGROUND_TUNNEL) {
                        has_sewer = true; break;
                    }
                } else if (m_registry.all_of<InfrastructureArterialComponent>(art_ent)) {
                    auto type = m_registry.get<InfrastructureArterialComponent>(art_ent).type;
                    if (type == ArterialType::SEWER || type == ArterialType::UNDERGROUND_TUNNEL) {
                        has_sewer = true; break;
                    }
                }
            }
            if (has_sewer) break;
        }

        if (has_sewer && disType(gen) < 1) { // 10% chance to be a sewer dweller
            record.layer_id = -1;
            int sewer_roll = disType(gen);
            if (sewer_roll < 4) {
                record.archetype = "Sewer Dreg";
                record.faction_id = "CITIZEN";
                record.name = "Dreg #" + std::to_string(record.macro_id);
                record.status = 0.1f;
            } else if (sewer_roll < 7) {
                record.archetype = "Syndicate Thug";
                record.faction_id = "SYNDICATE";
                record.name = "Thug #" + std::to_string(record.macro_id);
                record.is_active_criminal = true;
                record.boldness = 75.0f;
            } else {
                record.species = SpeciesType::CACOGEN;
                record.is_xeno = true;
                record.xeno_type = XenoType::CACOGEN;
                record.name = "Cacogen #" + std::to_string(record.macro_id);
                record.archetype = "Xeno Scavenger";
                record.faction_id = "MAW";
                record.status = 0.05f;
            }
        }
        
        record.hunger = 100.0f;
        record.thirst = 100.0f;
        record.frustration = 0.0f;
        record.socialization = 100.0f;
        record.consciousness = 1.0f;
        record.cash_on_hand = 100;
        record.faction_id = (record.archetype == "Guard") ? "GOVERNMENT" : "CITIZEN";

        auto get_profile = [](const std::string& fid) -> std::string {
            if (fid == "GOVERNMENT") return "CORPORATE";
            if (fid == "REBEL") return "COLLECTIVE";
            if (fid == "VOID") return "HIERODULE";
            if (fid == "MAW") return "CACOGEN";
            if (fid == "SYNDICATE") return "SYNDICATE";
            return "NEUTRAL";
        };

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
        
        record.speech_profile = get_profile(record.faction_id);
        
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
            
            // [I.1] Increased crime probability in Slums and Industrial
            if (zone.type == ZoneType::SLUM || zone.type == ZoneType::INDUSTRIAL) {
                int crime_roll = disType(gen);
                if (crime_roll < 3) { // 30% chance in these zones
                    int type_roll = disType(gen);
                    if (type_roll < 3) record.archetype = "Pickpocket";
                    else if (type_roll < 6) record.archetype = "Mugger";
                    else if (type_roll < 9) record.archetype = "Dealer";
                    else record.archetype = "Fence";
                    
                    record.faction_id = "SYNDICATE";
                    record.is_active_criminal = true;
                    record.boldness = 60.0f + (disType(gen) * 4.0f);
                }
            }

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

        // [NEW] Personality roll for macro agents [F.1]
        int p_roll = disType(gen);
        if (p_roll < 2) record.personality_tags.push_back(PersonalityTag::LACONIC);
        else if (p_roll < 4) record.personality_tags.push_back(PersonalityTag::VERBOSE);
        else if (p_roll < 6) record.personality_tags.push_back(PersonalityTag::PARANOID);
        else if (p_roll < 7) record.personality_tags.push_back(PersonalityTag::AGGRESSIVE);
        else if (p_roll < 8) record.personality_tags.push_back(PersonalityTag::SUBSERVIENT);
        else record.personality_tags.push_back(PersonalityTag::NEUTRAL);

        chunk.stored_agents.push_back(record);
    }

    // [NEW] G.2 Family Tree Logic
    // Group agents into families within each chunk
    for (auto chunk_ent : chunks) {
        auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
        if (chunk.stored_agents.size() < 10) continue;

        // Family formation (approx 20% of population)
        int num_families = (int)chunk.stored_agents.size() / 10;
        std::uniform_int_distribution<> disAgent(0, (int)chunk.stored_agents.size() - 1);
        std::uniform_int_distribution<> disChildren(0, 3);

        for (int f = 0; f < num_families; ++f) {
            int p1_idx = disAgent(gen);
            int p2_idx = disAgent(gen);
            if (p1_idx == p2_idx) continue;
            
            auto& p1 = chunk.stored_agents[p1_idx];
            auto& p2 = chunk.stored_agents[p2_idx];

            if (p1.is_xeno || p2.is_xeno) continue; // Skip xenos for simple family trees
            if (p1.species != p2.species) continue; // Keep species consistent

            // Make them family
            p1.relationships.push_back({p2.name, p2.macro_id, RelationshipTier::FAMILY, 100.0f, true});
            p2.relationships.push_back({p1.name, p1.macro_id, RelationshipTier::FAMILY, 100.0f, true});

            // Shared home
            p2.home_x = p1.home_x; p2.home_y = p1.home_y;

            // Children
            int children = disChildren(gen);
            for (int c = 0; c < children; ++c) {
                int c_idx = disAgent(gen);
                if (c_idx == p1_idx || c_idx == p2_idx) continue;
                auto& child = chunk.stored_agents[c_idx];
                
                // Link Parents and Child
                child.relationships.push_back({p1.name, p1.macro_id, RelationshipTier::FAMILY, 100.0f, true});
                child.relationships.push_back({p2.name, p2.macro_id, RelationshipTier::FAMILY, 100.0f, true});
                p1.relationships.push_back({child.name, child.macro_id, RelationshipTier::FAMILY, 80.0f, true});
                p2.relationships.push_back({child.name, child.macro_id, RelationshipTier::FAMILY, 80.0f, true});

                // Shared home
                child.home_x = p1.home_x; child.home_y = p1.home_y;
            }
        }
    }
}

void AgentSpawnSystem::spawnAgents(int count, int layer) {
    // Build the candidate list from actual terrain that exists right now.
    // This avoids the needle-in-a-haystack problem of picking random coords
    // across the full world when only the hot chunks have generated terrain.
    std::vector<std::pair<int,int>> walkable;
    {
        // Build a set of obstacle positions for fast lookup
        std::unordered_set<uint64_t> obstacle_set;
        auto obs_view = m_registry.view<PositionComponent, ObstacleComponent>();
        for (auto e : obs_view) {
            const auto& p = obs_view.get<PositionComponent>(e);
            if (p.layer_id == layer) {
                uint64_t key = (static_cast<uint64_t>(p.x) << 32) | static_cast<uint32_t>(p.y);
                obstacle_set.insert(key);
            }
        }

        auto config_view = m_registry.view<WorldConfigComponent>();
        int macro_size = 20;
        if (config_view.begin() != config_view.end()) macro_size = config_view.get<WorldConfigComponent>(*config_view.begin()).macro_cell_size;

        // Map positions to zone types for density weighting
        std::map<std::pair<int, int>, ZoneType> zone_map;
        auto zone_view = m_registry.view<MacroZoneComponent>();
        for (auto e : zone_view) {
            const auto& z = zone_view.get<MacroZoneComponent>(e);
            zone_map[{z.macro_x, z.macro_y}] = z.type;
        }

        auto terrain_view = m_registry.view<PositionComponent, TerrainComponent>();
        for (auto e : terrain_view) {
            const auto& pos = terrain_view.get<PositionComponent>(e);
            if (pos.layer_id != layer) continue;
            const auto& terrain = terrain_view.get<TerrainComponent>(e);
            if (terrain.type != TerrainType::SIDEWALK && terrain.type != TerrainType::STREET &&
                terrain.type != TerrainType::CONCRETE_FLOOR && terrain.type != TerrainType::GRASS &&
                terrain.type != TerrainType::SEWER_FLOOR) continue;
            uint64_t key = (static_cast<uint64_t>(pos.x) << 32) | static_cast<uint32_t>(pos.y);
            if (obstacle_set.count(key) == 0) {
                walkable.emplace_back(pos.x, pos.y);
                
                // Extra weight for high-activity zones (POI clustering)
                int mx = pos.x / macro_size;
                int my = pos.y / macro_size;
                auto zt = zone_map[{mx, my}];
                int extra = 0;
                if (zt == ZoneType::URBAN_CORE) extra = 5;
                else if (zt == ZoneType::COMMERCIAL) extra = 4;
                else if (zt == ZoneType::CORPORATE || zt == ZoneType::COLOSSEUM) extra = 3;
                else if (zt == ZoneType::RESIDENTIAL || zt == ZoneType::INDUSTRIAL || zt == ZoneType::SLUM) extra = 1;
                for (int w = 0; w < extra; ++w) walkable.emplace_back(pos.x, pos.y);
            }
        }
    }

    if (walkable.empty()) {
        std::cerr << "[AgentSpawnSystem] spawnAgents: no walkable terrain on layer " << layer
                  << " — agents not spawned. Run after terrain generation." << std::endl;
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(walkable.begin(), walkable.end(), gen);

    // Weighted selection: Civilians are common, Guards are rarer
    std::uniform_int_distribution<> disType(0, 10);

    int spawned = 0;
    int idx = 0;
    const int available = static_cast<int>(walkable.size());

    while (spawned < count && idx < available) {
        auto [x, y] = walkable[idx++];
        {
            int type = disType(gen);

            if (layer == -1) {
                // [M.3] Sewer Specific Archetypes
                if (type < 4) {
                    createAgent(x, y, layer, "Sewer Dreg #" + std::to_string(spawned), "Civilian", 'd', "#555555", "REBEL", SpeciesType::HUMAN);
                } else if (type < 7) {
                    createAgent(x, y, layer, "Syndicate Thug #" + std::to_string(spawned), "Thug", 'T', "#AA0000", "SYNDICATE", (disType(gen) < 3) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
                } else {
                    createAgent(x, y, layer, "Cacogen #" + std::to_string(spawned), "Xeno", 'x', "#00AA00", "MAW", SpeciesType::CACOGEN);
                }
            } else {
                if (type < 4) {
                    createAgent(x, y, layer, "Citizen #" + std::to_string(spawned), "Civilian", 'o', "#AAAAAA", "GOVERNMENT", (disType(gen) < 2) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
                } else if (type < 6) {
                    createAgent(x, y, layer, "Drifter #" + std::to_string(spawned), "Civilian", 'd', "#AA5555", "REBEL", (disType(gen) < 1) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
                } else if (type < 7) {
                    // [I.1] Criminal Archetypes in Hot Spawn
                    int type_roll = disType(gen);
                    std::string c_arch = "Pickpocket";
                    if (type_roll < 3) c_arch = "Pickpocket";
                    else if (type_roll < 6) c_arch = "Mugger";
                    else if (type_roll < 9) c_arch = "Dealer";
                    else c_arch = "Fence";
                    createAgent(x, y, layer, c_arch + " #" + std::to_string(spawned), c_arch, 'k', "#FF5555", "SYNDICATE", (disType(gen) < 2) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
                } else if (type < 8) {
                    createAgent(x, y, layer, "Inherent #" + std::to_string(spawned), "Civilian", 'i', "#55FF55", "MAW", (disType(gen) < 5) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
                } else if (type < 10) {
                    createAgent(x, y, layer, "Acolyte #" + std::to_string(spawned), "Civilian", 'a', "#AA55FF", "VOID", (disType(gen) < 1) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
                } else {
                    createAgent(x, y, layer, "Peacekeeper #" + std::to_string(spawned), "Guard", 'G', "#5555FF", "GOVERNMENT", (disType(gen) < 3) ? SpeciesType::SYNTHETIC : SpeciesType::HUMAN);
                }
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
    
    uint64_t m_id = 0;
    auto config_view_macro = m_registry.view<WorldConfigComponent>();
    if (config_view_macro.begin() != config_view_macro.end()) {
        auto& config = config_view_macro.get<WorldConfigComponent>(*config_view_macro.begin());
        m_id = config.next_macro_id++;
    }
    m_registry.emplace<NPCComponent>(entity, 100, m_id);
    m_registry.emplace<AgentTaskComponent>(entity, AgentTaskType::IDLE);
    auto& needs = m_registry.emplace<NeedsComponent>(entity, 100.0f, 100.0f);
    needs.frustration = 0.0f;
    needs.socialization = 100.0f;
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
    
    // [NEW] Personality assignment [F.1]
    auto& personality = m_registry.emplace<PersonalityComponent>(entity);
    int p_roll = rand() % 10;
    if (p_roll < 2) personality.tags.push_back(PersonalityTag::LACONIC);
    else if (p_roll < 4) personality.tags.push_back(PersonalityTag::VERBOSE);
    else if (p_roll < 6) personality.tags.push_back(PersonalityTag::PARANOID);
    else if (p_roll < 7) personality.tags.push_back(PersonalityTag::AGGRESSIVE);
    else if (p_roll < 8) personality.tags.push_back(PersonalityTag::SUBSERVIENT);
    else personality.tags.push_back(PersonalityTag::NEUTRAL);

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

    // [I.1] Crime Behavior Archetypes
    if (archetype == "Pickpocket" || archetype == "Mugger" || archetype == "Dealer" || archetype == "Fence") {
        auto& crime = m_registry.emplace<CrimeRiskComponent>(entity);
        crime.is_active_criminal = true;
        crime.boldness = 60.0f + (float)(rand() % 40);
        
        // Ensure they have a criminal faction
        pol.primary_faction = "SYNDICATE";
    }

    auto get_profile = [](const std::string& fid) -> std::string {
        if (fid == "GOVERNMENT") return "CORPORATE";
        if (fid == "REBEL") return "COLLECTIVE";
        if (fid == "VOID") return "HIERODULE";
        if (fid == "MAW") return "CACOGEN";
        if (fid == "SYNDICATE") return "SYNDICATE";
        return "NEUTRAL";
    };
    std::string profile = get_profile(faction_id);
    m_registry.emplace<SpeechProfileComponent>(entity, profile);

    m_registry.emplace<VisibilityComponent>(entity, 12);
    m_registry.emplace<ConversationComponent>(entity);

    // [J.1] Age Component assignment
    auto& age = m_registry.emplace<AgeComponent>(entity);
    age.expected_lifespan_years = (species == SpeciesType::SYNTHETIC) ? 200 : 120;
    
    int age_roll = rand() % 60 + 18; // Default adult range
    if (archetype == "Guard") age_roll = rand() % 25 + 22;
    else if (species == SpeciesType::HIERODULE) {
        age_roll = rand() % 400 + 100;
        age.expected_lifespan_years = 1000;
    } else if (species == SpeciesType::CACOGEN) {
        age_roll = rand() % 15 + 2;
        age.expected_lifespan_years = 30;
    }
    
    age.years = age_roll;
    if (age.years < 4) age.stage = LifeStage::INFANT;
    else if (age.years < 13) age.stage = LifeStage::CHILD;
    else if (age.years < 25) age.stage = LifeStage::YOUNG_ADULT;
    else if (age.years < 65) age.stage = LifeStage::ADULT;
    else if (age.years < 100) age.stage = LifeStage::ELDER;
    else age.stage = LifeStage::ANCIENT;
    
    if (species == SpeciesType::SYNTHETIC || species == SpeciesType::HIERODULE) {
        if (rand() % 10 == 0) age.stage = LifeStage::AGELESS;
    }
    
    age.biological_wear = (float)age.years / (float)age.expected_lifespan_years;

    if (archetype == "Guard") {
        m_registry.emplace<FactionComponent>(entity, "GOVERNMENT", "CORPORATE", 50, 1.0f);
        
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
    } else if (archetype == "Mule") {
        m_registry.emplace<MuleComponent>(entity);
        m_registry.emplace<FactionComponent>(entity, faction_id, profile, 10, 0.1f);
        hierarchy.status = 0.3f;
        hierarchy.class_title = "Logistician";
    } else {
        m_registry.emplace<FactionComponent>(entity, faction_id, profile, 10, 0.1f);
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
