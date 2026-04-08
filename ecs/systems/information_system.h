#ifndef NEON_OUBLIETTE_INFORMATION_SYSTEM_H
#define NEON_OUBLIETTE_INFORMATION_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../simulation_coordinator.h"
#include "../event_declarations.h"

namespace NeonOubliette::Systems {

class InformationSystem : public NeonOubliette::ISimulationSystem {
public:
    InformationSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L4_Political; }

    /**
     * @brief Transfer a random rumor from source to target.
     * @return true if a rumor was transferred.
     */
    bool propagate_information(entt::entity source, entt::entity target);

    /**
     * @brief Create a new piece of information in the world.
     */
    void create_rumor(InformationType type, const std::string& content_tag, entt::entity source_faction = entt::null);

    void handleInformationCreated(const InformationCreatedEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    uint64_t m_current_tick = 0;
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_INFORMATION_SYSTEM_H
