#include "barter_system.h"
#include <algorithm>
#include <cmath>
#include <random>
#include "../components/lod_components.h"
#include "../components/simulation_layers.h"

namespace NeonOubliette {

BarterSystem::BarterSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher)
{
    m_dispatcher.sink<BarterEvent>().connect<&BarterSystem::handleBarterEvent>(*this);
    m_dispatcher.sink<OpenBarterEvent>().connect<&BarterSystem::handleOpenBarterEvent>(*this);
    m_dispatcher.sink<CloseBarterEvent>().connect<&BarterSystem::handleCloseBarterEvent>(*this);
}

void BarterSystem::handleOpenBarterEvent(const OpenBarterEvent& event) {
    auto view = m_registry.view<BarterUIComponent>();
    entt::entity ui_ent;
    if (view.begin() == view.end()) {
        ui_ent = m_registry.create();
        m_registry.emplace<BarterUIComponent>(ui_ent);
    } else {
        ui_ent = *view.begin();
    }
    
    auto& ui = m_registry.get<BarterUIComponent>(ui_ent);
    ui.is_open = true;
    ui.target_agent = event.target; 
    ui.player_offer.clear();
    ui.npc_offer.clear();
    ui.selected_inventory_index = 0;
    ui.focusing_npc_inventory = false;
    
    // [T.3] Negotiation Initialization
    ui.npc_patience = 1.0f;
    ui.npc_greed_margin = 1.15f; // Standard 15% markup
    ui.current_leverage = 0.0f;
    ui.npc_feedback = "Interested in a trade?";
    
    // Adjust greed based on relationship
    if (m_registry.all_of<ReputationComponent>(event.initiator)) {
        auto& rep = m_registry.get<ReputationComponent>(event.initiator);
        if (m_registry.all_of<Layer4PoliticalComponent>(event.target)) {
            auto target_faction = m_registry.get<Layer4PoliticalComponent>(event.target).primary_faction;
            ReputationTier tier = rep.get_tier(target_faction);
            
            switch(tier) {
                case ReputationTier::ALLY: ui.npc_greed_margin = 0.85f; break;
                case ReputationTier::FRIENDLY: ui.npc_greed_margin = 0.95f; break;
                case ReputationTier::FAVORED: ui.npc_greed_margin = 1.05f; break;
                case ReputationTier::NEUTRAL: ui.npc_greed_margin = 1.15f; break;
                case ReputationTier::SUSPICIOUS: ui.npc_greed_margin = 1.4f; break;
                case ReputationTier::HOSTILE: ui.npc_greed_margin = 1.8f; break;
                case ReputationTier::EXCOMMUNICATED: ui.npc_greed_margin = 2.5f; break;
            }
        }
    }
    
    // Fence logic: Fences are professional and have fixed greed
    if (m_registry.all_of<FenceComponent>(event.target)) {
        ui.npc_greed_margin = 1.05f; // Small margin, they live on volume
        ui.npc_feedback = "Show me the goods.";
    }
    
    // Personality modifiers
    if (auto* personality = m_registry.try_get<PersonalityComponent>(event.target)) {
        for (auto tag : personality->tags) {
            if (tag == PersonalityTag::AGGRESSIVE) ui.npc_greed_margin += 0.1f;
            if (tag == PersonalityTag::SUBSERVIENT) ui.npc_greed_margin -= 0.1f;
            if (tag == PersonalityTag::PARANOID) ui.npc_patience = 0.7f; // Start with less patience
        }
    }
}

void BarterSystem::handleCloseBarterEvent(const CloseBarterEvent& event) {
    auto view = m_registry.view<BarterUIComponent>();
    for (auto ent : view) {
        m_registry.get<BarterUIComponent>(ent).is_open = false;
    }
}

