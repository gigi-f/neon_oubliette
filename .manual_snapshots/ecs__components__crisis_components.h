#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_CRISIS_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_CRISIS_COMPONENTS_H

#include <string>
#include <vector>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <entt/entt.hpp>

namespace NeonOubliette {

/**
 * @brief [L.3] Tracks infection state for an agent during a Biological Outbreak.
 */
struct InfectionComponent {
    float progress = 0.0f;        // 0.0 (incubating) to 1.0 (critical)
    float virulence = 0.01f;      // Probability of transmission per tick
    float severity = 0.5f;        // Impact on biology
    uint64_t infected_at_tick = 0;
    bool is_contagious = true;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("progress", progress),
           cereal::make_nvp("virulence", virulence),
           cereal::make_nvp("severity", severity),
           cereal::make_nvp("infected_at_tick", infected_at_tick),
           cereal::make_nvp("is_contagious", is_contagious));
    }
};

/**
 * @brief [L.3] Tracks localized environmental hazards in a chunk or global singleton.
 */
struct EnvironmentalHazardComponent {
    float toxicity_level = 0.0f;   // 0.0 to 1.0
    float acidity_level = 0.0f;    // 0.0 to 1.0 (affects buildings)
    bool is_active = false;
    std::string hazard_description;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("toxicity_level", toxicity_level),
           cereal::make_nvp("acidity_level", acidity_level),
           cereal::make_nvp("is_active", is_active),
           cereal::make_nvp("hazard_description", hazard_description));
    }
};

/**
 * @brief [M.4] Localized hazard types for sewers and industrial areas.
 */
enum class HazardType : uint8_t {
    TOXIC_GAS,      // Affects biology (consciousness/pain)
    ELECTRICAL,    // High damage, affects electronics
    STEAM_VENT,    // High heat, localized
    BIO_HAZARD,    // Chance to infect
    RAD_ZONE,      // Gradual biological wear
    FIRE           // High heat, spreads?
};

/**
 * @brief [NEW CLASS] [M.4] Localized environmental hazard.
 */
struct TileHazardComponent {
    HazardType type = HazardType::TOXIC_GAS;
    float intensity = 1.0f;
    float radius = 0.0f; // 0 = single tile
    bool is_active = true;
    bool is_intermittent = false;
    uint32_t pulse_rate = 10; // Ticks between toggles if intermittent
    uint32_t ticks_to_next_pulse = 0;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("type", type),
           cereal::make_nvp("intensity", intensity),
           cereal::make_nvp("radius", radius),
           cereal::make_nvp("is_active", is_active),
           cereal::make_nvp("is_intermittent", is_intermittent),
           cereal::make_nvp("pulse_rate", pulse_rate),
           cereal::make_nvp("ticks_to_next_pulse", ticks_to_next_pulse));
    }
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_CRISIS_COMPONENTS_H
