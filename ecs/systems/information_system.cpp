#include "information_system.h"
#include <random>
#include <algorithm>
#include <iostream>

namespace NeonOubliette::Systems {

InformationSystem::InformationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
}

void InformationSystem::initialize() {
    m_dispatcher.sink<InformationCreatedEvent>().connect<&InformationSystem::handleInformationCreated>(this);
}

void InformationSystem::update(double delta_time) {
    (void)delta_time;
    m_current_tick++;

    // 1. Veracity decay and removal of obsolete/meaningless rumors
    auto info_view = m_registry.view<InformationComponent>();
    for (auto entity : info_view) {
        auto& info = info_view.get<InformationComponent>(entity);
        auto& records = info.records;

        for (auto it = records.begin(); it != records.end(); ) {
            // Very slow decay over time
            it->veracity = std::max(0.0f, it->veracity - 0.0001f);
            
            // Remove if veracity is extremely low or too many hops
            if (it->veracity < 0.05f || it->hops > 15) {
                it = records.erase(it);
            } else {
                ++it;
            }
        }

        // If no records left, optionally remove component to save memory
        // But let's keep it for now as agents might get new rumors soon.
    }

    // 2. Random rumor generation based on world events
    if (m_current_tick % 500 == 0) {
        int category = rand() % 3;
        if (category == 0) {
            create_rumor(InformationType::PRICE_TIP, "med-kits flush at the tram depot");
        } else if (category == 1) {
            create_rumor(InformationType::RUMOR, "Syndicate expansion into Sector 4");
        } else {
            create_rumor(InformationType::GOSSIP, "Corruption in the Central Guard");
        }
    }
}

bool InformationSystem::propagate_information(entt::entity source, entt::entity target) {
    if (!m_registry.all_of<InformationComponent>(source)) return false;
    
    auto& source_info = m_registry.get<InformationComponent>(source);
    if (source_info.records.empty()) return false;

    // Pick a random rumor from source
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, source_info.records.size() - 1);
    
    InformationRecord rumor = source_info.records[dis(gen)];

    // Add to target
    auto& target_info = m_registry.get_or_emplace<InformationComponent>(target);
    
    // Check if target already has this rumor
    for (const auto& r : target_info.records) {
        if (r.content_tag == rumor.content_tag) {
            // Already knows it. Maybe update veracity if the new one is higher?
            return false; 
        }
    }

    // Retelling effects
    rumor.hops++;
    rumor.veracity *= 0.9f; // Loss of detail with each hop

    target_info.records.push_back(rumor);

    // Notify of propagation (e.g., for logging or player intel)
    m_dispatcher.trigger(InformationPropagationEvent{source, target, rumor});

    return true;
}

void InformationSystem::create_rumor(InformationType type, const std::string& content_tag, entt::entity source_faction) {
    InformationRecord record;
    record.type = type;
    record.content_tag = content_tag;
    record.source_faction = source_faction;
    record.origin_tick = m_current_tick;
    record.veracity = 1.0f;
    record.hops = 0;

    // Distribute to a few random NPCs to start the "virus"
    auto npc_view = m_registry.view<NPCComponent, InformationComponent>();
    int distributed_count = 0;
    int target_distribution = 3;

    // First try agents that already have InformationComponent
    for (auto entity : npc_view) {
        auto& info = npc_view.get<InformationComponent>(entity);
        info.records.push_back(record);
        if (++distributed_count >= target_distribution) break;
    }

    // If we didn't find enough, try any NPC
    if (distributed_count < target_distribution) {
        auto all_npc_view = m_registry.view<NPCComponent>();
        for (auto entity : all_npc_view) {
            if (m_registry.all_of<InformationComponent>(entity)) continue;
            auto& info = m_registry.emplace<InformationComponent>(entity);
            info.records.push_back(record);
            if (++distributed_count >= target_distribution) break;
        }
    }
}

void InformationSystem::handleInformationCreated(const InformationCreatedEvent& event) {
    // Injects a rumor directly into the simulation
    create_rumor(event.record.type, event.record.content_tag, event.record.source_faction);
}

} // namespace NeonOubliette::Systems
