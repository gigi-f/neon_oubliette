#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CRISIS_DASHBOARD_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CRISIS_DASHBOARD_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"
#include "../components/components.h"
#include "../event_declarations.h"

namespace NeonOubliette::Systems {

/**
 * @brief [L.6] Aggregates crisis data and simulation stress metrics for the God Mode dashboard.
 *        Registered in Phase::Macro.
 */
class CrisisDashboardSystem : public ISystem {
public:
    CrisisDashboardSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    ~CrisisDashboardSystem() override = default;

    void initialize() override;
    void update(double delta_time) override;

    void handleToggleCrisisDashboard(const ToggleCrisisDashboardEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    void update_history(CrisisDashboardComponent& dash, const CrisisComponent& crisis);
    void update_propagation_vectors(CrisisDashboardComponent& dash, const CrisisComponent& crisis);
    
    float evaluate_biological_stress();
    float evaluate_environmental_stress();
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_CRISIS_DASHBOARD_SYSTEM_H
