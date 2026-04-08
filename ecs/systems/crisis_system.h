#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CRISIS_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CRISIS_SYSTEM_H

#include "../system_scheduler.h"
#include <entt/entt.hpp>
#include "../components/components.h"

namespace NeonOubliette::Systems {

/**
 * @brief [L.1] Manages the lifecycle of macro-scale crises in the city.
 *        Monitors systemic stress and triggers/resolves crisis events.
 *        Registered in Phase::Macro to ensure it ticks every simulation step.
 */
class CrisisSystem : public ISystem {
public:
    CrisisSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    ~CrisisSystem() override = default;

    void initialize() override;
    void update(double delta_time) override;

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    void process_active_crises(CrisisComponent& crisis_comp);
    void evaluate_new_crises(CrisisComponent& crisis_comp);
    
    float evaluate_economic_stress();
    float evaluate_political_stress();
    
    void trigger_crisis(CrisisComponent& crisis_comp, CrisisType type, float severity, uint32_t duration, const std::string& description);
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_CRISIS_SYSTEM_H
