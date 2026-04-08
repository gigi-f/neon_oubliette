#include "crisis_system.h"
#include "../event_declarations.h"
#include "../components/simulation_layers.h"
#include "../components/lod_components.h"
#include <random>
#include <algorithm>

namespace NeonOubliette::Systems {

CrisisSystem::CrisisSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {}

void CrisisSystem::initialize() {
    // Ensure CrisisComponent exists as a singleton
    auto view = m_registry.view<CrisisComponent>();
    if (view.empty()) {
        auto entity = m_registry.create();
        m_registry.emplace<CrisisComponent>(entity);
    }
}

void CrisisSystem::update(double delta_time) {
    (void)delta_time; // Turn-based simulation

    auto view = m_registry.view<CrisisComponent>();
    if (view.empty()) return;
    auto& crisis_comp = view.get<CrisisComponent>(*view.begin());

    process_active_crises(crisis_comp);
    evaluate_new_crises(crisis_comp);

    // [L.2] Update HUD singleton telemetry
    auto hud_view = m_registry.view<HUDComponent>();
    for (auto entity : hud_view) {
        auto& hud = hud_view.get<HUDComponent>(entity);
        hud.economic_stress_display = crisis_comp.stress_level;
        
        bool econ_active = false;
        for (const auto& crisis : crisis_comp.active_crises) {
            if (crisis.type == CrisisType::ECONOMIC_COLLAPSE) {
                hud.economy_status_label = "!!! MARKET CRASH !!!";
                econ_active = true;
                break;
            }
        }
        if (!econ_active) {
            if (crisis_comp.stress_level > 0.4f) hud.economy_status_label = "VOLATILE";
            else if (crisis_comp.stress_level > 0.2f) hud.economy_status_label = "STAGNANT";
            else hud.economy_status_label = "STABLE";
        }
    }
}

void CrisisSystem::process_active_crises(CrisisComponent& crisis_comp) {
    auto it = crisis_comp.active_crises.begin();
    while (it != crisis_comp.active_crises.end()) {
        if (it->ticks_remaining > 0) {
            it->ticks_remaining--;
            
            // [L.1] Core pulse event for other systems to react.
            // Occurs every turn for active crises.
            m_dispatcher.trigger(CrisisEffectEvent{it->type, it->severity});
            
            ++it;
        } else {
            // Crisis naturally resolved
            CrisisType resolved_type = it->type;
            std::string desc = it->description;
            
            it = crisis_comp.active_crises.erase(it);

            // [L.3] Cleanup Crisis-Specific State
            if (resolved_type == CrisisType::ENVIRONMENTAL_HAZARD) {
                auto hazard_view = m_registry.view<EnvironmentalHazardComponent>();
                for (auto entity : hazard_view) {
                    m_registry.get<EnvironmentalHazardComponent>(entity).is_active = false;
                }
            } else if (resolved_type == CrisisType::BIOLOGICAL_OUTBREAK) {
                // Immunity? For now just let agents recover over time in BiologySystem
            }

            m_dispatcher.trigger(CrisisResolvedEvent{resolved_type});
            m_dispatcher.trigger(HUDNotificationEvent{"CRISIS RESOLVED: " + desc, 3.0f, "#00FF00"});
        }
    }
}

void CrisisSystem::evaluate_new_crises(CrisisComponent& crisis_comp) {
    // [L.2] Evaluate city-wide stress metrics
    float econ_stress = evaluate_economic_stress();
    float pol_stress = evaluate_political_stress(); // [L.4]
    
    // Smoothly decay/increase stress level
    float combined_stress = (econ_stress + pol_stress) * 0.5f;
    crisis_comp.stress_level = crisis_comp.stress_level * 0.95f + combined_stress * 0.05f;
    
    static std::default_random_engine generator(12345);
    std::uniform_real_distribution<float> distribution(0.0, 1.0);

    // Crisis limit to prevent simulation spam in core phase
    if (crisis_comp.active_crises.size() < 2) {
        uint64_t current_turn = 0;
        auto city_view = m_registry.view<CityComponent>();
        if (!city_view.empty()) {
            current_turn = city_view.get<CityComponent>(city_view.front()).time_tick;
        }

        if (current_turn - crisis_comp.last_crisis_tick > 500) {
            // Baseline 0.005% + up to 0.5% based on stress
            float chance = 0.00005f + (crisis_comp.stress_level * 0.005f);
            
            if (distribution(generator) < chance) { 
                // Determine type based on dominant stress
                CrisisType type = CrisisType::NONE;
                if (econ_stress > pol_stress && econ_stress > 0.4f && distribution(generator) < 0.7f) {
                    type = CrisisType::ECONOMIC_COLLAPSE;
                } else if (pol_stress > 0.4f && distribution(generator) < 0.7f) {
                    type = (distribution(generator) < 0.6f) ? CrisisType::POLITICAL_UNREST : CrisisType::FACTION_WAR;
                } else {
                    int type_idx = static_cast<int>(distribution(generator) * 7) + 1;
                    type = static_cast<CrisisType>(type_idx);
                }
                
                std::string desc;
                switch(type) {
                    case CrisisType::ECONOMIC_COLLAPSE: desc = "Market Crash"; break;
                    case CrisisType::BIOLOGICAL_OUTBREAK: desc = "Tox-Flu Epidemic"; break;
                    case CrisisType::INFRASTRUCTURE_FAILURE: desc = "Grid Instability"; break;
                    case CrisisType::POLITICAL_UNREST: desc = "Syndicate Riots"; break;
                    case CrisisType::XENO_INCURSION: desc = "Cacogen Swarm"; break;
                    case CrisisType::ENVIRONMENTAL_HAZARD: desc = "Acid Storm Surge"; break;
                    case CrisisType::FACTION_WAR: desc = "Street Warfare"; break;
                    default: desc = "Systemic Anomaly"; break;
                }

                // Default duration 500-1500 turns
                uint32_t duration = 500 + static_cast<uint32_t>(distribution(generator) * 1000);
                trigger_crisis(crisis_comp, type, 0.5f + (crisis_comp.stress_level * 0.5f), duration, desc);
                crisis_comp.last_crisis_tick = current_turn;

                // [L.3] Seed Crisis-Specific Initial State
                if (type == CrisisType::BIOLOGICAL_OUTBREAK) {
                    // Seed infection in 1-3 agents
                    auto agents = m_registry.view<AgentComponent, Layer1BiologyComponent>();
                    int seed_count = 1 + (int)(distribution(generator) * 3);
                    int seeded = 0;
                    for (auto entity : agents) {
                        if (distribution(generator) < 0.05f) {
                            m_registry.emplace_or_replace<InfectionComponent>(entity, 0.0f, 0.05f, 0.5f, current_turn, true);
                            seeded++;
                            if (seeded >= seed_count) break;
                        }
                    }
                } else if (type == CrisisType::ENVIRONMENTAL_HAZARD) {
                    // Trigger extreme weather and set hazard singleton
                    auto hazard_view = m_registry.view<EnvironmentalHazardComponent>();
                    entt::entity hazard_ent;
                    if (hazard_view.empty()) {
                        hazard_ent = m_registry.create();
                        m_registry.emplace<EnvironmentalHazardComponent>(hazard_ent);
                    } else {
                        hazard_ent = hazard_view.front();
                    }
                    auto& hazard = m_registry.get<EnvironmentalHazardComponent>(hazard_ent);
                    hazard.is_active = true;
                    hazard.hazard_description = desc;
                    hazard.toxicity_level = 0.5f;
                    hazard.acidity_level = 0.8f;

                    // Force weather transition
                    auto weather_view = m_registry.view<WeatherComponent>();
                    if (!weather_view.empty()) {
                        auto& weather = weather_view.get<WeatherComponent>(weather_view.front());
                        weather.state = WeatherState::ACID_RAIN;
                        weather.intensity = 1.0f;
                        weather.ticks_remaining = duration;
                    }
                } else if (type == CrisisType::POLITICAL_UNREST) {
                    // Seed frustration in agents of a specific faction
                    auto pol_view = m_registry.view<Layer4PoliticalComponent, NeedsComponent>();
                    for (auto entity : pol_view) {
                        auto& pol = pol_view.get<Layer4PoliticalComponent>(entity);
                        auto& needs = pol_view.get<NeedsComponent>(entity);
                        if (pol.primary_faction == "REBEL" || pol.primary_faction == "SYNDICATE") {
                            needs.frustration += 50.0f;
                            needs.socialization -= 30.0f;
                        }
                    }
                } else if (type == CrisisType::FACTION_WAR) {
                    // Dramatically drop standing between two factions
                    m_dispatcher.enqueue<ChangeFactionStandingEvent>({"GOVERNMENT", "REBEL", -50.0f});
                    m_dispatcher.enqueue<ChangeFactionStandingEvent>({"CORPORATE", "SYNDICATE", -40.0f});
                }
            }
        }
    }
}

float CrisisSystem::evaluate_political_stress() {
    float stress = 0.0f;
    
    // 1. Influence Overlap Stress (Contested territory)
    auto chunk_view = m_registry.view<ChunkComponent, FactionInfluenceFieldComponent>();
    float avg_overlap = 0.0f;
    int chunk_count = 0;
    for (auto entity : chunk_view) {
        auto& inf = chunk_view.get<FactionInfluenceFieldComponent>(entity);
        if (inf.influence.size() > 1) {
            // Count significant competitors (> 10 influence)
            int competitors = 0;
            for (auto const& [f, amount] : inf.influence) {
                if (amount > 10.0f) competitors++;
            }
            if (competitors > 1) {
                avg_overlap += (float)competitors * 0.1f;
            }
        }
        chunk_count++;
    }
    if (chunk_count > 0) stress += (avg_overlap / (float)chunk_count);

    // 2. Global Public Opinion (Faction Approval)
    auto opinion_view = m_registry.view<PublicOpinionComponent>();
    if (!opinion_view.empty()) {
        auto& opinion = opinion_view.get<PublicOpinionComponent>(opinion_view.front());
        float total_approval = 0.0f;
        int faction_count = 0;
        for (auto const& [f, approval] : opinion.faction_approval) {
            total_approval += approval;
            faction_count++;
        }
        if (faction_count > 0) {
            float avg_approval = total_approval / (float)faction_count;
            if (avg_approval < 40.0f) stress += (40.0f - avg_approval) * 0.01f;
        }
    }

    // 3. Wanted Level / Crime Density Stress
    auto market_view = m_registry.view<MacroMarketComponent>();
    float avg_crime = 0.0f;
    for (auto entity : market_view) {
        avg_crime += market_view.get<MacroMarketComponent>(entity).crime_rate;
    }
    if (chunk_count > 0) stress += (avg_crime / (float)chunk_count) * 2.0f;

    return std::clamp(stress, 0.0f, 1.0f);
}

float CrisisSystem::evaluate_economic_stress() {
    float stress = 0.0f;
    
    // 1. Stock Market Volatility / Crash Stress
    auto stock_view = m_registry.view<StockComponent>();
    float avg_momentum = 0.0f;
    int stock_count = 0;
    for (auto entity : stock_view) {
        auto& stock = stock_view.get<StockComponent>(entity);
        avg_momentum += stock.momentum;
        stock_count++;
    }
    if (stock_count > 0) {
        avg_momentum /= static_cast<float>(stock_count);
        // Negative momentum increases stress
        if (avg_momentum < 0.0f) {
            stress += std::abs(avg_momentum) * 0.05f; 
        }
    }

    // 2. Macro-Market Stress (Unemployment and Poverty)
    auto market_view = m_registry.view<MacroMarketComponent>();
    float avg_unemployment = 0.0f;
    float avg_wealth = 0.0f;
    int market_count = 0;
    for (auto entity : market_view) {
        auto& market = market_view.get<MacroMarketComponent>(entity);
        avg_unemployment += market.unemployment_rate;
        avg_wealth += market.average_wealth;
        market_count++;
    }
    
    if (market_count > 0) {
        avg_unemployment /= static_cast<float>(market_count);
        avg_wealth /= static_cast<float>(market_count);
        
        // High unemployment increases stress
        stress += avg_unemployment * 0.5f;
        
        // Low wealth increases stress (baseline 100)
        if (avg_wealth < 80.0f) {
            stress += (80.0f - avg_wealth) * 0.01f;
        }
    }
    
    return std::clamp(stress, 0.0f, 1.0f);
}

void CrisisSystem::trigger_crisis(CrisisComponent& crisis_comp, CrisisType type, float severity, uint32_t duration, const std::string& description) {
    ActiveCrisis nc;
    nc.type = type;
    nc.severity = severity;
    nc.ticks_remaining = duration;
    nc.description = description;
    
    crisis_comp.active_crises.push_back(nc);
    
    m_dispatcher.trigger(CrisisStartedEvent{type, severity, description});
    m_dispatcher.trigger(HUDNotificationEvent{"!!! CRISIS DETECTED: " + description + " !!!", 5.0f, "#FF0000"});
}

} // namespace NeonOubliette::Systems
