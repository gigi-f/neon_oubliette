#include "simulation_coordinator.h"
#include <iostream>

namespace NeonOubliette {

SimulationCoordinator::SimulationCoordinator(entt::registry& registry, entt::dispatcher& dispatcher, SystemScheduler& scheduler)
    : m_registry(registry), m_dispatcher(dispatcher), m_scheduler(scheduler), m_turn_counter(0) {
    
    // Connect God Mode events
    m_dispatcher.sink<ToggleGodModeEvent>().connect<&SimulationCoordinator::on_toggle_god_mode>(this);
    m_dispatcher.sink<TogglePauseEvent>().connect<&SimulationCoordinator::on_toggle_pause>(this);
    m_dispatcher.sink<AdjustGodModeSpeedEvent>().connect<&SimulationCoordinator::on_adjust_speed>(this);
}

void SimulationCoordinator::advance_turn(double delta_time) {
    // 1. Always Process Input Phase (Every Turn)
    m_scheduler.run_phase(SystemScheduler::Phase::Input, m_registry, m_dispatcher, delta_time);

    auto view = m_registry.view<SimulationStateComponent>();
    if (view.empty()) {
        // Fallback to standard behavior if no state component exists
        run_simulation_tick(delta_time);
    } else {
        auto entity = *view.begin();
        auto& state = view.get<SimulationStateComponent>(entity);

        if (state.mode == SimulationMode::STANDARD) {
            if (!state.is_paused) {
                run_simulation_tick(delta_time);
            }
        } else if (state.mode == SimulationMode::GOD_MODE) {
            if (!state.is_paused) {
                state.accumulator += static_cast<float>(delta_time);
                float interval = 1.0f / state.god_mode_tps;
                if (state.accumulator >= interval) {
                    run_simulation_tick(delta_time);
                    state.accumulator -= interval;
                }
            }
        }
    }

    // 5. Always Process Output Phase (Every Turn)
    m_scheduler.run_phase(SystemScheduler::Phase::Output, m_registry, m_dispatcher, delta_time);

    // 6. Update Dispatcher to handle events triggered during the turn
    m_dispatcher.update();
}

void SimulationCoordinator::run_simulation_tick(double delta_time) {
    m_turn_counter++;

    // 2. Process Simulation Layers (Phase-Locked)
    for (uint8_t i = 0; i < static_cast<uint8_t>(SimulationLayer::Count); ++i) {
        SimulationLayer layer = static_cast<SimulationLayer>(i);
        
        if (should_layer_tick(layer, m_turn_counter)) {
            // Update all systems for this layer
            for (auto& system : m_layer_systems[layer]) {
                system->update(delta_time);
            }
        }
    }

    // 3. Process Macro Phase (Every Turn for non-simulation systems)
    m_scheduler.run_phase(SystemScheduler::Phase::Macro, m_registry, m_dispatcher, delta_time);

    // 4. Process Micro/PostMicro Phases (Every Turn)
    m_scheduler.run_phase(SystemScheduler::Phase::Micro, m_registry, m_dispatcher, delta_time);
    m_scheduler.run_phase(SystemScheduler::Phase::PostMicro, m_registry, m_dispatcher, delta_time);
}

void SimulationCoordinator::on_toggle_god_mode(const ToggleGodModeEvent& event) {
    auto view = m_registry.view<SimulationStateComponent>();
    if (view.empty()) return;
    auto& state = view.get<SimulationStateComponent>(*view.begin());
    
    state.mode = (state.mode == SimulationMode::STANDARD) ? SimulationMode::GOD_MODE : SimulationMode::STANDARD;
    
    // Synchronize God Cursor status
    auto cursor_view = m_registry.view<GodCursorComponent>();
    for (auto entity : cursor_view) {
        auto& cursor = cursor_view.get<GodCursorComponent>(entity);
        cursor.active = (state.mode == SimulationMode::GOD_MODE);
        
        // Snap cursor to player position on activation
        if (cursor.active) {
            auto player_view = m_registry.view<PlayerComponent, PositionComponent, PlayerCurrentLayerComponent>();
            if (player_view.begin() != player_view.end()) {
                auto p_entity = *player_view.begin();
                auto& pos = player_view.get<PositionComponent>(p_entity);
                auto& lay = player_view.get<PlayerCurrentLayerComponent>(p_entity);
                cursor.x = pos.x;
                cursor.y = pos.y;
                cursor.layer_id = lay.current_z;
            }
        }
    }

    m_dispatcher.trigger(HUDNotificationEvent{
        (state.mode == SimulationMode::GOD_MODE) ? "GOD MODE ACTIVATED" : "STANDARD MODE ACTIVATED",
        2.0f,
        "#FFFF00"
    });
}

void SimulationCoordinator::on_toggle_pause(const TogglePauseEvent& event) {
    auto view = m_registry.view<SimulationStateComponent>();
    if (view.empty()) return;
    auto& state = view.get<SimulationStateComponent>(*view.begin());
    state.is_paused = !state.is_paused;

    m_dispatcher.trigger(HUDNotificationEvent{
        state.is_paused ? "SIMULATION PAUSED" : "SIMULATION RESUMED",
        1.5f,
        "#00FF00"
    });
}

void SimulationCoordinator::on_adjust_speed(const AdjustGodModeSpeedEvent& event) {
    auto view = m_registry.view<SimulationStateComponent>();
    if (view.empty()) return;
    auto& state = view.get<SimulationStateComponent>(*view.begin());
    
    state.god_mode_tps = std::max(0.1f, state.god_mode_tps + event.delta);
    
    std::string msg = "SIM SPEED: " + std::to_string(state.god_mode_tps).substr(0, 3) + " TPS";
    m_dispatcher.trigger(HUDNotificationEvent{msg, 1.0f, "#00FFFF"});
}

void SimulationCoordinator::add_simulation_system(std::unique_ptr<ISimulationSystem> system) {
    m_layer_systems[system->simulation_layer()].push_back(std::move(system));
}

void SimulationCoordinator::initialize_all_systems() {
    for (auto& [layer, systems] : m_layer_systems) {
        for (auto& system : systems) {
            system->initialize();
        }
    }
}

bool SimulationCoordinator::should_layer_tick(SimulationLayer layer, uint64_t turn) const {
    switch (layer) {
        case SimulationLayer::L0_Physics:
        case SimulationLayer::L1_Biology:
            return true; // Fast physics/biology every turn
        
        case SimulationLayer::L2_Cognitive:
            return (turn % 5 == 0); // Cognition every 5 turns
            
        case SimulationLayer::L3_Economic:
            return (turn % 10 == 0); // Economics batch every 10 turns
            
        case SimulationLayer::L4_Political:
            return (turn % 20 == 0); // Politics batch every 20 turns
            
        default:
            return false;
    }
}

} // namespace NeonOubliette
