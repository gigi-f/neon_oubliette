#ifndef NEON_OUBLIETTE_PLAYER_CURRENT_LAYER_SYSTEM_H
#define NEON_OUBLIETTE_PLAYER_CURRENT_LAYER_SYSTEM_H

#include <entt/entt.hpp>

#include "system_scheduler.h"

namespace NeonOubliette::Systems {

class PlayerCurrentLayerSystem : public ISystem {
public:
    PlayerCurrentLayerSystem(entt::registry& registry);

    void initialize() override {
    }
    void update(double delta_time) override;

private:
    entt::registry& registry_;
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_PLAYER_CURRENT_LAYER_SYSTEM_H
