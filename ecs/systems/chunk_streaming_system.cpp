#include "chunk_streaming_system.h"
#include "city_generation_system.h"
#include "stock_market_system.h"
#include "../components/lod_components.h"
#include "../simulation_coordinator.h"
#include <algorithm>
#include <iostream>
#include <map>

namespace NeonOubliette {

ChunkStreamingSystem::ChunkStreamingSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {}

void ChunkStreamingSystem::initialize() {
    auto view = m_registry.view<ChunkComponent>();
    if (view.begin() != view.end()) return;

    auto zone_view = m_registry.view<MacroZoneComponent>();
    std::map<std::pair<int, int>, std::vector<entt::entity>> chunk_groups;
    for (auto entity : zone_view) {
        const auto& zone = zone_view.get<MacroZoneComponent>(entity);
        chunk_groups[{zone.macro_x / 2, zone.macro_y / 2}].push_back(entity);
    }

    for (auto const& [coords, zones] : chunk_groups) {
        auto chunk_entity = m_registry.create();
        auto& chunk = m_registry.emplace<ChunkComponent>(chunk_entity, coords.first, coords.second, false, false);
        chunk.macro_zones = zones;
        m_registry.emplace<MarketDemandComponent>(chunk_entity);
        m_registry.emplace<FactionInfluenceFieldComponent>(chunk_entity);
        m_registry.emplace<ReligionInfluenceFieldComponent>(chunk_entity);
        m_chunk_map[coords] = chunk_entity;
    }
}

void ChunkStreamingSystem::update(double delta_time) {
    (void)delta_time;
    auto player_view = m_registry.view<PlayerComponent, PositionComponent>();
    if (player_view.begin() == player_view.end()) return;
    auto player_entity = *player_view.begin();
    const auto& pos = player_view.get<PositionComponent>(player_entity);
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.begin() == config_view.end()) return;
    auto& config = config_view.get<WorldConfigComponent>(*config_view.begin());
    int cell_size = config.macro_cell_size;

    int ref_x = pos.x, ref_y = pos.y;
    if (pos.layer_id > 0) {
        auto building_view = m_registry.view<BuildingComponent, PositionComponent>();
        for (auto b_ent : building_view) {
            const auto& b_comp = building_view.get<BuildingComponent>(b_ent);
            int base_layer = 1000 + b_comp.building_id * 10;
            if (pos.layer_id >= base_layer && pos.layer_id < base_layer + 10) {
                const auto& b_pos = building_view.get<PositionComponent>(b_ent);
                ref_x = b_pos.x; ref_y = b_pos.y; break;
            }
        }
    }

    int chunk_size = cell_size * 2, p_chunk_x = ref_x / chunk_size, p_chunk_y = ref_y / chunk_size;
    if (p_chunk_x != m_last_player_chunk_x || p_chunk_y != m_last_player_chunk_y) {
        update_chunk_states(p_chunk_x, p_chunk_y);
        m_last_player_chunk_x = p_chunk_x; m_last_player_chunk_y = p_chunk_y;
        m_dispatcher.trigger(ChunkChangedEvent{});
    }

    TimeOfDay current_time = TimeOfDay::DAY;
    auto weather_view = m_registry.view<WeatherComponent>();
    if (!weather_view.empty()) current_time = m_registry.get<WeatherComponent>(*weather_view.begin()).time_of_day;

    // Only simulate cold-chunk agents every 10 frames to reduce per-frame cost
    m_macro_sim_counter++;
    if (m_macro_sim_counter >= 10) {
        m_macro_sim_counter = 0;
        auto chunk_view = m_registry.view<ChunkComponent>();
        for (auto entity : chunk_view) {
            auto& chunk = chunk_view.get<ChunkComponent>(entity);
            if (!chunk.is_hot) simulate_macro_agents(chunk, current_time);
        }
    }
}

