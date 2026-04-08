#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_POPULATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_POPULATION_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../components/zoning_components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"
#include <random>

namespace NeonOubliette {

/**
 * @brief Manages the migration and persistent presence of citizens across simulation scales.
 */
class PopulationSystem : public ISystem {
public:
    PopulationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}

    void update(double delta_time) override {
        // [J.1] Aging Simulation (Layer 1 - Biological)
        auto aged_view = m_registry.view<AgeComponent>();
        for (auto entity : aged_view) {
            auto& age = aged_view.get<AgeComponent>(entity);
            age.current_age_ticks++;

            const uint32_t TICKS_PER_YEAR = 100000; 

            if (age.current_age_ticks >= TICKS_PER_YEAR) {
                age.years++;
                age.current_age_ticks = 0;

                if (age.years < 4) age.stage = LifeStage::INFANT;
                else if (age.years < 13) age.stage = LifeStage::CHILD;
                else if (age.years < 25) age.stage = LifeStage::YOUNG_ADULT;
                else if (age.years < 65) age.stage = LifeStage::ADULT;
                else if (age.years < 100) age.stage = LifeStage::ELDER;
                else age.stage = LifeStage::ANCIENT;
                
                age.biological_wear = (float)age.years / (float)age.expected_lifespan_years;

                if (age.biological_wear >= 1.0f) {
                    uint64_t m_id = 0;
                    if (auto* npc = m_registry.try_get<NPCComponent>(entity)) m_id = npc->macro_id;
                    m_dispatcher.trigger(AgentDeathEvent{entity, m_id});
                }
            }
        }

        // [J.2] Birth Simulation (Layer 1 - Biological)
        auto repro_view = m_registry.view<ReproductionComponent, PositionComponent>();
        for (auto parent_a : repro_view) {
            auto& repro = repro_view.get<ReproductionComponent>(parent_a);
            if (!repro.is_pregnant) continue;

            if (repro.gestation_ticks_remaining > 0) {
                repro.gestation_ticks_remaining--;
            } else {
                auto const& pos = repro_view.get<PositionComponent>(parent_a);
                triggerBirth(parent_a, repro.partner, pos.x, pos.y, pos.layer_id);
                repro.is_pregnant = false;
                repro.partner = entt::null;
            }
        }

        // [J.4] Demographic Pressure & Migration
        calculateDemographics();
        processMacroMigration();

        // Migration Logic: Macro to Micro
        auto active_buildings = m_registry.view<BuildingInteriorComponent, BuildingComponent>();
        for (auto building : active_buildings) {
            auto const& interior = active_buildings.get<BuildingInteriorComponent>(building);
            if (!interior.is_generated || interior.floor_entities.empty()) continue;

            auto citizens = m_registry.view<CitizenComponent>();
            for (auto citizen : citizens) {
                if (m_registry.all_of<MicroPresenceComponent>(citizen)) {
                    if (m_registry.get<MicroPresenceComponent>(citizen).is_active) continue;
                }

                bool should_instantiate = false;
                int target_layer = -1;

                if (m_registry.all_of<HomeComponent>(citizen)) {
                    if (m_registry.get<HomeComponent>(citizen).building_entity == building) {
                        should_instantiate = true;
                        target_layer = static_cast<int>(entt::to_integral(building)) * 100;
                    }
                }

                if (!should_instantiate && m_registry.all_of<WorkplaceComponent>(citizen)) {
                    if (m_registry.get<WorkplaceComponent>(citizen).building_entity == building) {
                        should_instantiate = true;
                        target_layer = static_cast<int>(entt::to_integral(building)) * 100;
                    }
                }

                if (should_instantiate) {
                    instantiateMicroNPC(citizen, building, target_layer);
                }
            }
        }
    }

private:
    void calculateDemographics() {
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (config_view.empty()) return;
        int macro_cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

        auto zone_view = m_registry.view<MacroZoneComponent, MacroMarketComponent>();
        for (auto zone_ent : zone_view) {
            auto& zone = zone_view.get<MacroZoneComponent>(zone_ent);
            auto& market = zone_view.get<MacroMarketComponent>(zone_ent);

            int citizen_count = 0;
            int min_x = zone.macro_x * macro_cell_size;
            int max_x = (zone.macro_x + 1) * macro_cell_size;
            int min_y = zone.macro_y * macro_cell_size;
            int max_y = (zone.macro_y + 1) * macro_cell_size;

            auto citizens = m_registry.view<CitizenComponent, PositionComponent>();
            for (auto citizen : citizens) {
                const auto& pos = citizens.get<PositionComponent>(citizen);
                if (pos.x >= min_x && pos.x < max_x && pos.y >= min_y && pos.y < max_y) {
                    citizen_count++;
                }
            }

            float nature_impact = 0.0f;
            auto nature_view = m_registry.view<NatureEffectComponent, PositionComponent>();
            for (auto nature_ent : nature_view) {
                const auto& pos = nature_view.get<PositionComponent>(nature_ent);
                if (pos.x >= min_x && pos.x < max_x && pos.y >= min_y && pos.y < max_y) {
                    nature_impact += 1.0f;
                }
            }

            float density_pressure = (float)citizen_count / std::max(1, (int)zone.block_entities.size() * 4);
            zone.pressure = density_pressure * 5.0f + market.crime_rate * 10.0f;

            float job_attractiveness = (1.0f - market.unemployment_rate) * 10.0f;
            float wealth_attractiveness = (market.average_wealth / 100.0f) * 5.0f;
            zone.attractiveness = job_attractiveness + wealth_attractiveness + nature_impact * 2.0f - market.crime_rate * 15.0f;
        }
    }

