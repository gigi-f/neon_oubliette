#include "underground_media_system.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <random>
#include "../components/components.h"
#include "../components/base_types.h"
#include "../components/underground_media_components.h"

namespace NeonOubliette::Systems {

UndergroundMediaSystem::UndergroundMediaSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
}

void UndergroundMediaSystem::initialize() {
    m_dispatcher.sink<UseItemEvent>().connect<&UndergroundMediaSystem::onUseItem>(this);
    m_dispatcher.sink<InspectEvent>().connect<&UndergroundMediaSystem::onObserve>(this);
}

void UndergroundMediaSystem::update(double delta_time) {
    (void)delta_time;
    m_current_tick++;

    process_node_pulses();
    
    // Check for guard detection every few ticks
    if (m_current_tick % 5 == 0) {
        check_guard_detection();
    }
}

void UndergroundMediaSystem::process_node_pulses() {
    auto node_view = m_registry.view<PirateNodeComponent, PositionComponent>();
    
    for (auto entity : node_view) {
        auto& node = node_view.get<PirateNodeComponent>(entity);
        auto& pos = node_view.get<PositionComponent>(entity);

        // Pirate nodes don't always need power; they might have batteries or be parasitic.
        // But if they HAVE PowerGridComponent, they respect it.
        if (m_registry.all_of<PowerGridComponent>(entity)) {
            if (m_registry.get<PowerGridComponent>(entity).power_level <= 0.05f) {
                node.ticks_until_next = node.broadcast_interval;
                continue;
            }
        }

        if (node.ticks_until_next > 0) {
            node.ticks_until_next--;
        } else {
            emit_pirate_information(entity, node);
            
            // Dispatch pulse event for visuals
            // Signal to rendering that this is a "glitchy" pulse
            m_dispatcher.trigger(BroadcastPulseEvent{
                entity, pos.x, pos.y, pos.layer_id, node.radius, node.controlling_faction
            });
            
            node.ticks_until_next = node.broadcast_interval;
        }
    }
}

void UndergroundMediaSystem::emit_pirate_information(entt::entity node_entity, PirateNodeComponent& node) {
    if (node.active_information_entity == entt::null) return;

    if (!m_registry.all_of<InformationComponent>(node.active_information_entity)) return;
    auto& source_info = m_registry.get<InformationComponent>(node.active_information_entity);
    if (source_info.records.empty()) return;

    InformationRecord record = source_info.records[0];

    // Glitch corruption: reduce veracity and increase hops
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    if (dis(gen) < node.glitch_factor) {
        record.veracity *= 0.8f;
        record.hops += 1;
    }

    auto node_pos = m_registry.get<PositionComponent>(node_entity);
    auto npc_view = m_registry.view<NPCComponent, PositionComponent, InformationComponent>();

    for (auto agent : npc_view) {
        auto& agent_pos = npc_view.get<PositionComponent>(agent);
        if (agent_pos.layer_id != node_pos.layer_id) continue;

        float dist = std::sqrt(std::pow(agent_pos.x - node_pos.x, 2) + std::pow(agent_pos.y - node_pos.y, 2));
        if (dist <= node.radius) {
            auto& agent_info = npc_view.get<InformationComponent>(agent);
            
            bool already_knows = false;
            for (const auto& r : agent_info.records) {
                if (r.content_tag == record.content_tag) {
                    already_knows = true;
                    break;
                }
            }

            if (!already_knows) {
                agent_info.records.push_back(record);
                m_dispatcher.trigger(InformationPropagationEvent{node_entity, agent, record});
                
                // Underground media specifically targets loyalty of unaligned or hostile agents
                if (m_registry.all_of<Layer4PoliticalComponent>(agent)) {
                    auto& pol = m_registry.get<Layer4PoliticalComponent>(agent);
                    if (pol.primary_faction == "GOVERNMENT") {
                        pol.faction_loyalty = std::max(0.0f, pol.faction_loyalty - 0.02f);
                    } else if (pol.primary_faction == "CITIZEN" || pol.primary_faction == "REBEL") {
                        pol.faction_loyalty = std::min(1.0f, pol.faction_loyalty + 0.01f);
                    }
                }
            }
        }
    }
}

