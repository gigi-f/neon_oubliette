#include "inheritance_system.h"
#include "../components/simulation_layers.h"
#include "../components/lod_components.h"
#include <iostream>
#include <algorithm>

namespace NeonOubliette {

void InheritanceSystem::initialize() {
    m_dispatcher.sink<AgentDeathEvent>().connect<&InheritanceSystem::handle_agent_death>(this);
}

void InheritanceSystem::handle_agent_death(const AgentDeathEvent& event) {
    if (m_registry.valid(event.entity) && !m_registry.all_of<AgentComponent>(event.entity)) {
        return; // Already processed by another death check
    }

    int credits = 0;
    std::string faction_id = "CITIZEN";
    std::map<std::string, uint64_t> portfolio;
    std::vector<entt::entity> items;
    std::unordered_map<uint64_t, RelationshipRecord> relationships;

    // 1. Extract data from the deceased
    if (m_registry.valid(event.entity)) {
        // Live entity death
        if (auto* econ = m_registry.try_get<Layer3EconomicComponent>(event.entity)) {
            credits = econ->cash_on_hand;
            portfolio = econ->portfolio;
        }
        if (auto* pol = m_registry.try_get<Layer4PoliticalComponent>(event.entity)) {
            faction_id = pol->primary_faction;
        }
        if (auto* inv = m_registry.try_get<InventoryComponent>(event.entity)) {
            items = inv->contained_items;
        }
        if (auto* rel = m_registry.try_get<RelationshipComponent>(event.entity)) {
            relationships = rel->records;
        }
    } else {
        // Macro agent death - data passed in the event
        credits = event.credits;
        faction_id = event.faction_id;
        portfolio = event.portfolio;
        relationships = event.relationships;
        items = event.items;
    }

    if (credits > 0 || !portfolio.empty() || !items.empty()) {
        distribute_assets(event.macro_id, credits, faction_id, portfolio, items, relationships);
    }
}

void InheritanceSystem::distribute_assets(uint64_t deceased_id, int credits, const std::string& faction_id, 
                                          const std::map<std::string, uint64_t>& portfolio,
                                          const std::vector<entt::entity>& items,
                                          const std::unordered_map<uint64_t, RelationshipRecord>& relationships) {
    (void)deceased_id;
    
    std::vector<uint64_t> family_heirs;
    for (auto const& [target_id, record] : relationships) {
        if (record.tier == RelationshipTier::FAMILY) {
            family_heirs.push_back(target_id);
        }
    }

    float faction_tax_rate = 0.15f;
    float state_tax_rate = 0.05f;
    
    if (family_heirs.empty()) {
        faction_tax_rate = 0.60f;
        state_tax_rate = 0.40f;
    }

    int faction_cut = static_cast<int>(credits * faction_tax_rate);
    int state_cut = static_cast<int>(credits * state_tax_rate);
    int remaining_credits = credits - faction_cut - state_cut;

    // Log the event
    m_dispatcher.enqueue<LogEvent>({"Agent " + std::to_string(deceased_id) + " died. Assets: " + std::to_string(credits) + "cr. Faction tax: " + std::to_string(faction_cut) + ".", LogSeverity::INFO, "Inheritance"});

    // Find live and macro heirs
    auto mapping_view = m_registry.view<MacroIdMappingTag>();
    std::unordered_map<uint64_t, entt::entity> mapping;
    if (mapping_view.begin() != mapping_view.end()) {
        mapping = m_registry.get<MacroIdMappingTag>(*mapping_view.begin()).mapping;
    }

    std::vector<entt::entity> live_heirs;
    std::vector<uint64_t> macro_heir_ids;

    for (uint64_t heir_id : family_heirs) {
        auto it = mapping.find(heir_id);
        if (it != mapping.end() && m_registry.valid(it->second)) {
            live_heirs.push_back(it->second);
        } else {
            macro_heir_ids.push_back(heir_id);
        }
    }

    size_t total_heirs = live_heirs.size() + macro_heir_ids.size();
    if (total_heirs > 0) {
        int share = remaining_credits / (int)total_heirs;
        
        // Distribute to live heirs
        for (auto heir_ent : live_heirs) {
            if (auto* econ = m_registry.try_get<Layer3EconomicComponent>(heir_ent)) {
                econ->cash_on_hand += share;
                // Simple stock transfer (1st heir takes all for now or split later)
                if (heir_ent == live_heirs[0]) {
                    for (auto const& [ticker, shares] : portfolio) {
                        econ->portfolio[ticker] += shares;
                    }
                }
            }
            m_dispatcher.trigger(HUDNotificationEvent{"Inheritance received.", 2.0f, "#00FF00"});
        }

        // Distribute to macro heirs
        if (!macro_heir_ids.empty()) {
            auto chunk_view = m_registry.view<ChunkComponent>();
            for (auto heir_id : macro_heir_ids) {
                bool found = false;
                for (auto chunk_ent : chunk_view) {
                    auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
                    for (auto& record : chunk.stored_agents) {
                        if (record.macro_id == heir_id) {
                            record.cash_on_hand += share;
                            if (heir_id == macro_heir_ids[0] && live_heirs.empty()) {
                                for (auto const& [ticker, shares] : portfolio) {
                                    record.portfolio[ticker] += shares;
                                }
                            }
                            found = true; break;
                        }
                    }
                    if (found) break;
                }
            }
        }
    }

    // Handle items
    if (!items.empty()) {
        if (!live_heirs.empty()) {
            // Give items to the first live heir if they have space (abstracted as always having space for now)
            auto& first_heir_inv = m_registry.get_or_emplace<InventoryComponent>(live_heirs[0]);
            for (auto item : items) {
                first_heir_inv.contained_items.push_back(item);
            }
        } else {
            // No live heirs nearby, items are lost/liquidated or dropped as loot 
            // In a real roguelike we'd drop them on the tile, but here they stay in the corpse inventory 
            // if we don't remove them.
        }
    }
}

} // namespace NeonOubliette
