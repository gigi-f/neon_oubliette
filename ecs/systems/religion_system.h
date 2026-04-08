#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_RELIGION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_RELIGION_SYSTEM_H

#include <entt/entt.hpp>
#include "../simulation_coordinator.h"
#include "../components/religion_components.h"
#include "../event_declarations.h"

namespace NeonOubliette {

class ReligionSystem : public ISimulationSystem {
public:
    ReligionSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L4_Political; }

    // Event handlers
    void handleWorship(const WorshipEvent& event);
    void handleProselytizing(const ProselytizingEvent& event);
    void handleHolyDay(const HolyDayEvent& event);
    void handleRaid(const RaidEvent& event);

private:
    void loadReligions();
    void updateDevotion();
    void updateInfluence();
    void diffuseInfluence();
    void checkHolyDays();
    void updateProcessions();
    void handleProcessionTension();
    
    void startProcession(const std::string& religion_id, uint32_t holy_day_id);
    std::vector<PositionComponent> generateProcessionRoute(entt::entity start_building);
    void spawnEmergentFaction(const std::string& religion_id);

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    uint64_t m_current_tick = 0;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_RELIGION_SYSTEM_H
