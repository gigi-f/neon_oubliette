#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_BROADCAST_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_BROADCAST_COMPONENTS_H

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include "components.h"

namespace NeonOubliette {

/**
 * @brief [NEW CLASS] Defines an entity as a broadcast node for information propagation.
 *        Interacts with Layer 2 (Cognitive) and Layer 4 (Political).
 */
struct BroadcastTowerComponent {
    int radius = 30;                 // Broadcast range in tiles
    uint32_t broadcast_interval = 20; // Ticks between signal pulses
    uint32_t ticks_until_next = 0;   
    
    bool is_active = true;           
    float signal_fidelity = 1.0f;    // 0.0 to 1.0 (affects veracity of transmitted info)
    
    entt::entity active_information_entity = entt::null; // The specific rumor/news being pushed
    entt::entity controlling_faction = entt::null;     // Faction branding the broadcast
    
    // Pulse animation state for rendering
    float pulse_expansion = 0.0f;    // 0.0 to 1.0 of radius
    bool show_pulse = false;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("radius", radius),
           cereal::make_nvp("broadcast_interval", broadcast_interval),
           cereal::make_nvp("ticks_until_next", ticks_until_next),
           cereal::make_nvp("is_active", is_active),
           cereal::make_nvp("signal_fidelity", signal_fidelity),
           cereal::make_nvp("active_information_entity", active_information_entity),
           cereal::make_nvp("controlling_faction", controlling_faction));
    }
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_BROADCAST_COMPONENTS_H