void BarterSystem::handleBarterEvent(const BarterEvent& event) {
    switch (event.state) {
        case BarterState::REQUEST: {
            auto& target_name = m_registry.get<NameComponent>(event.target_entity).name;
            auto& initiator_name = m_registry.get<NameComponent>(event.initiator_entity).name;

            // Target evaluates: How much do I want what they're giving me?
            float offered_utility_to_target = calculateUtilityValue(event.target_entity, event.offered_items, event.offered_info);
            
            // Target evaluates: How much do I want what they're taking from me?
            float requested_utility_to_target = calculateUtilityValue(event.target_entity, event.requested_items, event.requested_info);

            // Add credit values to utility
            offered_utility_to_target += (float)event.credit_offered;
            requested_utility_to_target += (float)event.credit_requested;

            // [T.3] Get UI component for negotiation state
            auto ui_view = m_registry.view<BarterUIComponent>();
            if (ui_view.empty()) return;
            auto& ui = m_registry.get<BarterUIComponent>(ui_view.front());

            float greed = ui.npc_greed_margin - ui.current_leverage;
            float deal_ratio = (requested_utility_to_target > 0) ? offered_utility_to_target / (requested_utility_to_target * greed) : 2.0f;

            ui.npc_patience -= 0.05f; // Small patience loss for every request [T.3]

            if (deal_ratio >= 1.0f) {
                m_dispatcher.enqueue(BarterEvent{event.initiator_entity, event.target_entity, event.offered_items, event.requested_items, event.offered_info, event.requested_info, event.credit_offered, event.credit_requested, BarterState::ACCEPT});
                m_dispatcher.enqueue(HUDNotificationEvent{target_name + " accepts the trade.", 2.0f, "#00FF00"});
                ui.npc_feedback = "A fair deal. Very well.";
            } else {
                // DEAL REJECTED - Negotiation Phase [T.3]
                ui.npc_patience -= 0.10f; // Additional patience loss for a bad offer
                
                if (ui.npc_patience <= 0.0f) {
                    ui.npc_feedback = "I'm done here. Goodbye.";
                    m_dispatcher.enqueue(HUDNotificationEvent{target_name + " has lost patience and left.", 3.0f, "#FF0000"});
                    m_dispatcher.enqueue(CloseBarterEvent{});
                } else if (deal_ratio >= 0.7f) {
                    // COUNTER OFFER - NPC suggests an item from player's inventory [T.3]
                    float value_needed = (requested_utility_to_target * greed) - offered_utility_to_target;
                    
                    entt::entity best_item = entt::null;
                    float closest_val_diff = 100000.0f;
                    
                    if (m_registry.all_of<InventoryComponent>(event.initiator_entity)) {
                        const auto& p_inv = m_registry.get<InventoryComponent>(event.initiator_entity);
                        for (auto item : p_inv.contained_items) {
                            // Skip if already offered
                            if (std::find(event.offered_items.begin(), event.offered_items.end(), item) != event.offered_items.end()) continue;
                            
                            float item_val = calculateUtilityValue(event.target_entity, {item}, {});
                            float diff = std::abs(item_val - value_needed);
                            if (diff < closest_val_diff) {
                                closest_val_diff = diff;
                                best_item = item;
                            }
                        }
                    }

                    if (best_item != entt::null) {
                        std::string item_name = m_registry.get<ItemComponent>(best_item).name;
                        
                        // Contextual Feedback based on needs
                        bool has_needs = m_registry.all_of<NeedsComponent>(event.target_entity);
                        float hunger = has_needs ? m_registry.get<NeedsComponent>(event.target_entity).hunger : 100.0f;
                        float health = m_registry.all_of<NPCComponent>(event.target_entity) ? (float)m_registry.get<NPCComponent>(event.target_entity).health : 100.0f;

                        if (hunger < 50.0f && m_registry.all_of<ConsumableComponent>(best_item) && m_registry.get<ConsumableComponent>(best_item).restores_hunger > 0) {
                            ui.npc_feedback = "I'll do it if you throw in that " + item_name + ". My stomach is growling.";
                        } else if (health < 50.0f && m_registry.all_of<ItemMarketCategoryComponent>(best_item) && m_registry.get<ItemMarketCategoryComponent>(best_item).category == ItemMarketCategory::MEDICAL) {
                            ui.npc_feedback = "I need those meds. Include the " + item_name + " and it's yours.";
                        } else {
                            ui.npc_feedback = "Almost there. Give me the " + item_name + " and we can call it even.";
                        }
                    } else {
                        ui.npc_feedback = "That's not enough. You'll need to offer more.";
                    }
                    
                    m_dispatcher.enqueue(BarterEvent{event.initiator_entity, event.target_entity, event.offered_items, event.requested_items, event.offered_info, event.requested_info, event.credit_offered, event.credit_requested, BarterState::COUNTER_OFFER});
                } else {
                    // HARD REJECT
                    ui.npc_patience -= 0.2f; 
                    ui.npc_feedback = "That's an insult. Forget it.";
                    m_dispatcher.enqueue(BarterEvent{event.initiator_entity, event.target_entity, event.offered_items, event.requested_items, event.offered_info, event.requested_info, event.credit_offered, event.credit_requested, BarterState::REJECT});

                    // [T.3] Insult penalty
                    if (m_registry.all_of<RelationshipComponent>(event.target_entity) && m_registry.all_of<PlayerComponent>(event.initiator_entity)) {
                        auto& rel = m_registry.get<RelationshipComponent>(event.target_entity);
                        uint64_t p_macro_id = m_registry.get<PlayerComponent>(event.initiator_entity).macro_id;
                        rel.records[p_macro_id].affinity -= 2.0f;
                    }
                }
            }
            break;
        }
        case BarterState::PRESSURE: {
             auto ui_view = m_registry.view<BarterUIComponent>();
             if (ui_view.empty()) return;
             auto& ui = m_registry.get<BarterUIComponent>(ui_view.front());
             
             // [T.3] Success Probability Logic
             float success_chance = 0.4f;
             
             // Reputation Bonus
             if (m_registry.all_of<ReputationComponent>(event.initiator_entity) && m_registry.all_of<Layer4PoliticalComponent>(event.target_entity)) {
                 auto target_faction = m_registry.get<Layer4PoliticalComponent>(event.target_entity).primary_faction;
                 ReputationTier tier = m_registry.get<ReputationComponent>(event.initiator_entity).get_tier(target_faction);
                 
                 switch(tier) {
                     case ReputationTier::ALLY: success_chance += 0.4f; break;
                     case ReputationTier::FRIENDLY: success_chance += 0.25f; break;
                     case ReputationTier::FAVORED: success_chance += 0.1f; break;
                     case ReputationTier::SUSPICIOUS: success_chance -= 0.15f; break;
                     case ReputationTier::HOSTILE: success_chance -= 0.3f; break;
                     case ReputationTier::EXCOMMUNICATED: success_chance = -1.0f; break; // Immediate Hostility
                     case ReputationTier::NEUTRAL: break;
                 }
             }

             // Personality Multipliers
             if (auto* personality = m_registry.try_get<PersonalityComponent>(event.target_entity)) {
                 for (auto tag : personality->tags) {
                     if (tag == PersonalityTag::SUBSERVIENT) success_chance *= 1.5f;
                     if (tag == PersonalityTag::AGGRESSIVE) success_chance *= 0.5f;
                     if (tag == PersonalityTag::PARANOID) success_chance *= 0.8f;
                 }
             }

             // [T.3] Susceptibility (Biology/Cognitive)
             if (m_registry.all_of<NPCComponent>(event.target_entity)) {
                 if (m_registry.get<NPCComponent>(event.target_entity).health < 40) {
                     success_chance += 0.2f; // Desperate agents easier to pressure
                 }
             }
             if (m_registry.all_of<NeedsComponent>(event.target_entity)) {
                 if (m_registry.get<NeedsComponent>(event.target_entity).frustration > 70.0f) {
                     success_chance -= 0.2f; // Frustrated agents snap easily
                 }
             }

             static thread_local std::mt19937 gen(std::random_device{}());
             std::uniform_real_distribution<float> dis(0.0f, 1.0f);
             
             if (dis(gen) < success_chance) {
                 ui.current_leverage += 0.15f;
                 ui.npc_patience -= 0.1f;
                 ui.npc_feedback = "You drive a hard bargain. Fine.";
                 m_dispatcher.enqueue(HUDNotificationEvent{"Pressure successful!", 2.0f, "#00FF00"});
             } else {
                 ui.npc_patience -= 0.3f;
                 ui.npc_feedback = "You're pushing your luck. Offer something real or get out.";
                 m_dispatcher.enqueue(HUDNotificationEvent{"Pressure failed.", 2.0f, "#FF0000"});

                 // Individual Affinity penalty for pressuring
                 if (m_registry.all_of<RelationshipComponent>(event.target_entity) && m_registry.all_of<PlayerComponent>(event.initiator_entity)) {
                     auto& rel = m_registry.get<RelationshipComponent>(event.target_entity);
                     uint64_t p_macro_id = m_registry.get<PlayerComponent>(event.initiator_entity).macro_id;
                     rel.records[p_macro_id].affinity -= 5.0f;
                 }
             }
             
             if (ui.npc_patience <= 0.0f || success_chance < 0.0f) {
                 ui.npc_feedback = "I'm done here.";
                 m_dispatcher.enqueue(CloseBarterEvent{});
             }
             break;
        }
        case BarterState::COUNTER_OFFER: {
            // Already handled feedback in REQUEST logic
            break;
        }
        case BarterState::ACCEPT: {
            auto& initiator_inv = m_registry.get<InventoryComponent>(event.initiator_entity);
            auto& target_inv = m_registry.get<InventoryComponent>(event.target_entity);

            auto* initiator_econ = m_registry.try_get<Layer3EconomicComponent>(event.initiator_entity);
            auto* target_econ = m_registry.try_get<Layer3EconomicComponent>(event.target_entity);

            // Handle Credits
            if (initiator_econ && target_econ) {
                if (event.credit_offered > 0) {
                    initiator_econ->cash_on_hand -= event.credit_offered;
                    target_econ->cash_on_hand += event.credit_offered;
                }
                if (event.credit_requested > 0) {
                    target_econ->cash_on_hand -= event.credit_requested;
                    initiator_econ->cash_on_hand += event.credit_requested;
                }
            }

            // Handle Items
            for (auto item : event.offered_items) {
                initiator_inv.contained_items.erase(std::remove(initiator_inv.contained_items.begin(), initiator_inv.contained_items.end(), item), initiator_inv.contained_items.end());
                target_inv.contained_items.push_back(item);
                if (m_registry.all_of<ContainedByComponent>(item)) {
                    m_registry.get<ContainedByComponent>(item).container = event.target_entity;
                }
                if (m_registry.all_of<FenceComponent>(event.target_entity) && m_registry.all_of<StolenComponent>(item)) {
                    m_registry.remove<StolenComponent>(item);
                }
            }

            for (auto item : event.requested_items) {
                target_inv.contained_items.erase(std::remove(target_inv.contained_items.begin(), target_inv.contained_items.end(), item), target_inv.contained_items.end());
                initiator_inv.contained_items.push_back(item);
                if (m_registry.all_of<ContainedByComponent>(item)) {
                    m_registry.get<ContainedByComponent>(item).container = event.initiator_entity;
                }
            }

            // [T.3] Handle Information Records
            if (m_registry.all_of<InformationComponent>(event.target_entity)) {
                auto& t_info = m_registry.get<InformationComponent>(event.target_entity);
                for (auto record : event.offered_info) {
                    record.hops++;
                    t_info.records.push_back(record);
                }
            }
            if (m_registry.all_of<InformationComponent>(event.initiator_entity)) {
                auto& i_info = m_registry.get<InformationComponent>(event.initiator_entity);
                for (auto record : event.requested_info) {
                    record.hops++;
                    i_info.records.push_back(record);
                }
            }

            m_dispatcher.enqueue(HUDNotificationEvent{ "Trade successful!", 2.0f, "#00FF00" });
            m_dispatcher.enqueue(CloseBarterEvent{}); // [T.3] Close UI on success

            // Standing and Milestone logic...
            float total_value = 0.0f;
            for (auto item : event.offered_items) total_value += getBaseItemValue(item, event.target_entity);
            for (auto item : event.requested_items) total_value += getBaseItemValue(item, event.target_entity);
            
            if (total_value >= 1000.0f) {
                auto& name_a = m_registry.get<NameComponent>(event.initiator_entity).name;
                auto& name_b = m_registry.get<NameComponent>(event.target_entity).name;
                m_dispatcher.enqueue(MilestoneEvent{"MAJOR_TRADE_DEAL", "A high-value trade deal finalized.", "", "", event.initiator_entity, 3.5f});
            }

            if (m_registry.all_of<Layer4PoliticalComponent>(event.initiator_entity) && m_registry.all_of<Layer4PoliticalComponent>(event.target_entity)) {
                auto target_faction = m_registry.get<Layer4PoliticalComponent>(event.target_entity).primary_faction;
                float rep_boost = 1.0f + (total_value / 500.0f);
                m_dispatcher.enqueue(AgentFactionReputationEvent{event.initiator_entity, target_faction, rep_boost});
            }

            // [T.3] Individual Affinity boost
            if (m_registry.all_of<RelationshipComponent>(event.target_entity) && m_registry.all_of<PlayerComponent>(event.initiator_entity)) {
                auto& rel = m_registry.get<RelationshipComponent>(event.target_entity);
                uint64_t p_macro_id = m_registry.get<PlayerComponent>(event.initiator_entity).macro_id;
                rel.records[p_macro_id].affinity += 2.0f + (total_value / 250.0f);
            }
            break;
        }
        case BarterState::REJECT: {
            m_dispatcher.enqueue(HUDNotificationEvent{ "Trade rejected.", 2.0f, "#FFA500" });
            break;
        }
    }
}

