#ifndef NEON_OUBLIETTE_ECS_COMPONENT_REGISTRATION_H
#define NEON_OUBLIETTE_ECS_COMPONENT_REGISTRATION_H

#include <entt/entt.hpp>

namespace NeonOubliette {
namespace ECS {

void register_all_components(entt::registry& registry);
void register_all_events(entt::dispatcher& dispatcher);

} // namespace ECS
} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENT_REGISTRATION_H