void ChunkStreamingSystem::update_chunk_states(int player_cx, int player_cy) {
    std::set<entt::entity> next_hot_chunks, next_warm_chunks;
    for (int dx = -WARM_RADIUS; dx <= WARM_RADIUS; ++dx) {
        for (int dy = -WARM_RADIUS; dy <= WARM_RADIUS; ++dy) {
            int cx = player_cx + dx, cy = player_cy + dy;
            auto it = m_chunk_map.find({cx, cy});
            if (it != m_chunk_map.end()) {
                int dist = std::max(std::abs(dx), std::abs(dy));
                if (dist <= HOT_RADIUS) next_hot_chunks.insert(it->second); else next_warm_chunks.insert(it->second);
            }
        }
    }
    auto chunk_view = m_registry.view<ChunkComponent>();
    for (auto entity : m_hot_chunks) if (next_hot_chunks.find(entity) == next_hot_chunks.end()) {
        auto& chunk = chunk_view.get<ChunkComponent>(entity); dematerialize_chunk(entity, chunk); chunk.is_hot = false;
    }
    for (auto entity : next_hot_chunks) if (m_hot_chunks.find(entity) == m_hot_chunks.end()) {
        auto& chunk = chunk_view.get<ChunkComponent>(entity); materialize_chunk(entity, chunk); chunk.is_hot = true;
    }
    for (auto entity : next_warm_chunks) chunk_view.get<ChunkComponent>(entity).is_warm = true;
    m_hot_chunks = next_hot_chunks;
}

