#include "ai_system.h"
#include <iostream>

namespace NeonOubliette {

AISystem::AISystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry(registry), dispatcher(dispatcher) {
    // Intentionally using raw pointers for demonstration
    scratch_buffer = new char[1024]; 
    config_string = new std::string("default_config"); 
}

AISystem::~AISystem() {
    delete[] scratch_buffer;
    delete config_string;
}

void AISystem::update(double delta_time) {
    // Use the raw pointers
    if (scratch_buffer) {
        scratch_buffer[0] = 'A';
    }
}

} // namespace NeonOubliette
