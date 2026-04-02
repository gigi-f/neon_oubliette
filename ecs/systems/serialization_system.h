#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_SERIALIZATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_SERIALIZATION_SYSTEM_H

#include <entt/entt.hpp>

#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class SerializationSystem : public ISystem {
public:
    SerializationSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {
    }
    void update(double delta_time) override {
    }

    void save_game(const std::string& filename);
    void load_game(const std::string& filename);

private:
    entt::registry& registry;
    entt::dispatcher& dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_SERIALIZATION_SYSTEM_H