void ChunkStreamingSystem::materialize_chunk(entt::entity chunk_entity, ChunkComponent& chunk) {
    (void)chunk_entity;
    CityGenerationSystem gen(m_registry, m_dispatcher);
    for (auto zone_entity : chunk.macro_zones) gen.generate_chunk_content(zone_entity);

    auto mapping_view = m_registry.view<MacroIdMappingTag>();
    if (mapping_view.begin() == mapping_view.end()) m_registry.emplace<MacroIdMappingTag>(m_registry.create());
    auto& mapping = m_registry.get<MacroIdMappingTag>(*m_registry.view<MacroIdMappingTag>().begin()).mapping;

    for (const auto& record : chunk.stored_agents) {
        auto agent = m_registry.create();
        mapping[record.macro_id] = agent;

        m_registry.emplace<NameComponent>(agent, record.name);
        m_registry.emplace<PositionComponent>(agent, record.x, record.y, record.layer_id);
        char glyph = (record.archetype == "Guard") ? 'G' : 'o';
        std::string color = (record.archetype == "Guard") ? "#5555FF" : "#AAAAAA";
        if (record.is_xeno) {
            glyph = (record.species == SpeciesType::HIERODULE) ? 'H' : 'C';
            color = (record.species == SpeciesType::HIERODULE) ? "#55FFFF" : "#FF55FF";
        } else if (record.archetype == "Gladiator") { glyph = 'X'; color = "#FF0000"; }
        m_registry.emplace<RenderableComponent>(agent, glyph, color, record.layer_id);
        m_registry.emplace<AgentComponent>(agent); 
        m_registry.emplace<NPCComponent>(agent, 100, record.macro_id);
        m_registry.emplace<ConversationComponent>(agent);
        m_registry.emplace<AgentTaskComponent>(agent, AgentTaskType::IDLE);
        auto& needs = m_registry.emplace<NeedsComponent>(agent, record.hunger, record.thirst); 
        needs.frustration = record.frustration;
        needs.socialization = record.socialization;
        m_registry.emplace<ScheduleComponent>(agent);
        if (record.home_x != -1) m_registry.emplace<HomeComponent>(agent, record.home_x, record.home_y, record.home_layer);
        if (record.work_x != -1) m_registry.emplace<WorkplaceComponent>(agent, record.work_x, record.work_y, record.work_layer);
        auto& phys = m_registry.emplace<Layer0PhysicsComponent>(agent);
        phys.material = (record.species == SpeciesType::SYNTHETIC) ? MaterialType::STEEL : MaterialType::FLESH;
        phys.temperature_celsius = (record.species == SpeciesType::SYNTHETIC) ? 20.0f : 37.0f;
        auto& bio = m_registry.emplace<Layer1BiologyComponent>(agent); bio.consciousness_level = record.consciousness; bio.species = record.species;

        // [J.1] Restore Age & Life Stage
        auto& age = m_registry.emplace<AgeComponent>(agent);
        age.years = record.age_years;
        age.current_age_ticks = record.age_ticks;
        age.stage = record.life_stage;
        age.biological_wear = record.biological_wear;
        age.expected_lifespan_years = (record.species == SpeciesType::SYNTHETIC) ? 200 : 120;
        m_registry.emplace<Layer2CognitiveComponent>(agent);
        auto& hierarchy = m_registry.emplace<SocialHierarchyComponent>(agent); hierarchy.status = record.status; hierarchy.class_title = record.class_title; hierarchy.is_autonomous = record.is_autonomous;
        auto& econ = m_registry.emplace<Layer3EconomicComponent>(agent); econ.cash_on_hand = record.cash_on_hand; econ.portfolio = record.portfolio;
        m_registry.emplace<Layer4PoliticalComponent>(agent).primary_faction = record.faction_id;
        
        // [NEW] Restore speech profile [F.3]
        m_registry.emplace<SpeechProfileComponent>(agent, record.speech_profile);

        // [NEW] Restore personality [F.1]
        auto& personality = m_registry.emplace<PersonalityComponent>(agent);
        personality.tags = record.personality_tags;

        // [NEW] Restore information records [F.8]
        if (!record.records.empty()) {
            auto& info = m_registry.emplace<InformationComponent>(agent);
            info.records = record.records;
        }

        // [NEW] Restore relationships (G.1)
        if (!record.relationships.empty()) {
            auto& rel_comp = m_registry.emplace<RelationshipComponent>(agent);
            for (const auto& r : record.relationships) {
                RelationshipRecord live_r;
                live_r.tier = r.tier;
                live_r.affinity = r.affinity;
                live_r.shared_home = r.shared_home;
                live_r.target_macro_id = r.target_macro_id;
                rel_comp.records[r.target_macro_id] = live_r;
            }
        }

        if (record.is_xeno) {
            auto& xeno = m_registry.emplace<XenoComponent>(agent); xeno.type = record.xeno_type; xeno.origin = record.xeno_origin;
            auto& influence = m_registry.emplace<XenoInfluenceComponent>(agent);
            if (record.xeno_type == XenoType::HIERODULE) { influence.radius = 12; influence.frustration_delta = -0.5f; influence.temperature_offset = -5.0f; }
            else { influence.radius = 8; influence.frustration_delta = 0.2f; }
        }
        if (record.archetype == "Guard") { PatrolComponent p; p.waypoints.push_back({record.x, record.y, record.layer_id}); m_registry.emplace<PatrolComponent>(agent, p); }
    }
    chunk.stored_agents.clear();
}

