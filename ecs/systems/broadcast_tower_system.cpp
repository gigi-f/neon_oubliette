#include "broadcast_tower_system.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include "../components/components.h"
#include "../components/base_types.h"
#include "../components/infrastructure_components.h"
#include "../components/broadcast_components.h"

namespace NeonOubliette::Systems {

BroadcastTowerSystem::BroadcastTowerSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
}

void BroadcastTowerSystem::initialize() {
    m_dispatcher.sink<BroadcastPulseEvent>().connect<&BroadcastTowerSystem::onBroadcastPulse>(this);
}

void BroadcastTowerSystem::update(double delta_time) {
    (void)delta_time;
    m_current_tick++;

    auto tower_view = m_registry.view<BroadcastTowerComponent, PositionComponent>();
    
    for (auto entity : tower_view) {
        auto& tower = tower_view.get<BroadcastTowerComponent>(entity);
        auto& pos = tower_view.get<PositionComponent>(entity);

        // Check for power (Layer 0 link)
        bool has_power = true;
        if (m_registry.all_of<PowerGridComponent>(entity)) {
            has_power = m_registry.get<PowerGridComponent>(entity).power_level > 0.1f;
        }

        if (!tower.is_active || !has_power) {
            tower.ticks_until_next = tower.broadcast_interval;
            continue;
        }

        if (tower.ticks_until_next > 0) {
            tower.ticks_until_next--;
        } else {
            // FIRE!
            emit_information(entity, tower);
            
            // Dispatch pulse event for visuals/sounds
            m_dispatcher.trigger(BroadcastPulseEvent{
                entity, pos.x, pos.y, pos.layer_id, tower.radius, tower.controlling_faction
            });
            
            tower.ticks_until_next = tower.broadcast_interval;
        }
    }
}

void BroadcastTowerSystem::emit_information(entt::entity tower_entity, const BroadcastTowerComponent& tower) {
    if (tower.active_information_entity == entt::null) return;

    // Get the actual information record
    if (!m_registry.all_of<InformationComponent>(tower.active_information_entity)) return;
    auto& info = m_registry.get<InformationComponent>(tower.active_information_entity);
    if (info.records.empty()) return;

    // Pick a random rumor from the source info entity
    InformationRecord record = info.records[0]; // For towers, we use the primary message

    // Scale veracity by signal fidelity
    record.veracity *= tower.signal_fidelity;
    record.origin_tick = m_current_tick;
    record.hops = 0; // Fresh broadcast

    auto tower_pos = m_registry.get<PositionComponent>(tower_entity);

    // Find all agents within radius
    auto npc_view = m_registry.view<NPCComponent, PositionComponent, InformationComponent>();
    for (auto agent : npc_view) {
        auto& agent_pos = npc_view.get<PositionComponent>(agent);
        if (agent_pos.layer_id != tower_pos.layer_id) continue;

        float dist = std::sqrt(std::pow(agent_pos.x - tower_pos.x, 2) + std::pow(agent_pos.y - tower_pos.y, 2));
        if (dist <= tower.radius) {
            auto& agent_info = npc_view.get<InformationComponent>(agent);
            
            // Check if already knows this specific message
            bool already_knows = false;
            for (const auto& r : agent_info.records) {
                if (r.content_tag == record.content_tag) {
                    already_knows = true;
                    break;
                }
            }

            if (!already_knows) {
                agent_info.records.push_back(record);
                // Trigger event for tracking
                m_dispatcher.trigger(InformationPropagationEvent{tower_entity, agent, record});
            }
        }
    }

    // Update Faction Influence in radius (Layer 4)
    if (tower.controlling_faction != entt::null) {
        // Find chunks in radius and apply influence boost
        // For simplicity, let's boost influence of agents in range if they are unaligned
        for (auto agent : npc_view) {
            auto& agent_pos = npc_view.get<PositionComponent>(agent);
            if (agent_pos.layer_id != tower_pos.layer_id) continue;
            float dist = std::sqrt(std::pow(agent_pos.x - tower_pos.x, 2) + std::pow(agent_pos.y - tower_pos.y, 2));
            if (dist <= tower.radius) {
                if (m_registry.all_of<FactionAffiliationComponent>(agent)) {
                    auto& affil = m_registry.get<FactionAffiliationComponent>(agent);
                    if (affil.faction_id == tower.controlling_faction) {
                        affil.loyalty = std::min(1.0f, affil.loyalty + 0.01f);
                    } else {
                        affil.loyalty = std::max(0.0f, affil.loyalty - 0.005f);
                    }
                }
            }
        }
    }
}

void BroadcastTowerSystem::onBroadcastPulse(const BroadcastPulseEvent& event) {
    // Log or handle systemic side-effects of a pulse
    // (Actual visuals are handled by the rendering system which sinks this event)
    (void)event;
}

} // namespace NeonOubliette::Systems