void UndergroundMediaSystem::onUseItem(const UseItemEvent& event) {
    if (!m_registry.valid(event.item_in_inventory_entity)) return;
    
    if (m_registry.all_of<DataSlabComponent>(event.item_in_inventory_entity)) {
        auto& slab = m_registry.get<DataSlabComponent>(event.item_in_inventory_entity);
        
        if (slab.is_encrypted) {
            m_dispatcher.trigger(HUDNotificationEvent{"Slab is encrypted. Reading failed.", 2.0f, "#FF5555"});
            return;
        }

        if (slab.uses_remaining <= 0) {
            m_dispatcher.trigger(HUDNotificationEvent{"Data slab is corrupted and unreadable.", 2.0f, "#888888"});
            return;
        }

        // Add rumor to user
        auto& agent_info = m_registry.get_or_emplace<InformationComponent>(event.user_entity);
        
        bool already_knows = false;
        for (const auto& r : agent_info.records) {
            if (r.content_tag == slab.stored_record.content_tag) {
                already_knows = true;
                break;
            }
        }

        if (!already_knows) {
            agent_info.records.push_back(slab.stored_record);
            m_dispatcher.trigger(InformationPropagationEvent{event.item_in_inventory_entity, event.user_entity, slab.stored_record});
            
            if (m_registry.all_of<HUDComponent>(event.user_entity)) {
                m_dispatcher.trigger(HUDNotificationEvent{"DATA EXTRACTED: " + slab.stored_record.content_tag, 3.0f, "#55FF55"});
            }
        } else {
            if (m_registry.all_of<HUDComponent>(event.user_entity)) {
                m_dispatcher.trigger(HUDNotificationEvent{"Already possess this data.", 2.0f, "#AAAAAA"});
            }
        }

        slab.uses_remaining--;
        if (slab.uses_remaining == 0) {
            m_dispatcher.trigger(HUDNotificationEvent{"Slab burned out.", 1.5f, "#FF5555"});
        }
    }
}

void UndergroundMediaSystem::onObserve(const InspectEvent& event) {
    // If player inspects a hidden pirate node, it becomes visible
    auto node_view = m_registry.view<PirateNodeComponent, PositionComponent>();
    for (auto entity : node_view) {
        auto& pos = node_view.get<PositionComponent>(entity);
        if (pos.x == event.x && pos.y == event.y && pos.layer_id == event.layer_id) {
            auto& node = node_view.get<PirateNodeComponent>(entity);
            if (node.is_hidden) {
                node.is_hidden = false;
                m_dispatcher.trigger(HUDNotificationEvent{"ILLEGAL BROADCAST DETECTED", 2.0f, "#FF00FF"});
            }
        }
    }
}

void UndergroundMediaSystem::check_guard_detection() {
    auto node_view = m_registry.view<PirateNodeComponent, PositionComponent>();
    auto guard_view = m_registry.view<PositionComponent, FactionComponent>(); // Simplification for guards

    for (auto node_entity : node_view) {
        auto& node = node_view.get<PirateNodeComponent>(node_entity);
        auto& node_pos = node_view.get<PositionComponent>(node_entity);

        // Guards only detect non-hidden nodes OR nodes they happen to be right next to
        for (auto guard_entity : guard_view) {
            auto& guard_faction = guard_view.get<FactionComponent>(guard_entity);
            if (guard_faction.faction_id != "GOVERNMENT") continue;

            auto& guard_pos = guard_view.get<PositionComponent>(guard_entity);
            if (guard_pos.layer_id != node_pos.layer_id) continue;

            float dist = std::sqrt(std::pow(guard_pos.x - node_pos.x, 2) + std::pow(guard_pos.y - node_pos.y, 2));
            
            bool detected = (!node.is_hidden && dist < 5.0f) || (dist < 1.5f);

            if (detected) {
                // If the guard is extremely close, they "deactivate" it immediately
                if (dist < 1.5f) {
                    m_dispatcher.trigger(HUDNotificationEvent{"Guard decommissioned illegal node.", 2.0f, "#FFFF00"});
                    m_registry.remove<PirateNodeComponent>(node_entity);
                    
                    // Add a visual indicator of destruction?
                    if (m_registry.all_of<RenderableComponent>(node_entity)) {
                        auto& rend = m_registry.get<RenderableComponent>(node_entity);
                        rend.glyph = 'x';
                        rend.color = "#444444";
                    }
                    break; 
                } else if (node.is_hidden) {
                    // Reveal it to the simulation
                    node.is_hidden = false;
                }
            }
        }
    }
}

} // namespace NeonOubliette::Systems