void ChunkStreamingSystem::dematerialize_chunk(entt::entity chunk_entity, ChunkComponent& chunk) {
    (void)chunk_entity; auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.begin() == config_view.end()) return;
    int cell_size = config_view.get<WorldConfigComponent>(*config_view.begin()).macro_cell_size;
    int chunk_size = cell_size * 2, min_x = chunk.chunk_x * chunk_size, max_x = min_x + chunk_size, min_y = chunk.chunk_y * chunk_size, max_y = min_y + chunk_size;

    auto pos_view = m_registry.view<PositionComponent>(); std::vector<entt::entity> to_destroy;
    std::vector<std::pair<int, int>> interior_ranges;
    std::map<int, std::pair<int, int>> layer_to_building_coords;
    
    auto building_view = m_registry.view<BuildingComponent, PositionComponent>();
    for (auto b_ent : building_view) {
        const auto& b_pos = building_view.get<PositionComponent>(b_ent);
        if (b_pos.x >= min_x && b_pos.x < max_x && b_pos.y >= min_y && b_pos.y < max_y && b_pos.layer_id == 0) {
            const auto& b_comp = building_view.get<BuildingComponent>(b_ent);
            int base_layer = 1000 + b_comp.building_id * 10;
            interior_ranges.push_back({base_layer, base_layer + 9});
            for(int i=0; i<10; ++i) layer_to_building_coords[base_layer + i] = {b_pos.x, b_pos.y};
            
            if (auto* interior = m_registry.try_get<BuildingInteriorComponent>(b_ent)) {
                interior->floor_entities.clear();
                chunk.building_interiors[{b_pos.x, b_pos.y}] = *interior;
                chunk.building_interiors[{b_pos.x, b_pos.y}].stored_objects.clear(); // Will refill
            }
        }
    }

    for (auto entity : pos_view) {
        if (m_registry.all_of<PersistentEntityComponent>(entity) || m_registry.all_of<PlayerComponent>(entity)) continue;
        if (m_registry.all_of<InfrastructureArterialComponent>(entity) || m_registry.all_of<InfrastructureNodeComponent>(entity)) continue;
        if (m_registry.all_of<ChunkComponent>(entity) || m_registry.all_of<MacroZoneComponent>(entity)) continue;

        const auto& pos = pos_view.get<PositionComponent>(entity);
        bool in_chunk_overworld = (pos.x >= min_x && pos.x < max_x && pos.y >= min_y && pos.y < max_y && pos.layer_id == 0);
        bool in_chunk_interior = false;
        for (const auto& range : interior_ranges) { if (pos.layer_id >= range.first && pos.layer_id <= range.second) { in_chunk_interior = true; break; } }

        if (in_chunk_overworld || in_chunk_interior) {
            if (m_registry.all_of<AgentComponent>(entity)) {
                MacroAgentRecord record;
                record.name = m_registry.get<NameComponent>(entity).name; record.x = pos.x; record.y = pos.y; record.layer_id = pos.layer_id;
                auto& needs = m_registry.get<NeedsComponent>(entity); 
                record.hunger = needs.hunger; 
                record.thirst = needs.thirst; 
                record.frustration = needs.frustration;
                record.socialization = needs.socialization;
                if (auto* bio = m_registry.try_get<Layer1BiologyComponent>(entity)) { record.consciousness = bio->consciousness_level; record.species = bio->species; }
                if (auto* hierarchy = m_registry.try_get<SocialHierarchyComponent>(entity)) { record.status = hierarchy->status; record.class_title = hierarchy->class_title; record.is_autonomous = hierarchy->is_autonomous; }
                if (auto* xeno = m_registry.try_get<XenoComponent>(entity)) { record.is_xeno = true; record.xeno_type = xeno->type; record.xeno_origin = xeno->origin; }
                if (auto* econ = m_registry.try_get<Layer3EconomicComponent>(entity)) { record.cash_on_hand = econ->cash_on_hand; record.portfolio = econ->portfolio; }
                if (auto* pol = m_registry.try_get<Layer4PoliticalComponent>(entity)) record.faction_id = pol->primary_faction;
                
                // [J.1] Capture Age & Life Stage
                if (auto* age = m_registry.try_get<AgeComponent>(entity)) {
                    record.age_years = age->years;
                    record.age_ticks = age->current_age_ticks;
                    record.life_stage = age->stage;
                    record.biological_wear = age->biological_wear;
                }
                
                // [NEW] Capture speech profile [F.3]
                if (auto* sp = m_registry.try_get<SpeechProfileComponent>(entity)) record.speech_profile = sp->profile_id;

                // [NEW] Capture macro_id and mapping removal
                if (auto* npc = m_registry.try_get<NPCComponent>(entity)) {
                    record.macro_id = npc->macro_id;
                    auto mapping_view = m_registry.view<MacroIdMappingTag>();
                    if (mapping_view.begin() != mapping_view.end()) {
                        m_registry.get<MacroIdMappingTag>(*mapping_view.begin()).mapping.erase(record.macro_id);
                    }
                }

                // [NEW] Capture personality [F.1]
                if (auto* personality = m_registry.try_get<PersonalityComponent>(entity)) record.personality_tags = personality->tags;

                // [NEW] Capture information records [F.8]
                if (auto* info = m_registry.try_get<InformationComponent>(entity)) record.records = info->records;

                // [NEW] Capture relationships (G.1)
                if (auto* rel = m_registry.try_get<RelationshipComponent>(entity)) {
                    for (auto const& [target_id, r] : rel->records) {
                        MacroRelationshipRecord mr;
                        mr.target_name = ""; // Could lookup but redundant with target_macro_id
                        mr.target_macro_id = target_id;
                        mr.tier = r.tier;
                        mr.affinity = r.affinity;
                        mr.shared_home = r.shared_home;
                        record.relationships.push_back(mr);
                    }
                }

                if (auto* home = m_registry.try_get<HomeComponent>(entity)) { record.home_x = home->x; record.home_y = home->y; record.home_layer = home->layer; }
                if (auto* work = m_registry.try_get<WorkplaceComponent>(entity)) { record.work_x = work->x; record.work_y = work->y; record.work_layer = work->layer; }
                if (m_registry.all_of<PatrolComponent>(entity)) record.archetype = "Guard";
                else if (auto* pol_ptr = m_registry.try_get<Layer4PoliticalComponent>(entity)) {
                    if (pol_ptr->primary_faction == "SYNDICATE") record.archetype = "Gladiator"; else record.archetype = "Citizen";
                } else record.archetype = "Citizen";
                chunk.stored_agents.push_back(record);
            } else if (in_chunk_interior && !m_registry.all_of<TerrainComponent>(entity)) {
                MacroObjectRecord obj;
                obj.name = m_registry.all_of<NameComponent>(entity) ? m_registry.get<NameComponent>(entity).name : "Object";
                auto const& render = m_registry.get<RenderableComponent>(entity);
                obj.glyph = render.glyph; obj.color = render.color; obj.x = pos.x; obj.y = pos.y; obj.layer_id = pos.layer_id;
                obj.is_obstacle = m_registry.all_of<ObstacleComponent>(entity);
                if (auto* item = m_registry.try_get<ItemComponent>(entity)) obj.item_type_id = item->item_type_id;
                if (auto* val = m_registry.try_get<ItemValueComponent>(entity)) obj.item_value = val->value;
                if (auto* cons = m_registry.try_get<ConsumableComponent>(entity)) { obj.restores_hunger = cons->restores_hunger; obj.restores_thirst = cons->restores_thirst; }
                auto it = layer_to_building_coords.find(pos.layer_id);
                if (it != layer_to_building_coords.end()) chunk.building_interiors[it->second].stored_objects.push_back(obj);
            }
            to_destroy.push_back(entity);
        }
    }
    std::sort(to_destroy.begin(), to_destroy.end()); to_destroy.erase(std::unique(to_destroy.begin(), to_destroy.end()), to_destroy.end());
    for (auto e : to_destroy) m_registry.destroy(e);
}

