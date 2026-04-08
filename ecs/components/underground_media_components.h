#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_UNDERGROUND_MEDIA_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_UNDERGROUND_MEDIA_H

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include "components.h"

namespace NeonOubliette {

/**
 * @brief [NEW CLASS] Clandestine, low-power, illegal broadcast node.
 *        Interacts with Layer 2 (Cognitive) and Layer 4 (Political).
 */
struct PirateNodeComponent {
    int radius = 12;                 
    uint32_t broadcast_interval = 15; 
    uint32_t ticks_until_next = 0;
    
    float glitch_factor = 0.2f;      // Chance of data corruption per pulse
    entt::entity active_information_entity = entt::null; 
    entt::entity controlling_faction = entt::null;     
    
    bool is_hidden = true;           // Harder to detect for guards/unaligned
    bool is_improvised = false;      // Created by an agent during simulation

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("radius", radius),
           cereal::make_nvp("broadcast_interval", broadcast_interval),
           cereal::make_nvp("ticks_until_next", ticks_until_next),
           cereal::make_nvp("glitch_factor", glitch_factor),
           cereal::make_nvp("active_information_entity", active_information_entity),
           cereal::make_nvp("controlling_faction", controlling_faction),
           cereal::make_nvp("is_hidden", is_hidden),
           cereal::make_nvp("is_improvised", is_improvised));
    }
};

/**
 * @brief [NEW CLASS] Physical information storage item (Data Slab).
 *        Propagates through trade and item exchange.
 */
struct DataSlabComponent {
    InformationRecord stored_record;
    bool is_encrypted = false;       
    int uses_remaining = 3;          

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("stored_record", stored_record),
           cereal::make_nvp("is_encrypted", is_encrypted),
           cereal::make_nvp("uses_remaining", uses_remaining));
    }
};

/**
 * @brief Event fired when a pirate node is detected by an authority.
 */
struct PirateNodeDetectedEvent {
    entt::entity node_entity;
    entt::entity detector_entity;
    int x, y, layer;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_UNDERGROUND_MEDIA_H