float BarterSystem::calculateInformationUtility(const InformationRecord& record, entt::entity target_agent) {
    float base_value = 0.0f;
    switch(record.type) {
        case InformationType::RUMOR: base_value = 50.0f; break;
        case InformationType::PROPAGANDA: base_value = 100.0f; break;
        case InformationType::INTELLIGENCE: base_value = 250.0f; break;
        case InformationType::PRICE_TIP: base_value = 150.0f; break;
        case InformationType::GOSSIP: base_value = 25.0f; break;
    }

    float value = base_value * record.veracity;
    value *= std::pow(0.9f, (float)record.hops);

    // Faction Relevance [T.3]
    if (m_registry.all_of<FactionAffiliationComponent>(target_agent)) {
        auto target_faction = m_registry.get<FactionAffiliationComponent>(target_agent).faction_id;
        
        // Internal Intel: Source matches Target's faction
        if (record.source_faction == target_faction) {
            value *= 1.5f;
        }

        // Rival Intel: check if source is a rival (not implemented globally yet, but we can check if it's NOT target faction)
        if (record.source_faction != entt::null && record.source_faction != target_faction) {
             value *= 1.2f; // Slight boost for external info
        }
    }
    return value;
}

float BarterSystem::calculateUtilityValue(entt::entity agent_entity, const std::vector<entt::entity>& items, const std::vector<InformationRecord>& records) {
    float total_utility = 0.0f;
    
    // Items Utility
    bool has_needs = m_registry.all_of<NeedsComponent>(agent_entity);
    float hunger = has_needs ? m_registry.get<NeedsComponent>(agent_entity).hunger : 100.0f;
    float thirst = has_needs ? m_registry.get<NeedsComponent>(agent_entity).thirst : 100.0f;

    // Get current weather context
    WeatherState weather = WeatherState::CLEAR;
    auto weather_view = m_registry.view<WeatherComponent>();
    if (!weather_view.empty()) {
        weather = m_registry.get<WeatherComponent>(weather_view.front()).state;
    }

    // Get life stage context
    LifeStage stage = LifeStage::ADULT;
    if (m_registry.all_of<AgeComponent>(agent_entity)) {
        stage = m_registry.get<AgeComponent>(agent_entity).stage;
    }

    for (auto item : items) {
        float base_val = getBaseItemValue(item, agent_entity);
        float item_utility = base_val;

        // [I.3] Fencing Logic
        bool is_stolen = m_registry.all_of<StolenComponent>(item);
        auto* fence_comp = m_registry.try_get<FenceComponent>(agent_entity);
        
        if (is_stolen) {
            if (fence_comp) {
                // Fences buy stolen goods at a discount
                item_utility = base_val * fence_comp->fee_multiplier;
            } else {
                // Normal agents won't buy stolen goods
                item_utility = 0.0f;
            }
        }

        // Apply need-based multipliers
        if (m_registry.all_of<ConsumableComponent>(item)) {
            auto& cons = m_registry.get<ConsumableComponent>(item);
            
            if (cons.restores_hunger > 0 && hunger < 80.0f) {
                float mult = 1.0f + (80.0f - hunger) / 10.0f;
                if (stage == LifeStage::CHILD) mult *= 1.5f; // Children need food more
                item_utility *= mult;
            }
            
            if (cons.restores_thirst > 0 && thirst < 80.0f) {
                float mult = 1.0f + (80.0f - thirst) / 10.0f;
                if (weather == WeatherState::SMOG) mult *= 1.3f; // Thirstier in smog
                item_utility *= mult;
            }
        }

        // [T.2] Medicine Utility
        auto* item_cat = m_registry.try_get<ItemMarketCategoryComponent>(item);
        if (item_cat && item_cat->category == ItemMarketCategory::MEDICAL) {
            if (m_registry.all_of<NPCComponent>(agent_entity)) {
                auto& npc = m_registry.get<NPCComponent>(agent_entity);
                if (npc.health < 50) {
                    item_utility *= 5.0f; // Critical need for medical supplies
                } else if (stage == LifeStage::ELDER) {
                    item_utility *= 2.0f; // Elders value health preservation
                }
            }
        }

        // [T.2] Technology/Energy in Storms
        if (weather == WeatherState::ELECTRICAL_STORM) {
            if (item_cat && (item_cat->category == ItemMarketCategory::TECHNOLOGY || item_cat->category == ItemMarketCategory::TOOLS)) {
                item_utility *= 1.5f; // Repair tools/tech more valuable in storms
            }
        }

        total_utility += item_utility;
    }

    // [T.3] Information Utility
    for (const auto& record : records) {
        total_utility += calculateInformationUtility(record, agent_entity);
    }

    // [H.2] Dogma-based utility adjustments
    if (m_registry.all_of<ReligiosityComponent>(agent_entity)) {
        auto& religiosity = m_registry.get<ReligiosityComponent>(agent_entity);
        auto* registry_ptr = m_registry.ctx().find<ReligionRegistryComponent>();
        if (registry_ptr && registry_ptr->religions.count(religiosity.religion_id)) {
            const auto& religion = registry_ptr->religions.at(religiosity.religion_id);
            float devotion_mult = religiosity.devotion / 100.0f;

            for (auto dogma : religion.dogma_tags) {
                if (dogma == DogmaTag::ASCETIC) {
                    // Ascetics value high-cost items much less
                    if (total_utility > 100.0f) {
                        total_utility *= (1.0f - (0.5f * devotion_mult));
                    }
                } else if (dogma == DogmaTag::HEDONIST) {
                    // Hedonists value luxury items more
                    if (total_utility > 100.0f) {
                        total_utility *= (1.0f + (0.5f * devotion_mult));
                    }
                }
            }
        }
    }
    
    return total_utility;
}

