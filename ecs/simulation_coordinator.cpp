#include "simulation_coordinator.h"
#include <iostream>

namespace NeonOubliette {

SimulationCoordinator::SimulationCoordinator(entt::registry& registry, entt::dispatcher& dispatcher, SystemScheduler& scheduler)
    : m_registry(registry), m_dispatcher(dispatcher), m_scheduler(scheduler), m_turn_counter(0) {
}

void SimulationCoordinator::advance_turn(double delta_time) {
    m_turn_counter++;

    // 1. Process Input Phase (Every Turn)
    m_scheduler.run_phase(SystemScheduler::Phase::Input, m_registry, m_dispatcher, delta_time);

    // 2. Process Simulation Layers (Phase-Locked)
    // Enforces the causality hierarchy: Macro (L4) -> Micro (L0) is standard for many top-down systems
    // but often bottom-up (L0 -> L4) is used for physics-based causality.
    // Here we iterate through layers.
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

    // 5. Output Phase (Every Turn)
    m_scheduler.run_phase(SystemScheduler::Phase::Output, m_registry, m_dispatcher, delta_time);

    // 6. Update Dispatcher to handle events triggered during the turn
    m_dispatcher.update();
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