void ChunkStreamingSystem::simulate_macro_agents(ChunkComponent& chunk, TimeOfDay current_time) {
    StockMarketSystem::processMacroTrades(m_registry, chunk.stored_agents);
    bool has_park = false;
    for (auto zone_ent : chunk.macro_zones) if (m_registry.valid(zone_ent) && m_registry.get<MacroZoneComponent>(zone_ent).type == ZoneType::PARK) { has_park = true; break; }
    
    // Use stable erase-remove idiom
    for (auto it = chunk.stored_agents.begin(); it != chunk.stored_agents.end(); ) {
        auto& agent = *it;
        agent.hunger = std::max(0.0f, agent.hunger - 0.05f); 
        agent.thirst = std::max(0.0f, agent.thirst - 0.1f);
        agent.socialization = std::max(0.0f, agent.socialization - 0.02f);
        
        float f_change = has_park ? -0.1f : 0.05f; 
        agent.frustration = std::clamp(agent.frustration + f_change, 0.0f, 100.0f);
        
        // [J.1] Macro Aging (Resolution: 10 simulation ticks per macro-sim update)
        agent.age_ticks += 10;
        if (agent.age_ticks >= 100000) {
            agent.age_years++;
            agent.age_ticks = 0;
            // Basic Life Stage update for macro
            if (agent.age_years < 4) agent.life_stage = LifeStage::INFANT;
            else if (agent.age_years < 13) agent.life_stage = LifeStage::CHILD;
            else if (agent.age_years < 25) agent.life_stage = LifeStage::YOUNG_ADULT;
            else if (agent.age_years < 65) agent.life_stage = LifeStage::ADULT;
            else if (agent.age_years < 100) agent.life_stage = LifeStage::ELDER;
            else agent.life_stage = LifeStage::ANCIENT;

            uint32_t lifespan = (agent.species == SpeciesType::SYNTHETIC) ? 200 : 120;
            agent.biological_wear = (float)agent.age_years / (float)lifespan;
        }
        
        // [Death Check]
        if (agent.hunger <= 0.0f || agent.thirst <= 0.0f || agent.consciousness <= 0.0f || agent.biological_wear >= 1.0f) {
            AgentDeathEvent death_ev;
            death_ev.entity = entt::null;
            death_ev.macro_id = agent.macro_id;
            death_ev.credits = agent.cash_on_hand;
            death_ev.faction_id = agent.faction_id;
            death_ev.portfolio = agent.portfolio;
            
            for (auto const& r : agent.relationships) {
                RelationshipRecord rel_rec;
                rel_rec.target_macro_id = r.target_macro_id;
                rel_rec.tier = r.tier;
                rel_rec.affinity = r.affinity;
                rel_rec.shared_home = r.shared_home;
                death_ev.relationships[r.target_macro_id] = rel_rec;
            }

            m_dispatcher.trigger(death_ev);
            it = chunk.stored_agents.erase(it);
            continue;
        }

        // [G.6] Statistical affinity drift based on faction alignment
        for (auto& rel : agent.relationships) {
            float drift = 0.01f; // Base slow drift
            // Find target faction if possible? We don't have a global macro-id to faction map here easily.
            // But we can use the same-faction bonus if we had it. 
            // For now, base drift is positive for family/friends.
            if (rel.tier >= RelationshipTier::FRIEND) {
                rel.affinity = std::min(100.0f, rel.affinity + 0.05f);
                agent.socialization = std::min(100.0f, agent.socialization + 0.1f);
            }
        }

        // [F.8] Statistical rumor veracity decay for macro-agents
        for (auto rit = agent.records.begin(); rit != agent.records.end(); ) {
            rit->veracity -= 0.0001f;
            if (rit->veracity < 0.05f || rit->hops > 15) {
                rit = agent.records.erase(rit);
            } else {
                ++rit;
            }
        }

        int tx = agent.x, ty = agent.y;
        if (current_time == TimeOfDay::NIGHT || current_time == TimeOfDay::DUSK) { if (agent.home_x != -1) { tx = agent.home_x; ty = agent.home_y; } }
        else if (current_time == TimeOfDay::DAY) { if (agent.work_x != -1) { tx = agent.work_x; ty = agent.work_y; } }
        if (agent.x < tx) agent.x++; else if (agent.x > tx) agent.x--;
        if (agent.y < ty) agent.y++; else if (agent.y > ty) agent.y--;
        
        ++it;
    }

    // [Phase F.8] Statistical conversations and affinity drift based on proximity/faction
    if (chunk.stored_agents.size() > 1) {
        int num_attempts = (int)chunk.stored_agents.size() / 4; 
        for (int i = 0; i < num_attempts; ++i) {
            int idx_a = rand() % chunk.stored_agents.size();
            int idx_b = rand() % chunk.stored_agents.size();
            if (idx_a == idx_b) continue;

            auto& a = chunk.stored_agents[idx_a];
            auto& b = chunk.stored_agents[idx_b];

            float dx = (float)(a.x - b.x); float dy = (float)(a.y - b.y);
            float dist_sq = dx*dx + dy*dy;
            
            if (dist_sq < 100.0f) { // Within 10 tiles macro-proximity
                float alignment = 0.0f;
                if (!a.faction_id.empty() && a.faction_id == b.faction_id) alignment = 1.0f;
                else if (!a.faction_id.empty() && !b.faction_id.empty()) {
                    if ((a.faction_id == "GOVERNMENT" && b.faction_id == "REBEL") ||
                        (a.faction_id == "REBEL" && b.faction_id == "GOVERNMENT") ||
                        (a.faction_id == "MAW" && b.faction_id == "VOID") ||
                        (a.faction_id == "VOID" && b.faction_id == "MAW")) {
                        alignment = -2.0f;
                    }
                }

                auto update_fn = [&](MacroAgentRecord& src, const MacroAgentRecord& tgt, float drift) {
                    bool found = false;
                    for (auto& r : src.relationships) {
                        if (r.target_macro_id == tgt.macro_id) {
                            r.affinity = std::clamp(r.affinity + drift, -100.0f, 100.0f);
                            if (r.affinity > 80.0f) r.tier = RelationshipTier::FAMILY;
                            else if (r.affinity > 40.0f) r.tier = RelationshipTier::FRIEND;
                            else if (r.affinity > 10.0f) r.tier = RelationshipTier::ACQUAINTANCE;
                            found = true; break;
                        }
                    }
                    if (!found && drift > 0.0f && (rand() % 50 == 0)) { // Small chance to form acquaintance
                        MacroRelationshipRecord nr; nr.target_macro_id = tgt.macro_id; nr.affinity = drift; nr.tier = RelationshipTier::ACQUAINTANCE;
                        src.relationships.push_back(nr);
                    }
                };
                update_fn(a, b, 0.2f + alignment);
                update_fn(b, a, 0.2f + alignment);
                a.socialization = std::min(100.0f, a.socialization + 2.0f);
                b.socialization = std::min(100.0f, b.socialization + 2.0f);

                // [F.8] Statistical rumor propagation
                auto propagate_macro = [](MacroAgentRecord& src, MacroAgentRecord& tgt) {
                    if (src.records.empty()) return;
                    // Pick a random rumor
                    const auto& rumor_to_share = src.records[rand() % src.records.size()];
                    
                    // Check if target already knows it
                    bool knows_it = false;
                    for (const auto& r : tgt.records) {
                        if (r.content_tag == rumor_to_share.content_tag) {
                            knows_it = true; break;
                        }
                    }

                    if (!knows_it) {
                        InformationRecord new_rumor = rumor_to_share;
                        new_rumor.hops++;
                        new_rumor.veracity *= 0.85f; // Faster decay in macro-cells due to abstraction
                        if (new_rumor.veracity > 0.05f && new_rumor.hops < 15) {
                            tgt.records.push_back(new_rumor);
                        }
                    }
                };

                // Bi-directional propagation chance
                if (rand() % 10 == 0) propagate_macro(a, b);
                if (rand() % 10 == 0) propagate_macro(b, a);
            }
        }
    }
}

} // namespace NeonOubliette
