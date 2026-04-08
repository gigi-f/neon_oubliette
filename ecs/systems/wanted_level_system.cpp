#include "wanted_level_system.h"
#include <algorithm>
#include <cmath>
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../components/zoning_components.h"

namespace NeonOubliette {

WantedLevelSystem::WantedLevelSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher)
{
    m_dispatcher.sink<CrimeReportEvent>().connect<&WantedLevelSystem::handleCrimeReport>(*this);
}

void WantedLevelSystem::initialize() {
    // Ensure player has a WantedComponent
    auto player_view = m_registry.view<PlayerComponent>();
    for (auto entity : player_view) {
        if (!m_registry.all_of<WantedComponent>(entity)) {
            m_registry.emplace<WantedComponent>(entity);
        }
    }
}

void WantedLevelSystem::update(double delta_time) {
    (void)delta_time;
    auto city_view = m_registry.view<CityComponent>();
    if (city_view.begin() == city_view.end()) return;
    uint64_t current_tick = m_registry.get<CityComponent>(city_view.front()).time_tick;

    auto player_view = m_registry.view<PlayerComponent, WantedComponent, PositionComponent>();
    if (player_view.begin() == player_view.end()) return;

    auto player_ent = player_view.front();
    auto& wanted = player_view.get<WantedComponent>(player_ent);
    auto& pos = player_view.get<PositionComponent>(player_ent);

    // 1. TRESSPASSING DETECTION
    auto* interior = m_registry.try_get<InteriorStateComponent>(player_ent);
    if (interior && interior->building_entity != entt::null) {
        auto* b_interior = m_registry.try_get<BuildingInteriorComponent>(interior->building_entity);
        auto* property = m_registry.try_get<PropertyComponent>(interior->building_entity);
        
        if (b_interior && property && interior->current_room_index >= 0 && 
            (size_t)interior->current_room_index < b_interior->rooms.size()) {
            
            const auto& room = b_interior->rooms[interior->current_room_index];
            
            // Restricted Room Tags
            bool is_private = (room.tag == RoomTag::OFFICE || 
                               room.tag == RoomTag::SERVER_ROOM || 
                               room.tag == RoomTag::EXECUTIVE_SUITE || 
                               room.tag == RoomTag::BEDROOM || 
                               room.tag == RoomTag::SUPERVISOR_OFFICE ||
                               room.tag == RoomTag::CLANDESTINE_LAB);

            if (is_private && property->owner_faction != "NEUTRAL") {
                // Check if any guard is in the building and sees the player
                auto guard_view = m_registry.view<PositionComponent, VisibilityComponent, PatrolComponent>();
                bool witnessed = false;
                for (auto guard : guard_view) {
                    const auto& g_pos = guard_view.get<PositionComponent>(guard);
                    if (g_pos.layer_id == pos.layer_id) {
                        const auto& g_vis = guard_view.get<VisibilityComponent>(guard);
                        if (g_vis.visible_tiles.count(pos)) {
                            witnessed = true;
                            break;
                        }
                    }
                }

                if (witnessed && (current_tick % 10 == 0)) { // Don't spam report every tick
                    m_dispatcher.enqueue<CrimeReportEvent>({
                        player_ent,
                        interior->building_entity,
                        pos.x, pos.y, pos.layer_id,
                        "TRESPASSING",
                        current_tick
                    });
                    m_dispatcher.enqueue<HUDNotificationEvent>({
                        "Witnessed! Restricted area trespassing.",
                        2.0f,
                        "#FF5500"
                    });
                }
            }
        }
    }

    // 2. CONTRABAND DETECTION (Proximity to Guards)
    auto* inventory = m_registry.try_get<InventoryComponent>(player_ent);
    if (inventory && !inventory->contained_items.empty()) {
        bool has_contraband = false;
        for (auto item : inventory->contained_items) {
            if (m_registry.all_of<ContrabandComponent>(item)) {
                has_contraband = true;
                break;
            }
        }

        if (has_contraband) {
            auto guard_view = m_registry.view<PositionComponent, VisibilityComponent, PatrolComponent>();
            for (auto guard : guard_view) {
                const auto& g_pos = guard_view.get<PositionComponent>(guard);
                if (g_pos.layer_id == pos.layer_id) {
                    float dist = std::sqrt(std::pow(g_pos.x - pos.x, 2) + std::pow(g_pos.y - pos.y, 2));
                    if (dist < 3.0f) { // Very close proximity for "stop and search" simulation
                        const auto& g_vis = guard_view.get<VisibilityComponent>(guard);
                        if (g_vis.visible_tiles.count(pos) && (current_tick % 20 == 0)) {
                            m_dispatcher.enqueue<CrimeReportEvent>({
                                player_ent,
                                entt::null,
                                pos.x, pos.y, pos.layer_id,
                                "CONTRABAND_POSSESSION",
                                current_tick
                            });
                             m_dispatcher.enqueue<HUDNotificationEvent>({
                                "Search triggered! Contraband detected.",
                                3.0f,
                                "#FF0000"
                            });
                        }
                    }
                }
            }
        }
    }

    // 3. DECAY
    // Decay global notoriety every 50 ticks
    if (current_tick >= m_last_decay_tick + 50) {
        processDecay();
        m_last_decay_tick = current_tick;
    }

    // 4. GUARD ALERTS
    updateGuardAlerts();

    // 5. UPDATE HUD COMPONENT
    auto* hud = m_registry.try_get<HUDComponent>(player_ent);
    if (hud) {
        hud->faction_wanted_levels.clear();
        for (auto const& [faction, level] : wanted.faction_wanted_levels) {
            if (level > 0) {
                hud->faction_wanted_levels[faction] = level;
            }
        }
        hud->global_notoriety = wanted.global_notoriety;
    }
}

