#include "system_scheduler.h"
#include "components/components.h"

#include <algorithm>
#include <iostream>

namespace NeonOubliette {

SystemScheduler::SystemScheduler(entt::registry& registry, entt::dispatcher& event_dispatcher)
    : registry_(registry), event_dispatcher_(event_dispatcher) {
}

void SystemScheduler::add_system(Phase phase, std::unique_ptr<ISystem> system, std::string name) {
    systems_[phase].push_back({std::move(name), std::move(system)});
}

void SystemScheduler::initialize_all_systems() {
    for (auto& [phase, phase_systems] : systems_) {
        for (auto& ns : phase_systems) {
            ns.system->initialize();
        }
    }
}

void SystemScheduler::run_phase(Phase phase, entt::registry& registry, entt::dispatcher& event_dispatcher,
                                double delta_time) {
    (void)event_dispatcher;

    // Grab the debug overlay component (may be null before player entity is created)
    auto get_debug = [&]() -> NeonOubliette::DebugOverlayComponent* {
        auto dv = registry.view<NeonOubliette::DebugOverlayComponent>();
        if (dv.begin() != dv.end()) return &dv.get<NeonOubliette::DebugOverlayComponent>(*dv.begin());
        return nullptr;
    };

    if (systems_.find(phase) != systems_.end()) {
        for (auto& ns : systems_[phase]) {
            if (!ns.name.empty()) {
                if (auto* dbg = get_debug()) dbg->current_system = ns.name;
            }
            ns.system->update(delta_time);
        }
        if (auto* dbg = get_debug()) dbg->current_system.clear();
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