    void processMacroMigration() {
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (config_view.empty()) return;
        int macro_cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

        auto citizens = m_registry.view<CitizenComponent, PositionComponent, NeedsComponent, Layer3EconomicComponent>();
        auto zone_view = m_registry.view<MacroZoneComponent, MacroMarketComponent>();

        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 99);

        for (auto citizen : citizens) {
            auto& pos = citizens.get<PositionComponent>(citizen);
            auto& needs = citizens.get<NeedsComponent>(citizen);

            if (dis(gen) > 2) continue; // 3% migration chance per turn for active evaluators

            entt::entity current_zone_ent = entt::null;
            int mx = pos.x / macro_cell_size;
            int my = pos.y / macro_cell_size;
            for (auto ze : zone_view) {
                const auto& z = zone_view.get<MacroZoneComponent>(ze);
                if (z.macro_x == mx && z.macro_y == my) {
                    current_zone_ent = ze;
                    break;
                }
            }

            if (current_zone_ent == entt::null) continue;

            const auto& cur_zone = zone_view.get<MacroZoneComponent>(current_zone_ent);
            float current_score = cur_zone.attractiveness - cur_zone.pressure;

            if (needs.frustration > 50.0f || current_score < 0.0f) {
                entt::entity best_zone = entt::null;
                float best_score = current_score;

                for (auto target_ent : zone_view) {
                    const auto& tz = zone_view.get<MacroZoneComponent>(target_ent);
                    if (std::abs(tz.macro_x - mx) <= 1 && std::abs(tz.macro_y - my) <= 1) {
                        float t_score = tz.attractiveness - tz.pressure;
                        if (t_score > best_score + 5.0f) { 
                            best_score = t_score;
                            best_zone = target_ent;
                        }
                    }
                }

                if (best_zone != entt::null) {
                    const auto& target_z = zone_view.get<MacroZoneComponent>(best_zone);
                    pos.x = target_z.macro_x * macro_cell_size + (macro_cell_size / 2);
                    pos.y = target_z.macro_y * macro_cell_size + (macro_cell_size / 2);
                    
                    if (auto* home = m_registry.try_get<HomeComponent>(citizen)) {
                         home->x = pos.x; home->y = pos.y;
                         home->building_entity = entt::null; 
                    }

                    m_dispatcher.enqueue<LogEvent>({"Citizen " + std::to_string(entt::to_integral(citizen)) + " migrated to " + target_z.district_name, LogSeverity::INFO, "Demographics"});
                }
            }
        }
    }

    void triggerBirth(entt::entity parent_a, entt::entity parent_b, int x, int y, int layer) {
        auto child = m_registry.create();
        auto& rel_child = m_registry.get_or_emplace<RelationshipComponent>(child);
        auto& rel_a = m_registry.get_or_emplace<RelationshipComponent>(parent_a);
        
        uint64_t id_a = 0;
        if (auto* npc_a = m_registry.try_get<NPCComponent>(parent_a)) id_a = npc_a->macro_id;
        else if (auto* player = m_registry.try_get<PlayerComponent>(parent_a)) id_a = player->macro_id;
        
        uint64_t id_b = 0;
        if (m_registry.valid(parent_b)) {
            if (auto* npc_b = m_registry.try_get<NPCComponent>(parent_b)) id_b = npc_b->macro_id;
            else if (auto* player = m_registry.try_get<PlayerComponent>(parent_b)) id_b = player->macro_id;
            auto& rel_b = m_registry.get_or_emplace<RelationshipComponent>(parent_b);
        }

        uint64_t m_id = 0;
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (config_view.begin() != config_view.end()) {
            auto& config = config_view.get<WorldConfigComponent>(*config_view.begin());
            m_id = config.next_macro_id++;
        }

        m_registry.emplace<NameComponent>(child, "Infant #" + std::to_string(m_id));
        m_registry.emplace<PositionComponent>(child, x, y, layer);
        m_registry.emplace<RenderableComponent>(child, '.', "#FFFFFF", layer);
        m_registry.emplace<AgentComponent>(child);
        m_registry.emplace<NPCComponent>(child, 20, m_id);
        m_registry.emplace<AgentTaskComponent>(child, AgentTaskType::IDLE);
        m_registry.emplace<NeedsComponent>(child, 100.0f, 100.0f);
        m_registry.emplace<SizeComponent>(child, 1, 1);
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 99);

        // [J.5] Generational Faction Drift
        auto* pol_a = m_registry.try_get<Layer4PoliticalComponent>(parent_a);
        auto* pol_b = m_registry.valid(parent_b) ? m_registry.try_get<Layer4PoliticalComponent>(parent_b) : nullptr;
        
        auto& pol_child = m_registry.emplace<Layer4PoliticalComponent>(child);
        
        int drift_roll = dis(gen);
        if (pol_a && (drift_roll < 85 || !pol_b)) {
            pol_child.primary_faction = pol_a->primary_faction;
            pol_child.faction_loyalty = 0.5f + (float)(dis(gen) % 30) / 100.0f; // 0.5 to 0.8 initial loyalty
        } else if (pol_b && drift_roll < 98) {
            pol_child.primary_faction = pol_b->primary_faction;
            pol_child.faction_loyalty = 0.5f + (float)(dis(gen) % 30) / 100.0f;
        } else {
            // 2% chance or fallback: Random faction or unaligned
            pol_child.primary_faction = "UNALIGNED";
            pol_child.faction_loyalty = 0.0f;
        }

        // [J.5] Generational Religion Drift
        auto* rel_a_comp = m_registry.try_get<ReligiosityComponent>(parent_a);
        auto* rel_b_comp = m_registry.valid(parent_b) ? m_registry.try_get<ReligiosityComponent>(parent_b) : nullptr;
        
        int rel_drift_roll = dis(gen);
        if (rel_a_comp && (rel_drift_roll < 85 || !rel_b_comp)) {
            m_registry.emplace<ReligiosityComponent>(child, rel_a_comp->religion_id, 40.0f + (dis(gen) % 20), 0ULL, (dis(gen) % 100 < 5));
        } else if (rel_b_comp && rel_drift_roll < 98) {
            m_registry.emplace<ReligiosityComponent>(child, rel_b_comp->religion_id, 40.0f + (dis(gen) % 20), 0ULL, (dis(gen) % 100 < 5));
        } else {
            // Random religion or secular
            if (dis(gen) < 10) { // 10% chance to join a random religion if parents differ or 2% case
                auto* reg_comp = m_registry.ctx().find<ReligionRegistryComponent>();
                if (reg_comp && !reg_comp->religions.empty()) {
                    auto it = reg_comp->religions.begin();
                    std::advance(it, dis(gen) % reg_comp->religions.size());
                    m_registry.emplace<ReligiosityComponent>(child, it->first, 10.0f + (dis(gen) % 10), 0ULL, false);
                }
            }
        }

        if (auto* bio_a = m_registry.try_get<Layer1BiologyComponent>(parent_a)) {
            auto& bio_child = m_registry.emplace<Layer1BiologyComponent>(child);
            bio_child.species = bio_a->species;
            auto& phys_child = m_registry.emplace<Layer0PhysicsComponent>(child);
            phys_child.material = (bio_child.species == SpeciesType::SYNTHETIC) ? MaterialType::STEEL : MaterialType::FLESH;
        }

        auto& age = m_registry.emplace<AgeComponent>(child);
        age.years = 0;
        age.current_age_ticks = 0;
        age.stage = LifeStage::INFANT;
        age.expected_lifespan_years = (m_registry.get<Layer1BiologyComponent>(child).species == SpeciesType::SYNTHETIC) ? 200 : 120;

        if (id_a != 0) {
            RelationshipRecord child_rec_a;
            child_rec_a.target_macro_id = id_a;
            child_rec_a.tier = RelationshipTier::FAMILY;
            child_rec_a.affinity = 100.0f;
            child_rec_a.shared_home = true;
            rel_child.records[id_a] = child_rec_a;

            RelationshipRecord parent_rec_a;
            parent_rec_a.target_macro_id = m_id;
            parent_rec_a.tier = RelationshipTier::FAMILY;
            parent_rec_a.affinity = 100.0f;
            rel_a.records[m_id] = parent_rec_a;
        }

        m_dispatcher.trigger<BirthEvent>({child, parent_a, parent_b, x, y, layer});
        m_dispatcher.enqueue<LogEvent>({"Agent " + std::to_string(m_id) + " born into the city.", LogSeverity::INFO, "Population"});
    }

    void instantiateMicroNPC(entt::entity macro_citizen, entt::entity building, int layer_id) {
        auto npc = m_registry.create();
        m_registry.emplace<NPCComponent>(npc, 50, entt::to_integral(macro_citizen));
        m_registry.emplace<PositionComponent>(npc, 5, 5, layer_id);
        m_registry.emplace<RenderableComponent>(npc, 'n', "#00FF00", layer_id);
        
        if (m_registry.all_of<Layer3EconomicComponent>(macro_citizen)) {
            m_registry.emplace<Layer3EconomicComponent>(npc, m_registry.get<Layer3EconomicComponent>(macro_citizen));
        }
        
        if (m_registry.all_of<AgeComponent>(macro_citizen)) {
            m_registry.emplace<AgeComponent>(npc, m_registry.get<AgeComponent>(macro_citizen));
        }

        auto& presence = m_registry.get_or_emplace<MicroPresenceComponent>(macro_citizen);
        presence.is_active = true;
        presence.micro_entity = npc;

        m_dispatcher.enqueue<LogEvent>({"Citizen " + std::to_string(entt::to_integral(macro_citizen)) + " migrated to micro-sim.", LogSeverity::INFO, "Population"});
    }

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_POPULATION_SYSTEM_H
