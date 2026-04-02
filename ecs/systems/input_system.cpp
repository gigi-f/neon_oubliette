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
    while ((key_id = notcurses_get(m_ncContext, &ts, &input)) > 0) {
        // 1. Global Commands
        if (input.evtype == NCTYPE_PRESS || input.evtype == NCTYPE_UNKNOWN) {
            if (key_id == 'q' || key_id == 'Q') {
                m_dispatcher.trigger<NeonOubliette::ShutdownEvent>();
                return;
            }
        }

        // 2. Player-Dependent Commands
        if (input.evtype == NCTYPE_PRESS || input.evtype == NCTYPE_UNKNOWN) {
            auto player_view = m_registry.view<PlayerComponent, PositionComponent>();
            entt::entity player_entity = entt::null;
            for (auto entity : player_view) {
                player_entity = entity;
                break;
            }

            if (player_entity == entt::null)
                continue;
            
            auto& pos = player_view.get<PositionComponent>(player_entity);

            // Movement (Horizontal)
            if (key_id == 'w' || key_id == 'W') {
                m_dispatcher.trigger(MoveEvent{player_entity, 0, -1, pos.layer_id});
                m_dispatcher.trigger<AdvanceTurnRequestEvent>();
            } else if (key_id == 's' || key_id == 'S') {
                m_dispatcher.trigger(MoveEvent{player_entity, 0, 1, pos.layer_id});
                m_dispatcher.trigger<AdvanceTurnRequestEvent>();
            } else if (key_id == 'a' || key_id == 'A') {
                m_dispatcher.trigger(MoveEvent{player_entity, -1, 0, pos.layer_id});
                m_dispatcher.trigger<AdvanceTurnRequestEvent>();
            } else if (key_id == 'd' || key_id == 'D') {
                m_dispatcher.trigger(MoveEvent{player_entity, 1, 0, pos.layer_id});
                m_dispatcher.trigger<AdvanceTurnRequestEvent>();
            } else if (key_id == ' ') {
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
            
            // Inspection
            else if (key_id == 'i') { // Surface Scan (Adjacent or Current)
                m_dispatcher.trigger(InspectEvent{player_entity, pos.layer_id, pos.x, pos.y, InspectionMode::SURFACE_SCAN});
            } else if (key_id == 'I') { // Biological Audit (Current for now, could be direction later)
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
}

} // namespace Systems
} // namespace NeonOubliette
