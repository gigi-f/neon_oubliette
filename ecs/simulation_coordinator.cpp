#include "simulation_coordinator.h"
#include <iostream>
#include <chrono>

namespace NeonOubliette {

SimulationCoordinator::SimulationCoordinator(entt::registry& registry, entt::dispatcher& dispatcher, SystemScheduler& scheduler)
    : m_registry(registry), m_dispatcher(dispatcher), m_scheduler(scheduler), m_turn_counter(0) {
    
    // Connect God Mode events
    m_dispatcher.sink<ToggleGodModeEvent>().connect<&SimulationCoordinator::on_toggle_god_mode>(this);
    m_dispatcher.sink<TogglePauseEvent>().connect<&SimulationCoordinator::on_toggle_pause>(this);
    m_dispatcher.sink<AdjustGodModeSpeedEvent>().connect<&SimulationCoordinator::on_adjust_speed>(this);
    m_dispatcher.sink<GodModeFocusBuildingEvent>().connect<&SimulationCoordinator::on_focus_building>(this);
    m_dispatcher.sink<GodModeExitFocusEvent>().connect<&SimulationCoordinator::on_exit_focus>(this);
    m_dispatcher.sink<AdvanceTurnRequestEvent>().connect<&SimulationCoordinator::on_advance_turn_request>(this);
}

void SimulationCoordinator::advance_turn(double delta_time) {
    // Helper to get/create debug overlay
    auto get_debug = [&]() -> DebugOverlayComponent* {
        auto dv = m_registry.view<DebugOverlayComponent>();
        if (dv.begin() != dv.end()) return &dv.get<DebugOverlayComponent>(*dv.begin());
        return nullptr;
    };

    // 1. Always Process Input Phase (Every Turn)
    if (auto* dbg = get_debug()) { dbg->current_phase = "Input"; }
    auto t_input_start = std::chrono::steady_clock::now();
    m_scheduler.run_phase(SystemScheduler::Phase::Input, m_registry, m_dispatcher, delta_time);
    auto t_input_end = std::chrono::steady_clock::now();
    if (auto* dbg = get_debug()) {
        dbg->ms_input = std::chrono::duration<float, std::milli>(t_input_end - t_input_start).count();
    }

    bool ran_sim_tick = false;

    auto view = m_registry.view<SimulationStateComponent>();
    if (view.empty()) {
        // Fallback to standard behavior if no state component exists
        run_simulation_tick(delta_time);
        ran_sim_tick = true;
    } else {
        auto entity = *view.begin();
        auto& state = view.get<SimulationStateComponent>(entity);

        if (state.mode == SimulationMode::STANDARD) {
            if (!state.is_paused && m_turn_requested) {
                m_turn_requested = false;
                run_simulation_tick(delta_time);
                ran_sim_tick = true;
            }
        } else if (state.mode == SimulationMode::GOD_MODE) {
            if (!state.is_paused) {
                state.accumulator += static_cast<float>(delta_time);
                float interval = 1.0f / state.god_mode_tps;
                if (state.accumulator >= interval) {
                    run_simulation_tick(delta_time);
                    state.accumulator -= interval;
                    ran_sim_tick = true;
                }
            }
        }
    }

    // 5. Process Output at a fixed cadence, or immediately after simulation tick.
    m_output_accumulator += static_cast<float>(delta_time);
    const bool should_render = ran_sim_tick || (m_output_accumulator >= kOutputIntervalSeconds);
    if (should_render) {
        if (auto* dbg = get_debug()) { dbg->current_phase = "Output"; }
        auto t_output_start = std::chrono::steady_clock::now();
        m_scheduler.run_phase(SystemScheduler::Phase::Output, m_registry, m_dispatcher, delta_time);
        auto t_output_end = std::chrono::steady_clock::now();
        if (auto* dbg = get_debug()) {
            dbg->ms_output = std::chrono::duration<float, std::milli>(t_output_end - t_output_start).count();
            if (!ran_sim_tick) {
                // Ignore cadence-only render passes for "hottest output" reporting.
                dbg->ms_hottest_output = 0.0f;
                dbg->hottest_output_system.clear();
            }
        }
        m_output_accumulator = 0.0f;
    } else if (auto* dbg = get_debug()) {
        dbg->ms_output = 0.0f;
    }

    // 6. Update Dispatcher to handle events triggered during the turn
    if (auto* dbg = get_debug()) { dbg->current_phase = "Dispatch"; }
    auto t_disp_start = std::chrono::steady_clock::now();
    m_dispatcher.update();
    auto t_disp_end = std::chrono::steady_clock::now();
    if (auto* dbg = get_debug()) {
        dbg->ms_dispatch = std::chrono::duration<float, std::milli>(t_disp_end - t_disp_start).count();
        dbg->current_phase = "idle";
    }
}

void SimulationCoordinator::run_simulation_tick(double delta_time) {
    m_turn_counter++;

    auto get_debug = [&]() -> DebugOverlayComponent* {
        auto dv = m_registry.view<DebugOverlayComponent>();
        if (dv.begin() != dv.end()) return &dv.get<DebugOverlayComponent>(*dv.begin());
        return nullptr;
    };

    auto t_tick_start = std::chrono::steady_clock::now();

    // Update entity counts for debug overlay
    if (auto* dbg = get_debug()) {
        dbg->turn = m_turn_counter;
        auto npc_v = m_registry.view<NPCComponent>();
        dbg->num_agents = npc_v.size();
    }

    // 2. Process Simulation Layers (Phase-Locked)
    if (auto* dbg = get_debug()) { dbg->current_phase = "SimLayers"; }
    auto t0 = std::chrono::steady_clock::now();
    for (uint8_t i = 0; i < static_cast<uint8_t>(SimulationLayer::Count); ++i) {
        SimulationLayer layer = static_cast<SimulationLayer>(i);
        
        if (should_layer_tick(layer, m_turn_counter)) {
            for (auto& system : m_layer_systems[layer]) {
                system->update(delta_time);
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    if (auto* dbg = get_debug()) {
        dbg->ms_sim_layers = std::chrono::duration<float, std::milli>(t1 - t0).count();
    }

    // 3. Process Macro Phase (Every Turn for non-simulation systems)
    if (auto* dbg = get_debug()) { dbg->current_phase = "Macro"; }
    auto t2 = std::chrono::steady_clock::now();
    m_scheduler.run_phase(SystemScheduler::Phase::Macro, m_registry, m_dispatcher, delta_time);
    auto t3 = std::chrono::steady_clock::now();
    if (auto* dbg = get_debug()) {
        dbg->ms_macro = std::chrono::duration<float, std::milli>(t3 - t2).count();
    }

    // 4. Process Micro/PostMicro Phases (Every Turn)
    if (auto* dbg = get_debug()) { dbg->current_phase = "Micro"; }
    auto t4 = std::chrono::steady_clock::now();
    m_scheduler.run_phase(SystemScheduler::Phase::Micro, m_registry, m_dispatcher, delta_time);
    auto t5 = std::chrono::steady_clock::now();
    if (auto* dbg = get_debug()) {
        dbg->ms_micro = std::chrono::duration<float, std::milli>(t5 - t4).count();
    }

    if (auto* dbg = get_debug()) { dbg->current_phase = "PostMicro"; }
    auto t6 = std::chrono::steady_clock::now();
    m_scheduler.run_phase(SystemScheduler::Phase::PostMicro, m_registry, m_dispatcher, delta_time);
    auto t7 = std::chrono::steady_clock::now();
    if (auto* dbg = get_debug()) {
        dbg->ms_post_micro = std::chrono::duration<float, std::milli>(t7 - t6).count();
        dbg->ms_total_tick = std::chrono::duration<float, std::milli>(t7 - t_tick_start).count();
        dbg->current_phase = "idle";
    }
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

void SimulationCoordinator::on_focus_building(const GodModeFocusBuildingEvent& event) {
    auto view = m_registry.view<SimulationStateComponent>();
    if (view.empty()) return;
    auto& state = view.get<SimulationStateComponent>(*view.begin());

    if (state.mode != SimulationMode::GOD_MODE) return;

    state.focused_building = event.building_entity;
    state.is_inside_view = true;
    state.focus_floor = 0;

    // Trigger interior generation if needed
    m_dispatcher.trigger(BuildingEntranceEvent{entt::null, event.building_entity, entt::null, 0, 0, 0});

    m_dispatcher.trigger(HUDNotificationEvent{"Inspecting Interior...", 2.0f, "#00FF00"});
}

void SimulationCoordinator::on_exit_focus(const GodModeExitFocusEvent& event) {
    auto view = m_registry.view<SimulationStateComponent>();
    if (view.empty()) return;
    auto& state = view.get<SimulationStateComponent>(*view.begin());

    state.is_inside_view = false;
    state.focused_building = entt::null;

    m_dispatcher.trigger(HUDNotificationEvent{"Returning to Overworld", 1.5f, "#AAAAAA"});
}

void SimulationCoordinator::on_advance_turn_request(const AdvanceTurnRequestEvent& event) {
    (void)event;
    m_turn_requested = true;
}

} // namespace NeonOubliette
