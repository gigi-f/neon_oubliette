#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_SOUND_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_SOUND_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette::Systems {

class SoundSystem : public ISystem {
public:
    SoundSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;

    void handleSpeechEvent(const SpeechEvent& event);

private:
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;

    void propagateSound(entt::entity speaker);
};

} // namespace NeonOubliette::Systems

#endif
