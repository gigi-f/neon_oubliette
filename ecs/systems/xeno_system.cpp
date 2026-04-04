#include "xeno_system.h"
#include <cmath>
#include <algorithm>

namespace NeonOubliette {

XenoSystem::XenoSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {}

void XenoSystem::update(double delta_time) {
    (void)delta_time;
    process_hierodule_auras();
    process_cacogen_interferences();
}

void XenoSystem::process_hierodule_auras() {
    auto view = m_registry.view<PositionComponent, XenoComponent, XenoInfluenceComponent>();
    
    for (auto entity : view) {
        auto [pos, xeno, influence] = view.get<PositionComponent, XenoComponent, XenoInfluenceComponent>(entity);
        
        if (xeno.type == XenoType::HIERODULE || xeno.type == XenoType::POST_HUMAN) {
            auto targets = m_registry.view<PositionComponent, NeedsComponent>();
            for (auto target : targets) {
                const auto& t_pos = targets.get<PositionComponent>(target);
                if (t_pos.layer_id == pos.layer_id) {
                    float dx = (float)(t_pos.x - pos.x);
                    float dy = (float)(t_pos.y - pos.y);
                    float dist = std::sqrt(dx*dx + dy*dy);
                    
                    if (dist <= influence.radius) {
                        auto& needs = targets.get<NeedsComponent>(target);
                        // Positive effect: reduce frustration
                        needs.frustration = std::max(0.0f, needs.frustration + influence.frustration_delta);
                        
                        // Apply temperature stabilization
                        if (auto* p = m_registry.try_get<Layer0PhysicsComponent>(target)) {
                            p->temperature_celsius += influence.temperature_offset * 0.1f; // gradual stabilization
                        }
                    }
                }
            }
        }
    }
}

void XenoSystem::process_cacogen_interferences() {
    auto view = m_registry.view<PositionComponent, XenoComponent, XenoInfluenceComponent>();
    
    for (auto entity : view) {
        auto [pos, xeno, influence] = view.get<PositionComponent, XenoComponent, XenoInfluenceComponent>(entity);
        
        if (xeno.type == XenoType::CACOGEN) {
            auto targets = m_registry.view<PositionComponent, NeedsComponent>();
            for (auto target : targets) {
                const auto& t_pos = targets.get<PositionComponent>(target);
                if (t_pos.layer_id == pos.layer_id) {
                    float dx = (float)(t_pos.x - pos.x);
                    float dy = (float)(t_pos.y - pos.y);
                    float dist = std::sqrt(dx*dx + dy*dy);
                    
                    if (dist <= influence.radius) {
                        auto& needs = targets.get<NeedsComponent>(target);
                        // Negative effect: increase frustration
                        needs.frustration = std::min(100.0f, needs.frustration + influence.frustration_delta);
                    }
                }
            }
        }
    }
}

} // namespace NeonOubliette
