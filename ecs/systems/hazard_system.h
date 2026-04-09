#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_HAZARD_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_HAZARD_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include <random>
#include <vector>
#include <algorithm>

namespace NeonOubliette {

/**
 * @brief [NEW SYSTEM] [M.4] Manages the temporal and spatial logic of environmental hazards.
 *        Updates active states for intermittent hazards and emits events/modifiers for other systems.
 */
class HazardSystem : public ISimulationSystem {
public:
    HazardSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
        // Pre-create storage so first infection application doesn't create a pool
        // during active hazard iteration.
        (void)m_registry.storage<InfectionComponent>();
    }

    void update(double delta_time) override {
        (void)delta_time;
        m_pending_infections.clear();
        auto view = m_registry.view<PositionComponent, TileHazardComponent, RenderableComponent>();
        
        for (auto entity : view) {
            if (!m_registry.valid(entity)) continue;

            const auto& pos = view.get<PositionComponent>(entity);
            auto& hazard = view.get<TileHazardComponent>(entity);
            auto& render = view.get<RenderableComponent>(entity);

            // 1. Pulsing/Temporal Logic
            if (hazard.is_intermittent) {
                if (hazard.ticks_to_next_pulse > 0) {
                    hazard.ticks_to_next_pulse--;
                } else {
                    hazard.is_active = !hazard.is_active;
                    hazard.ticks_to_next_pulse = hazard.pulse_rate;
                    
                    // Visual feedback for pulsing
                    if (hazard.is_active) {
                        // Restore original color or brighten
                        if (hazard.type == HazardType::ELECTRICAL) render.color = "#FFFF00";
                        else if (hazard.type == HazardType::STEAM_VENT) render.color = "#FFFFFF";
                    } else {
                        // Dim or hide
                        render.color = "#222222";
                    }
                }
            }

            // 2. Localized Effects (Applying to neighbors or self)
            if (hazard.is_active) {
                apply_active_hazard_effects(entity, pos, hazard);
            }
        }

        if (!m_pending_infections.empty()) {
            std::sort(m_pending_infections.begin(), m_pending_infections.end());
            m_pending_infections.erase(std::unique(m_pending_infections.begin(), m_pending_infections.end()),
                                       m_pending_infections.end());

            for (auto agent_ent : m_pending_infections) {
                if (!m_registry.valid(agent_ent)) continue;
                if (m_registry.all_of<InfectionComponent>(agent_ent)) continue;
                m_registry.emplace<InfectionComponent>(agent_ent, 0.0f, 0.05f, 0.7f, 0ULL, true);
            }
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L0_Physics;
    }

private:
    void apply_active_hazard_effects(entt::entity hazard_ent, const PositionComponent& pos, const TileHazardComponent& hazard) {
        if (!m_registry.valid(hazard_ent)) return;
        float hazard_radius = std::max(0.0f, hazard.radius);
        
        // 1. Single-tile Physics (Heat)
        if (hazard.type == HazardType::STEAM_VENT || hazard.type == HazardType::FIRE) {
            if (auto* phys = m_registry.try_get<Layer0PhysicsComponent>(hazard_ent)) {
                phys->temperature_celsius += hazard.intensity * 20.0f;
            }
        }

        // 2. Proximity Biology Effects (L1)
        auto agent_view = m_registry.view<PositionComponent, Layer1BiologyComponent>();
        for (auto agent_ent : agent_view) {
            if (!m_registry.valid(agent_ent)) continue;
            const auto& a_pos = agent_view.get<PositionComponent>(agent_ent);
            if (a_pos.layer_id != pos.layer_id) continue;

            int dx = std::abs(a_pos.x - pos.x);
            int dy = std::abs(a_pos.y - pos.y);
            int dist = std::max(dx, dy); // Chebyshev distance

            if ((float)dist <= hazard_radius) {
                auto& bio = agent_view.get<Layer1BiologyComponent>(agent_ent);
                // Falloff for radius > 0
                float effective_intensity = hazard.intensity;
                if (hazard_radius > 0.0f) effective_intensity *= (1.0f - (float)dist / (hazard_radius + 1.0f));
                
                apply_biological_impact(agent_ent, bio, hazard, effective_intensity);
            }
        }
    }

    void apply_biological_impact(entt::entity agent_ent, Layer1BiologyComponent& bio, const TileHazardComponent& hazard, float effective_intensity) {
        if (!m_registry.valid(agent_ent)) return;
        switch(hazard.type) {
            case HazardType::TOXIC_GAS:
                bio.pain_level = std::min(10, bio.pain_level + (int)(effective_intensity * 2.0f));
                bio.consciousness_level -= effective_intensity * 0.05f;
                break;
            case HazardType::ELECTRICAL:
                bio.pain_level = 10;
                bio.consciousness_level -= effective_intensity * 0.2f;
                bio.vitals.heart_rate += 30.0f * effective_intensity;
                break;
            case HazardType::STEAM_VENT:
                bio.pain_level = std::min(10, bio.pain_level + (int)(effective_intensity * 3.0f));
                bio.vitals.core_temperature += 0.5f * effective_intensity;
                break;
            case HazardType::BIO_HAZARD:
                // Chance to infect
                if ((double)rand() / (double)RAND_MAX < (double)effective_intensity * 0.1) {
                    if (!m_registry.all_of<InfectionComponent>(agent_ent)) {
                        // Defer mutation until after view iteration to avoid
                        // invalidating internal view/pool pointers.
                        m_pending_infections.push_back(agent_ent);
                    }
                }
                break;
            case HazardType::RAD_ZONE:
                if (auto* age = m_registry.try_get<AgeComponent>(agent_ent)) {
                    age->biological_wear += effective_intensity * 0.01f;
                }
                break;
            default: break;
        }
    }

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    std::vector<entt::entity> m_pending_infections;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_HAZARD_SYSTEM_H
