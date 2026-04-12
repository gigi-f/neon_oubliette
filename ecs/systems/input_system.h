#ifndef NEON_OUBLIETTE_INPUT_SYSTEM_H
#define NEON_OUBLIETTE_INPUT_SYSTEM_H

#include <notcurses/notcurses.h>

#include <entt/entt.hpp>

#include "system_scheduler.h" // For ISystem interface

namespace NeonOubliette {
namespace Systems {

class InputSystem : public ISystem {
public:
    InputSystem(entt::registry& registry, struct notcurses* nc_context, entt::dispatcher& dispatcher);
    ~InputSystem() = default;

    void initialize() override;
    void update(double delta_time) override;

private:
    entt::registry& m_registry;
    struct notcurses* m_ncContext;
    entt::dispatcher& m_dispatcher;

    // Event types for input actions
};

} // namespace Systems
} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_INPUT_SYSTEM_H
