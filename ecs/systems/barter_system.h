#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_BARTER_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_BARTER_SYSTEM_H

#include <entt/entt.hpp>

#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class BarterSystem : public ISystem {
public:
    BarterSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {}
    void update(double delta_time) override {}

    void handleBarterEvent(const BarterEvent& event);
    void handleOpenBarterEvent(const OpenBarterEvent& event);
    void handleCloseBarterEvent(const CloseBarterEvent& event);
    void finalizeTrade(entt::entity initiator, entt::entity target, 
                       const std::vector<entt::entity>& initiator_items, 
                       const std::vector<entt::entity>& target_items);

private:
    float calculateUtilityValue(entt::entity agent_entity, const std::vector<entt::entity>& items, const std::vector<InformationRecord>& records);
    float calculateInformationUtility(const InformationRecord& record, entt::entity target_agent);
    float getBaseItemValue(entt::entity item_entity, entt::entity perspective_agent);
    
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_BARTER_SYSTEM_H
