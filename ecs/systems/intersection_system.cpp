#include "intersection_system.h"
#include "../components/infrastructure_components.h"
#include "../components/components.h"

namespace NeonOubliette::Systems {

IntersectionSystem::IntersectionSystem(entt::registry& registry, entt::dispatcher& event_dispatcher)
    : m_registry(registry), m_dispatcher(event_dispatcher) {}

void IntersectionSystem::update(double delta_time) {
    update_traffic_lights(delta_time);
    handle_unprotected_intersections();
}

void IntersectionSystem::update_traffic_lights(double delta_time) {
    auto view = m_registry.view<TrafficLightComponent>();
    uint32_t delta_ticks = static_cast<uint32_t>(delta_time * 60.0); // Assume 60 ticks per sec for simplicity

    for (auto entity : view) {
        auto& light = view.get<TrafficLightComponent>(entity);
        light.timer += delta_ticks;

        if (light.timer >= light.cycle_duration) {
            light.timer = 0;
            switch (light.state) {
                case TrafficLightState::RED:
                    light.state = TrafficLightState::GREEN;
                    break;
                case TrafficLightState::YELLOW:
                    light.state = TrafficLightState::RED;
                    break;
                case TrafficLightState::GREEN:
                    light.state = TrafficLightState::YELLOW;
                    // Yellow is usually shorter
                    light.cycle_duration = 50; 
                    break;
            }
            
            // Reset durations
            if (light.state == TrafficLightState::RED || light.state == TrafficLightState::GREEN) {
                light.cycle_duration = 200; // Standard duration
            }
        }
    }
}

void IntersectionSystem::handle_unprotected_intersections() {
    // Basic FIFO logic would require tracking wait times per vehicle at an intersection.
    // For now, we'll implement a simpler "check for space" in TrafficFlowSystem,
    // and this system could eventually manage more complex queueing.
    // (Placeholder for advanced FIFO logic)
}

} // namespace NeonOubliette::Systems
