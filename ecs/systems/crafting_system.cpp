#include "crafting_system.h"

#include <iostream>

namespace NeonOubliette {

CraftingSystem::CraftingSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry(registry), dispatcher(dispatcher) {
    dispatcher.sink<CraftItemEvent>().connect<&CraftingSystem::handleCraftItemEvent>(*this);
}

void CraftingSystem::handleCraftItemEvent(const CraftItemEvent& event) {
    // Current crafting logic from root version
    std::cout << "Crafting item for requester..." << std::endl;
}

} // namespace NeonOubliette