void WantedLevelSystem::handleCrimeReport(const CrimeReportEvent& event) {
    if (!m_registry.valid(event.perpetrator) || !m_registry.all_of<PlayerComponent>(event.perpetrator)) return;

    auto& wanted = m_registry.get_or_emplace<WantedComponent>(event.perpetrator);
    
    // Determine the reporting faction
    std::string reporting_faction = "NEUTRAL";
    
    // If there's a victim, use their faction
    if (m_registry.valid(event.victim) && m_registry.all_of<Layer4PoliticalComponent>(event.victim)) {
        reporting_faction = m_registry.get<Layer4PoliticalComponent>(event.victim).primary_faction;
    } 
    // Otherwise, if it's a property-related crime, use building owner
    else if (m_registry.valid(event.victim) && m_registry.all_of<PropertyComponent>(event.victim)) {
        reporting_faction = m_registry.get<PropertyComponent>(event.victim).owner_faction;
    }
    // Fallback: check the macro-zone influence
    else {
        // Find the macro-zone at the crime location
        // For simplicity, we'll just check if any guard nearby reports it.
        auto guard_view = m_registry.view<PositionComponent, Layer4PoliticalComponent, PatrolComponent>();
        for (auto guard : guard_view) {
            const auto& g_pos = guard_view.get<PositionComponent>(guard);
            if (g_pos.layer_id == event.layer && 
                std::abs(g_pos.x - event.x) < 15 && std::abs(g_pos.y - event.y) < 15) {
                reporting_faction = guard_view.get<Layer4PoliticalComponent>(guard).primary_faction;
                break;
            }
        }
    }

    if (reporting_faction == "NEUTRAL" || reporting_faction.empty()) {
        reporting_faction = "CITY_WATCH"; // Default catch-all
    }

    // Calculate Increase
    int increase = 1;
    float notoriety_gain = 5.0f;

    if (event.crime_type == "THEFT") {
        increase = 1;
        notoriety_gain = 10.0f;
    } else if (event.crime_type == "TRESPASSING") {
        increase = 1;
        notoriety_gain = 5.0f;
    } else if (event.crime_type == "CONTRABAND_POSSESSION") {
        increase = 2;
        notoriety_gain = 15.0f;
    } else if (event.crime_type == "MUGGING") {
        increase = 3;
        notoriety_gain = 25.0f;
    } else if (event.crime_type == "ASSAULT") {
        increase = 3;
        notoriety_gain = 30.0f;
    }

    // Apply Changes
    wanted.faction_wanted_levels[reporting_faction] = std::min(5, wanted.faction_wanted_levels[reporting_faction] + increase);
    wanted.global_notoriety = std::min(100.0f, wanted.global_notoriety + notoriety_gain);

    // [P.2] Reputation Impact
    float reputation_loss = -5.0f * increase;
    m_dispatcher.enqueue<AgentFactionReputationEvent>({
        event.perpetrator,
        reporting_faction,
        reputation_loss
    });

    // Fame increase for being a criminal
    if (m_registry.all_of<ReputationComponent>(event.perpetrator)) {
        auto& rep = m_registry.get<ReputationComponent>(event.perpetrator);
        rep.fame = std::min(1.0f, rep.fame + (notoriety_gain / 1000.0f)); 
    }

    m_dispatcher.enqueue<HUDNotificationEvent>({
        "CRIME REPORTED: " + event.crime_type + " (" + reporting_faction + ")",
        3.0f,
        "#FF0000"
    });
}

void WantedLevelSystem::processDecay() {
    auto view = m_registry.view<WantedComponent>();
    for (auto entity : view) {
        auto& wanted = view.get<WantedComponent>(entity);
        
        // Decay global notoriety
        wanted.global_notoriety = std::max(0.0f, wanted.global_notoriety - 0.5f);

        // Faction wanted level decay
        // The higher the notoriety, the slower the decay.
        float decay_threshold = 0.05f * (1.0f - (wanted.global_notoriety / 100.0f));
        
        std::vector<std::string> factions_to_remove;
        for (auto& [faction, level] : wanted.faction_wanted_levels) {
            if (level > 0) {
                // Random chance to drop a star, influenced by notoriety
                if ((rand() % 1000) < (int)(decay_threshold * 1000.0f)) {
                    level--;
                    if (level == 0) factions_to_remove.push_back(faction);
                }
            }
        }
        
        for (const auto& f : factions_to_remove) {
            wanted.faction_wanted_levels.erase(f);
        }
    }
}

void WantedLevelSystem::updateGuardAlerts() {
    auto player_view = m_registry.view<PlayerComponent, WantedComponent, PositionComponent>();
    for (auto player_ent : player_view) {
        auto& wanted = player_view.get<WantedComponent>(player_ent);
        
        for (auto& [faction, level] : wanted.faction_wanted_levels) {
            if (level >= 3) {
                // High alert: Guards of this faction should actively pursue
                m_dispatcher.enqueue<WantedAlertEvent>({
                    player_ent,
                    faction,
                    level
                });
            }
        }
    }
}

} // namespace NeonOubliette