float BarterSystem::getBaseItemValue(entt::entity item_entity, entt::entity perspective_agent) {
    float val = 1.0f;
    if (m_registry.all_of<ItemValueComponent>(item_entity)) {
        val = static_cast<float>(m_registry.get<ItemValueComponent>(item_entity).value);
    }

    auto* pos = m_registry.try_get<PositionComponent>(perspective_agent);
    if (!pos) return val;

    float scarcity_mult = 1.0f;

    // 1. Chunk-level Scarcity (Specific Item Type)
    int cs = get_chunk_size(m_registry);
    int chunk_x = pos->x / cs;
    int chunk_y = pos->y / cs;
    auto chunk_view = m_registry.view<ChunkComponent, MarketDemandComponent>();
    for (auto chunk_ent : chunk_view) {
        auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
        if (chunk.chunk_x == chunk_x && chunk.chunk_y == chunk_y) {
            auto& demand = chunk_view.get<MarketDemandComponent>(chunk_ent);
            if (m_registry.all_of<ItemComponent>(item_entity)) {
                auto& item = m_registry.get<ItemComponent>(item_entity);
                if (demand.item_type_scarcity.count(item.item_type_id)) {
                    scarcity_mult *= demand.item_type_scarcity.at(item.item_type_id);
                }
            }
            break;
        }
    }

    // 2. Macro-level Scarcity (Raw Material)
    auto* item_mat = m_registry.try_get<ItemMaterialComponent>(item_entity);
    if (item_mat) {
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (!config_view.empty()) {
            int cell_size = m_registry.get<WorldConfigComponent>(config_view.front()).macro_cell_size;
            int mx = pos->x / cell_size;
            int my = pos->y / cell_size;
            auto market_view = m_registry.view<MacroMarketComponent, MacroZoneComponent>();
            for (auto m_ent : market_view) {
                const auto& zone = market_view.get<MacroZoneComponent>(m_ent);
                if (zone.macro_x == mx && zone.macro_y == my) {
                    auto& market = market_view.get<MacroMarketComponent>(m_ent);
                    if (market.material_scarcity.count(item_mat->material)) {
                        scarcity_mult *= market.material_scarcity.at(item_mat->material);
                    }
                    break;
                }
            }
        }
    }

    return val * scarcity_mult;
}

} // namespace NeonOubliette
