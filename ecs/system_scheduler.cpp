#include "system_scheduler.h"

#include <algorithm>
#include <iostream>

namespace NeonOubliette {

SystemScheduler::SystemScheduler(entt::registry& registry, entt::dispatcher& event_dispatcher)
    : registry_(registry), event_dispatcher_(event_dispatcher) {
}

void SystemScheduler::add_system(Phase phase, std::unique_ptr<ISystem> system) {
    systems_[phase].push_back(std::move(system));
}

void SystemScheduler::initialize_all_systems() {
    for (auto& [phase, phase_systems] : systems_) {
        for (auto& system : phase_systems) {
            system->initialize();
        }
    }
}

void SystemScheduler::run_phase(Phase phase, entt::registry& registry, entt::dispatcher& event_dispatcher,
                                double delta_time) {
    (void)registry;
    (void)event_dispatcher;

    if (systems_.find(phase) != systems_.end()) {
        for (auto& system : systems_[phase]) {
            system->update(delta_time);
        }
    }
}

const char* SystemScheduler::phase_to_string(Phase phase) {
    switch (phase) {
        case Phase::Input:
            return "Input";
        case Phase::Macro:
            return "Macro";
        case Phase::Micro:
            return "Micro";
        case Phase::PostMicro:
            return "PostMicro";
        case Phase::Output:
            return "Output";
        default:
            return "Unknown";
    }
}

} // namespace NeonOubliette
