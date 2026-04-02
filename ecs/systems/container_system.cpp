#include "container_system.h"
#include <iostream>

namespace NeonOubliette {

ContainerSystem::ContainerSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry(registry), dispatcher(dispatcher) {
    dispatcher.sink<ContainerInteractionEvent>().connect<&ContainerSystem::handleContainerInteractionEvent>(*this);
}

void ContainerSystem::handleContainerInteractionEvent(const ContainerInteractionEvent& event) {
    if (!registry.valid(event.container_entity))
        return;

    if (registry.all_of<ContainerComponent>(event.container_entity)) {
        auto& container = registry.get<ContainerComponent>(event.container_entity);

        switch (event.interaction_type) {
            case ContainerInteractionType::OPEN:
                container.is_open = true;
                break;
            case ContainerInteractionType::CLOSE:
                container.is_open = false;
                break;
            case ContainerInteractionType::TOGGLE:
                container.is_open = !container.is_open;
                break;
        }
    }
}

} // namespace NeonOubliette
