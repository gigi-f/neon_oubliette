#include "supply_chain_system.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../components/lod_components.h"
#include "../event_declarations.h"
#include <random>
#include <algorithm>

namespace NeonOubliette {

SupplyChainSystem::SupplyChainSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    m_dispatcher.sink<TurnEvent>().connect<&SupplyChainSystem::handleTurnEvent>(*this);
    m_dispatcher.sink<CrisisEffectEvent>().connect<&SupplyChainSystem::handleCrisisEffect>(*this);
}

void SupplyChainSystem::update(double delta_time) {
    (void)delta_time; // Turn-based
}

void SupplyChainSystem::handleTurnEvent(const TurnEvent& event) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    auto factory_view = m_registry.view<FactoryComponent, PositionComponent>();
    
    // 1. Gather Macro Data for context (Public Opinion & Economic Stress)
    float avg_approval = 50.0f;
    auto opinion_view = m_registry.view<PublicOpinionComponent>();
    if (!opinion_view.empty()) {
        auto& opinion = opinion_view.get<PublicOpinionComponent>(opinion_view.front());
        float total = 0.0f;
        for (auto const& [f, approval] : opinion.faction_approval) total += approval;
        if (!opinion.faction_approval.empty()) avg_approval = total / (float)opinion.faction_approval.size();
    }

    // 2. Process each factory
    for (auto entity : factory_view) {
        auto& factory = factory_view.get<FactoryComponent>(entity);
        auto& pos = factory_view.get<PositionComponent>(entity);
        auto& disruption = m_registry.get_or_emplace<SupplyChainDisruptionComponent>(entity);

        // --- Infrastructure Bottlenecks ---
        // Check for deteriorated infrastructure in the current chunk or building itself
        float local_bottleneck = m_global_bottleneck_stress;
        auto* health = m_registry.try_get<BuildingHealthComponent>(entity);
        if (health && health->integrity < 50.0f) {
            local_bottleneck += (50.0f - health->integrity) * 0.01f;
        }
        
        // --- Labor Strikes ---
        // Likelihood increases if the owning faction has low public approval or high frustration
        float local_strike = m_global_strike_stress;
        if (avg_approval < 40.0f) {
            local_strike += (40.0f - avg_approval) * 0.005f;
        }
        
        // --- Industrial Sabotage ---
        // Higher crime rate in the chunk increases sabotage risk
        float local_sabotage = m_global_sabotage_stress;
        // Search for chunk containing this factory
        auto chunk_view = m_registry.view<ChunkComponent, MacroMarketComponent>();
        for (auto chunk_ent : chunk_view) {
            const auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
            int cs = get_chunk_size(m_registry);
            int cx = chunk.chunk_x * cs;
            int cy = chunk.chunk_y * cs;
            if (pos.x >= cx && pos.x < cx + cs && pos.y >= cy && pos.y < cy + cs) {
                const auto& market = chunk_view.get<MacroMarketComponent>(chunk_ent);
                local_sabotage += market.crime_rate * 0.2f;
                break;
            }
        }

        // Apply natural recovery and update
        disruption.transport_bottleneck = std::clamp(disruption.transport_bottleneck * 0.9f + local_bottleneck * 0.1f, 0.0f, 1.0f);
        disruption.labor_strike = std::clamp(disruption.labor_strike * 0.9f + local_strike * 0.1f, 0.0f, 1.0f);
        disruption.sabotage_risk = std::clamp(disruption.sabotage_risk * 0.9f + local_sabotage * 0.1f, 0.0f, 1.0f);

        // Natural recovery of the base disruption component (non-stressed parts)
        disruption.transport_bottleneck = std::max(0.0f, disruption.transport_bottleneck - disruption.recovery_rate);
        disruption.labor_strike = std::max(0.0f, disruption.labor_strike - disruption.recovery_rate);
        disruption.sabotage_risk = std::max(0.0f, disruption.sabotage_risk - disruption.recovery_rate);

        // Trigger events for significant disruptions
        if (disruption.transport_bottleneck > 0.6f && dis(gen) < 0.05f) {
            m_dispatcher.trigger<SupplyChainDisruptedEvent>({entity, "LOGISTICS", disruption.transport_bottleneck});
            m_dispatcher.enqueue<SpeechEvent>({entity, "Logistics queue stalled.", 15, AudibilityLevel::CLEAR});
        }
        if (disruption.labor_strike > 0.5f && dis(gen) < 0.05f) {
            m_dispatcher.trigger<SupplyChainDisruptedEvent>({entity, "STRIKE", disruption.labor_strike});
            m_dispatcher.trigger<HUDNotificationEvent>({"Industrial strike at " + factory.owning_faction + " facility!", 3.0f, "#FF3333"});
        }
        if (disruption.sabotage_risk > 0.4f && dis(gen) < 0.02f) {
            m_dispatcher.trigger<SupplyChainDisruptedEvent>({entity, "SABOTAGE", disruption.sabotage_risk});
        }
    }

    // Reset global stress factors for the next turn; they are refilled by CrisisEffectEvent
    m_global_bottleneck_stress *= 0.8f;
    m_global_strike_stress *= 0.8f;
    m_global_sabotage_stress *= 0.8f;
}

void SupplyChainSystem::handleCrisisEffect(const CrisisEffectEvent& event) {
    float impact = event.intensity * 0.2f;

    switch (event.type) {
        case CrisisType::ECONOMIC_COLLAPSE:
            m_global_bottleneck_stress += impact;
            m_global_strike_stress += impact * 1.5f;
            break;
        case CrisisType::INFRASTRUCTURE_FAILURE:
            m_global_bottleneck_stress += impact * 2.0f;
            break;
        case CrisisType::POLITICAL_UNREST:
            m_global_strike_stress += impact * 2.0f;
            m_global_sabotage_stress += impact;
            break;
        case CrisisType::FACTION_WAR:
            m_global_sabotage_stress += impact * 2.5f;
            m_global_bottleneck_stress += impact;
            break;
        default:
            break;
    }
}

} // namespace NeonOubliette
