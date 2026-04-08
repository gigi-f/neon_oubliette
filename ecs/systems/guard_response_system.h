#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_GUARD_RESPONSE_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_GUARD_RESPONSE_SYSTEM_H

#include <entt/entt.hpp>
#include "../event_declarations.h"
#include "../system_scheduler.h"
#include "../components/components.h"

namespace NeonOubliette {

/**
 * @brief [I.6] Coordinates guard reactions to crimes and wanted status.
 *        Manages pursuit, investigation, and arrest cycles.
 */
class GuardResponseSystem : public ISystem {
public:
    GuardResponseSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {}
    void update(double delta_time) override;

    void handleCrimeReport(const CrimeReportEvent& event);
    void handleWantedAlert(const WantedAlertEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    uint32_t m_nextPathRequestId = 20000;

    void processPursue(entt::entity guard, entt::entity target, AgentTaskComponent& task, PositionComponent& pos);
    void processInvestigate(entt::entity guard, AgentTaskComponent& task, PositionComponent& pos);
    void processArrest(entt::entity guard, entt::entity target, AgentTaskComponent& task);

    // Helper: Find a holding cell building
    entt::entity findNearestHoldingCell(const PositionComponent& pos);
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_GUARD_RESPONSE_SYSTEM_H
