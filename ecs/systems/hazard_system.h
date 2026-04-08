#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_HAZARD_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_HAZARD_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include <random>

namespace NeonOubliette {

/**
 * @brief [NEW SYSTEM] [M.4] Manages the temporal and spatial logic of environmental hazards.
 *        Updates active states for intermittent hazards and emits events/modifiers for other systems.
 */
class HazardSystem : public ISimulationSystem {
public:
    HazardSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}

    void update(double delta_time) override {
        (void)delta_time;
        auto view = m_registry.view<TileHazardComponent, RenderableComponent>();
        
        for (auto entity : view) {
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
                apply_active_hazard_effects(entity, hazard);
            }
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L0_Physics;
    }

private:
    void apply_active_hazard_effects(entt::entity hazard_ent, const TileHazardComponent& hazard) {
        const auto& pos = m_registry.get<PositionComponent>(hazard_ent);
        
        // 1. Single-tile Physics (Heat)
        if (hazard.type == HazardType::STEAM_VENT || hazard.type == HazardType::FIRE) {
            if (auto* phys = m_registry.try_get<Layer0PhysicsComponent>(hazard_ent)) {
                phys->temperature_celsius += hazard.intensity * 20.0f;
            }
        }

        // 2. Proximity Biology Effects (L1)
        auto agent_view = m_registry.view<PositionComponent, Layer1BiologyComponent>();
        for (auto agent_ent : agent_view) {
            const auto& a_pos = agent_view.get<PositionComponent>(agent_ent);
            if (a_pos.layer_id != pos.layer_id) continue;

            int dx = std::abs(a_pos.x - pos.x);
            int dy = std::abs(a_pos.y - pos.y);
            int dist = std::max(dx, dy); // Chebyshev distance

            if (dist <= (int)hazard.radius) {
                auto& bio = agent_view.get<Layer1BiologyComponent>(agent_ent);
                // Falloff for radius > 0
                float effective_intensity = hazard.intensity;
                if (hazard.radius > 0) effective_intensity *= (1.0f - (float)dist / (hazard.radius + 1.0f));
                
                apply_biological_impact(agent_ent, bio, hazard, effective_intensity);
            }
        }
    }

    void apply_biological_impact(entt::entity agent_ent, Layer1BiologyComponent& bio, const TileHazardComponent& hazard, float effective_intensity) {
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
                        m_registry.emplace<InfectionComponent>(agent_ent, 0.0f, 0.05f, 0.7f, 0ULL, true);
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
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_HAZARD_SYSTEM_H
