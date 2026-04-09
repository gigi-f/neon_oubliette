#include "system_scheduler.h"
#include "components/components.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

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

    auto phase_it = systems_.find(phase);
    if (phase_it == systems_.end()) return;

    NeonOubliette::DebugOverlayComponent* dbg = nullptr;
    auto dv = registry.view<NeonOubliette::DebugOverlayComponent>();
    if (dv.begin() != dv.end()) {
        dbg = &dv.get<NeonOubliette::DebugOverlayComponent>(*dv.begin());
    }

    float hottest_ms = 0.0f;
    std::string hottest_name;
    auto t_phase_start = std::chrono::steady_clock::now();

    for (auto& ns : phase_it->second) {
        if (dbg) {
            const std::string started_name = ns.name.empty() ? "<unnamed>" : ns.name;
            dbg->current_system = started_name;
            dbg->last_started_phase = phase_to_string(phase);
            dbg->last_started_system = started_name;

            if (phase != Phase::Output) {
                dbg->last_logic_phase = dbg->last_started_phase;
                dbg->last_logic_system = started_name;
            }
        }

        auto t_sys_start = std::chrono::steady_clock::now();
        ns.system->update(delta_time);
        auto t_sys_end = std::chrono::steady_clock::now();

        float ms = std::chrono::duration<float, std::milli>(t_sys_end - t_sys_start).count();
        if (ms > hottest_ms) {
            hottest_ms = ms;
            hottest_name = ns.name.empty() ? "<unnamed>" : ns.name;
        }
    }

    auto t_phase_end = std::chrono::steady_clock::now();
    float phase_ms = std::chrono::duration<float, std::milli>(t_phase_end - t_phase_start).count();

    if (dbg) {
        dbg->current_system.clear();

        auto store_hottest = [&](float& slot_ms, std::string& slot_name) {
            slot_ms = hottest_ms;
            slot_name = hottest_name;
        };

        switch (phase) {
            case Phase::Input:
                dbg->ms_input = phase_ms;
                store_hottest(dbg->ms_hottest_input, dbg->hottest_input_system);
                break;
            case Phase::Macro:
                store_hottest(dbg->ms_hottest_macro, dbg->hottest_macro_system);
                break;
            case Phase::Micro:
                store_hottest(dbg->ms_hottest_micro, dbg->hottest_micro_system);
                break;
            case Phase::PostMicro:
                store_hottest(dbg->ms_hottest_post_micro, dbg->hottest_post_micro_system);
                break;
            case Phase::Output:
                store_hottest(dbg->ms_hottest_output, dbg->hottest_output_system);
                break;
            default:
                break;
        }

        if (!hottest_name.empty()) {
            std::ostringstream oss;
            oss << phase_to_string(phase) << " hot: " << hottest_name << " "
                << std::fixed << std::setprecision(2) << hottest_ms << "ms";
            if (phase == Phase::Input || phase == Phase::Macro) {
                dbg->diag_line1 = oss.str();
            } else if (phase == Phase::Output || phase == Phase::Micro || phase == Phase::PostMicro) {
                dbg->diag_line2 = oss.str();
            }
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
