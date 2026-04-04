#include "input_system.h"
#include "../components/components.h"
#include "../event_declarations.h"
#include <cstdlib>

namespace NeonOubliette {
namespace Systems {

InputSystem::InputSystem(entt::registry& registry, struct notcurses* nc_context, entt::dispatcher& dispatcher)
    : m_registry(registry), m_ncContext(nc_context), m_dispatcher(dispatcher) {
}

void InputSystem::initialize() {
}

void InputSystem::update(double delta_time) {
    ncinput input;
    struct timespec ts = {0, 0};
    uint32_t key_id;

    // Get Simulation State
    SimulationMode current_mode = SimulationMode::STANDARD;
    auto state_view = m_registry.view<SimulationStateComponent>();
    if (!state_view.empty()) {
        auto& state = state_view.get<SimulationStateComponent>(*state_view.begin());
        current_mode = state.mode;
    }

    while ((key_id = notcurses_get(m_ncContext, &ts, &input)) > 0) {
        if (input.evtype != NCTYPE_PRESS && input.evtype != NCTYPE_UNKNOWN) continue;

        // 1. Global Commands
        if (key_id == 'q' || key_id == 'Q') {
            m_dispatcher.trigger<NeonOubliette::ShutdownEvent>();
            return;
        }

        // Toggle God Mode
        if (key_id == 'g' || key_id == 'G') {
            m_dispatcher.trigger<ToggleGodModeEvent>();
            continue;
        }

        // Toggle Pause
        if (key_id == 'p' || key_id == 'P' || (current_mode == SimulationMode::GOD_MODE && key_id == ' ')) {
            m_dispatcher.trigger<TogglePauseEvent>();
            continue;
        }

        // Adjust Speed (God Mode Only)
        if (current_mode == SimulationMode::GOD_MODE) {
            if (key_id == '+' || key_id == '=') {
                m_dispatcher.trigger(AdjustGodModeSpeedEvent{0.5f});
                continue;
            } else if (key_id == '-' || key_id == '_') {
                m_dispatcher.trigger(AdjustGodModeSpeedEvent{-0.5f});
                continue;
            }
        }

        // 2. Mode-Specific Commands
        if (current_mode == SimulationMode::GOD_MODE) {
            auto cursor_view = m_registry.view<GodCursorComponent>();
            if (!cursor_view.empty()) {
                auto& cursor = cursor_view.get<GodCursorComponent>(*cursor_view.begin());
                
                if (key_id == 'w' || key_id == 'W' || key_id == NCKEY_UP) cursor.y--;
                else if (key_id == 's' || key_id == 'S' || key_id == NCKEY_DOWN) cursor.y++;
                else if (key_id == 'a' || key_id == 'A' || key_id == NCKEY_LEFT) cursor.x--;
                else if (key_id == 'd' || key_id == 'D' || key_id == NCKEY_RIGHT) cursor.x++;
                
                // Vertical Navigation in God Mode
                else if (key_id == '>') cursor.layer_id++;
                else if (key_id == '<') cursor.layer_id--;
                
                // UI Toggles
                else if (key_id == NCKEY_ESC) {
                    m_dispatcher.trigger<CloseInspectionWindowEvent>();
                }

                // Inspection in God Mode (uses cursor pos)
                if (key_id == 'i') m_dispatcher.trigger(InspectEvent{entt::null, cursor.layer_id, cursor.x, cursor.y, InspectionMode::SURFACE_SCAN});
                else if (key_id == 'I') m_dispatcher.trigger(InspectEvent{entt::null, cursor.layer_id, cursor.x, cursor.y, InspectionMode::BIOLOGICAL_AUDIT});
                else if (key_id == 'c') m_dispatcher.trigger(InspectEvent{entt::null, cursor.layer_id, cursor.x, cursor.y, InspectionMode::COGNITIVE_PROFILE});
                else if (key_id == 'f') m_dispatcher.trigger(InspectEvent{entt::null, cursor.layer_id, cursor.x, cursor.y, InspectionMode::FINANCIAL_FORENSICS});
                else if (key_id == 't') m_dispatcher.trigger(InspectEvent{entt::null, cursor.layer_id, cursor.x, cursor.y, InspectionMode::STRUCTURAL_ANALYSIS});
            }
            continue;
        }

        // STANDARD MODE (Player-centric)
        auto player_view = m_registry.view<PlayerComponent, PositionComponent>();
        entt::entity player_entity = entt::null;
        for (auto entity : player_view) {
            player_entity = entity;
            break;
        }

        if (player_entity == entt::null)
            continue;
        
        auto& pos = player_view.get<PositionComponent>(player_entity);

        // Inventory UI Focus
        if (m_registry.all_of<HUDComponent>(player_entity)) {
            auto& hud = m_registry.get<HUDComponent>(player_entity);
            if (hud.inventory_open) {
                if (key_id == NCKEY_UP) {
                    hud.selected_inventory_index = std::max(0, hud.selected_inventory_index - 1);
                    continue;
                } else if (key_id == NCKEY_DOWN) {
                    if (m_registry.all_of<InventoryComponent>(player_entity)) {
                        auto& inv = m_registry.get<InventoryComponent>(player_entity);
                        if (!inv.contained_items.empty()) {
                            hud.selected_inventory_index = std::min(static_cast<int>(inv.contained_items.size()) - 1, hud.selected_inventory_index + 1);
                        }
                    }
                    continue;
                } else if (key_id == NCKEY_ENTER || key_id == '\r' || key_id == '\n') {
                    if (m_registry.all_of<InventoryComponent>(player_entity)) {
                        auto& inv = m_registry.get<InventoryComponent>(player_entity);
                        if (!inv.contained_items.empty() && hud.selected_inventory_index < (int)inv.contained_items.size()) {
                            entt::entity item_ent = inv.contained_items[hud.selected_inventory_index];
                            m_dispatcher.trigger(UseItemEvent{player_entity, item_ent});
                            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
                        }
                    }
                    continue;
                } else if (key_id == 'b' || key_id == 'B') {
                    m_dispatcher.trigger(InventoryToggleEvent{player_entity});
                    continue;
                } else if (key_id == NCKEY_ESC) {
                    m_dispatcher.trigger(InventoryToggleEvent{player_entity});
                    continue;
                }
                // Swallow other keys if inventory is open
                continue;
            }
        }

        // Determine target for movement (Direct or via Personal Vehicle)
        entt::entity move_target = player_entity;
        if (m_registry.all_of<RidingComponent>(player_entity)) {
            auto vehicle = m_registry.get<RidingComponent>(player_entity).vehicle;
            if (m_registry.valid(vehicle) && m_registry.all_of<PersonalVehicleComponent>(vehicle)) {
                if (m_registry.get<PersonalVehicleComponent>(vehicle).driver == player_entity) {
                    move_target = vehicle;
                }
            }
        }

        // Movement (Horizontal)
        if (key_id == 'w' || key_id == 'W' || key_id == NCKEY_UP) {
            m_dispatcher.trigger(MoveEvent{move_target, 0, -1, pos.layer_id});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
        } else if (key_id == 's' || key_id == 'S' || key_id == NCKEY_DOWN) {
            m_dispatcher.trigger(MoveEvent{move_target, 0, 1, pos.layer_id});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
        } else if (key_id == 'a' || key_id == 'A' || key_id == NCKEY_LEFT) {
            m_dispatcher.trigger(MoveEvent{move_target, -1, 0, pos.layer_id});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
        } else if (key_id == 'd' || key_id == 'D' || key_id == NCKEY_RIGHT) {
            m_dispatcher.trigger(MoveEvent{move_target, 1, 0, pos.layer_id});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
        } else if (key_id == ' ') {
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
        }
        
        // Interaction
        else if (key_id == 'e' || key_id == 'E') {
            m_dispatcher.trigger(InteractEvent{player_entity});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
        }

        // Movement (Vertical)
        else if (key_id == '>') {
            m_dispatcher.trigger(PlayerLayerChangeEvent{1});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
        } else if (key_id == '<') {
            m_dispatcher.trigger(PlayerLayerChangeEvent{-1});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
        }
        
        // UI Toggles
        else if (key_id == '?') {
            m_dispatcher.trigger(ToggleControlsHelpEvent{player_entity});
        } else if (key_id == 'b' || key_id == 'B') {
            m_dispatcher.trigger(InventoryToggleEvent{player_entity});
        } else if (key_id == NCKEY_ESC) {
            m_dispatcher.trigger<CloseInspectionWindowEvent>();
        }

        // Inspection
        else if (key_id == 'i') { // Surface Scan
            m_dispatcher.trigger(InspectEvent{player_entity, pos.layer_id, pos.x, pos.y, InspectionMode::SURFACE_SCAN});
        } else if (key_id == 'I') { // Biological Audit
            m_dispatcher.trigger(InspectEvent{player_entity, pos.layer_id, pos.x, pos.y, InspectionMode::BIOLOGICAL_AUDIT});
        } else if (key_id == 'c') { // Cognitive Profile
            m_dispatcher.trigger(InspectEvent{player_entity, pos.layer_id, pos.x, pos.y, InspectionMode::COGNITIVE_PROFILE});
        } else if (key_id == 'f') { // Financial Forensics
            m_dispatcher.trigger(InspectEvent{player_entity, pos.layer_id, pos.x, pos.y, InspectionMode::FINANCIAL_FORENSICS});
        } else if (key_id == 't') { // Structural Analysis
            m_dispatcher.trigger(InspectEvent{player_entity, pos.layer_id, pos.x, pos.y, InspectionMode::STRUCTURAL_ANALYSIS});
        }
    }
}

} // namespace Systems
} // namespace NeonOubliette
