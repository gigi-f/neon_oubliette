#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class AgentActionSystem : public ISystem {
public:
    AgentActionSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {
    }
    void update(double delta_time) override {
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    void handleTurnEvent(const TurnEvent& event);
};

} // namespace NeonOubliette
