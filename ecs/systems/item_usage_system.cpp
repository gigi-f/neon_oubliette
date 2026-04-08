#include "item_usage_system.h"
#include <iostream>
#include <algorithm>
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../event_declarations.h"

namespace NeonOubliette {

ItemUsageSystem::ItemUsageSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry(registry), dispatcher(dispatcher) {
    dispatcher.sink<UseItemEvent>().connect<&ItemUsageSystem::handleUseItemEvent>(*this);
}

void ItemUsageSystem::handleUseItemEvent(const UseItemEvent& event) {
    if (!registry.valid(event.item_in_inventory_entity) || !registry.valid(event.user_entity))
        return;

    // 1. Check for Consumables
    if (registry.all_of<ConsumableComponent>(event.item_in_inventory_entity)) {
        dispatcher.trigger<ConsumeItemEvent>({event.user_entity, event.item_in_inventory_entity});
        return;
    }

    // 2. Check for Usable Tools/Items
    if (registry.all_of<UsableComponent>(event.item_in_inventory_entity)) {
        // [C.2] Flash the HUD slot if it's the player's held item
        if (registry.all_of<HUDComponent>(event.user_entity)) {
            auto& hud = registry.get<HUDComponent>(event.user_entity);
            if (hud.held_item == event.item_in_inventory_entity) {
                hud.held_item_flash_timer = 0.3f;
            }
        }

        const auto& usable = registry.get<UsableComponent>(event.item_in_inventory_entity);
        std::string item_name = "item";
        if (registry.all_of<NameComponent>(event.item_in_inventory_entity)) {
            item_name = registry.get<NameComponent>(event.item_in_inventory_entity).name;
        }

        bool used = false;
        std::string feedback = "Used " + item_name;

        // Logic based on effect_id
        if (usable.effect_id == "diagnostic_scanner") {
            if (registry.all_of<PositionComponent>(event.user_entity)) {
                auto& pos = registry.get<PositionComponent>(event.user_entity);
                dispatcher.trigger(InspectEvent{event.user_entity, pos.layer_id, pos.x, pos.y, InspectionMode::SURFACE_SCAN});
                feedback = "Scanner pulse sent.";
                used = true;
            }
        } else if (usable.effect_id == "bio_scanner") {
             if (registry.all_of<PositionComponent>(event.user_entity)) {
                auto& pos = registry.get<PositionComponent>(event.user_entity);
                dispatcher.trigger(InspectEvent{event.user_entity, pos.layer_id, pos.x, pos.y, InspectionMode::BIOLOGICAL_AUDIT});
                feedback = "Biological audit pulse sent.";
                used = true;
            }
        } else if (usable.effect_id == "repair_tool") {
            feedback = "Tool active. No repairable target detected.";
            used = true; 
        } else if (usable.effect_id == "propaganda_poster") {
             // Find building at user position
             if (registry.all_of<PositionComponent>(event.user_entity)) {
                 auto& pos = registry.get<PositionComponent>(event.user_entity);
                 auto building_view = registry.view<BuildingComponent, PositionComponent>();
                 entt::entity target_building = entt::null;
                 for (auto b_ent : building_view) {
                     auto& b_pos = building_view.get<PositionComponent>(b_ent);
                     // Buildings can be larger than 1x1, but let's check exact tile for now
                     if (b_pos.x == pos.x && b_pos.y == pos.y && b_pos.layer_id == pos.layer_id) {
                         target_building = b_ent; break;
                     }
                 }

                 if (target_building != entt::null) {
                     // Get player faction
                     std::string faction_id = "NEUTRAL";
                     if (auto* pol = registry.try_get<Layer4PoliticalComponent>(event.user_entity)) {
                         faction_id = pol->primary_faction;
                     }

                     // Apply Graffiti/Propaganda
                     auto& graffiti = registry.get_or_emplace<GraffitiComponent>(target_building);
                     graffiti.type = GraffitiType::TAG;
                     graffiti.glyph = '$'; // Symbol of influence
                     graffiti.creator_faction = event.user_entity; // Set as player for tracking
                     
                     // [P.2] Reputation boost with the faction you are promoting
                     dispatcher.enqueue<AgentFactionReputationEvent>({
                         event.user_entity,
                         faction_id,
                         10.0f // High reward for risk
                     });

                     // [I.5] This is a crime (Vandalism)
                     dispatcher.enqueue<CrimeReportEvent>({
                        event.user_entity,
                        target_building,
                        pos.x, pos.y, pos.layer_id,
                        "VANDALISM",
                        0
                     });

                     feedback = "Poster plastered onto the wall. Your faction's influence grows.";
                     used = true;
                     
                     // Consume the poster
                     auto& inv = registry.get<InventoryComponent>(event.user_entity);
                     inv.contained_items.erase(std::remove(inv.contained_items.begin(), inv.contained_items.end(), event.item_in_inventory_entity), inv.contained_items.end());
                     registry.destroy(event.item_in_inventory_entity);
                 } else {
                     feedback = "Must be next to a building to plaster a poster.";
                 }
             }
        } else {
            feedback = "Used " + item_name + " with no effect.";
            used = true;
        }

        if (used) {
            dispatcher.trigger(HUDNotificationEvent{feedback, 2.0f, "#FFFFFF"});
        }
    }
}

} // namespace NeonOubliette