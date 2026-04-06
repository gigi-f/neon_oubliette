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
    }

    TimeOfDay current_time = TimeOfDay::DAY;
    auto weather_view = m_registry.view<WeatherComponent>();
    if (!weather_view.empty()) current_time = m_registry.get<WeatherComponent>(*weather_view.begin()).time_of_day;

    auto chunk_view = m_registry.view<ChunkComponent>();
    for (auto entity : chunk_view) {
        auto& chunk = chunk_view.get<ChunkComponent>(entity);
        if (!chunk.is_hot) simulate_macro_agents(chunk, current_time);
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

    for (const auto& record : chunk.stored_agents) {
        auto agent = m_registry.create();
        m_registry.emplace<NameComponent>(agent, record.name);
        m_registry.emplace<PositionComponent>(agent, record.x, record.y, record.layer_id);
        char glyph = (record.archetype == "Guard") ? 'G' : 'o';
        std::string color = (record.archetype == "Guard") ? "#5555FF" : "#AAAAAA";
        if (record.is_xeno) {
            glyph = (record.species == SpeciesType::HIERODULE) ? 'H' : 'C';
            color = (record.species == SpeciesType::HIERODULE) ? "#55FFFF" : "#FF55FF";
        } else if (record.archetype == "Gladiator") { glyph = 'X'; color = "#FF0000"; }
        m_registry.emplace<RenderableComponent>(agent, glyph, color, record.layer_id);
        m_registry.emplace<AgentComponent>(agent); m_registry.emplace<NPCComponent>(agent, 100, 0);
        m_registry.emplace<AgentTaskComponent>(agent, AgentTaskType::IDLE);
        auto& needs = m_registry.emplace<NeedsComponent>(agent, record.hunger, record.thirst); needs.frustration = record.frustration;
        m_registry.emplace<ScheduleComponent>(agent);
        if (record.home_x != -1) m_registry.emplace<HomeComponent>(agent, record.home_x, record.home_y, record.home_layer);
        if (record.work_x != -1) m_registry.emplace<WorkplaceComponent>(agent, record.work_x, record.work_y, record.work_layer);
        auto& phys = m_registry.emplace<Layer0PhysicsComponent>(agent);
        phys.material = (record.species == SpeciesType::SYNTHETIC) ? MaterialType::STEEL : MaterialType::FLESH;
        phys.temperature_celsius = (record.species == SpeciesType::SYNTHETIC) ? 20.0f : 37.0f;
        auto& bio = m_registry.emplace<Layer1BiologyComponent>(agent); bio.consciousness_level = record.consciousness; bio.species = record.species;
        m_registry.emplace<Layer2CognitiveComponent>(agent);
        auto& hierarchy = m_registry.emplace<SocialHierarchyComponent>(agent); hierarchy.status = record.status; hierarchy.class_title = record.class_title; hierarchy.is_autonomous = record.is_autonomous;
        auto& econ = m_registry.emplace<Layer3EconomicComponent>(agent); econ.cash_on_hand = record.cash_on_hand; econ.portfolio = record.portfolio;
        m_registry.emplace<Layer4PoliticalComponent>(agent).primary_faction = record.faction_id;
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
                auto& needs = m_registry.get<NeedsComponent>(entity); record.hunger = needs.hunger; record.thirst = needs.thirst; record.frustration = needs.frustration;
                if (auto* bio = m_registry.try_get<Layer1BiologyComponent>(entity)) { record.consciousness = bio->consciousness_level; record.species = bio->species; }
                if (auto* hierarchy = m_registry.try_get<SocialHierarchyComponent>(entity)) { record.status = hierarchy->status; record.class_title = hierarchy->class_title; record.is_autonomous = hierarchy->is_autonomous; }
                if (auto* xeno = m_registry.try_get<XenoComponent>(entity)) { record.is_xeno = true; record.xeno_type = xeno->type; record.xeno_origin = xeno->origin; }
                if (auto* econ = m_registry.try_get<Layer3EconomicComponent>(entity)) { record.cash_on_hand = econ->cash_on_hand; record.portfolio = econ->portfolio; }
                if (auto* pol = m_registry.try_get<Layer4PoliticalComponent>(entity)) record.faction_id = pol->primary_faction;
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
    for (auto& agent : chunk.stored_agents) {
        agent.hunger = std::max(0.0f, agent.hunger - 0.05f); agent.thirst = std::max(0.0f, agent.thirst - 0.1f);
        float f_change = has_park ? -0.1f : 0.05f; agent.frustration = std::clamp(agent.frustration + f_change, 0.0f, 100.0f);
        int tx = agent.x, ty = agent.y;
        if (current_time == TimeOfDay::NIGHT || current_time == TimeOfDay::DUSK) { if (agent.home_x != -1) { tx = agent.home_x; ty = agent.home_y; } }
        else if (current_time == TimeOfDay::DAY) { if (agent.work_x != -1) { tx = agent.work_x; ty = agent.work_y; } }
        if (agent.x < tx) agent.x++; else if (agent.x > tx) agent.x--;
        if (agent.y < ty) agent.y++; else if (agent.y > ty) agent.y--;
    }
}

} // namespace NeonOubliette
