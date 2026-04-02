#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_AI_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_AI_SYSTEM_H

#include "../system_scheduler.h"
#include <entt/entt.hpp>
#include <string>

namespace NeonOubliette {

class AISystem : public ISystem {
public:
    AISystem(entt::registry& registry, entt::dispatcher& dispatcher);
    ~AISystem(); // We will implement manual delete here to show RAII issues

    void initialize() override {}
    void update(double delta_time) override;

private:
    entt::registry& registry;
    entt::dispatcher& dispatcher;
    
    // Intentionally using raw pointers for demonstration
    char* scratch_buffer; 
    std::string* config_string;
};

}

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_AI_SYSTEM_H
