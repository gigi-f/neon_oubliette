#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_ZONING_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_ZONING_COMPONENTS_H

#include <string>
#include <vector>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace NeonOubliette {

enum class ZoneType : uint8_t {
    VOID = 0,
    CORPORATE,
    COMMERCIAL,
    RESIDENTIAL,
    SLUM,
    INDUSTRIAL,
    PARK,
    TRANSIT,
    Count
};

/**
 * @brief Represents a macro-scale urban zone (e.g., a 20x20 tile area).
 *        Used by the generation system to decide what to build and by 
 *        simulation systems to apply district-wide modifiers.
 */
struct MacroZoneComponent {
    ZoneType type = ZoneType::VOID;
    int macro_x = 0; // Coordinates in macro-grid space
    int macro_y = 0;
    float density = 0.5f; // 0.0 to 1.0 (building vs open space)
    std::string district_name;
    
    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("type", type),
           cereal::make_nvp("macro_x", macro_x),
           cereal::make_nvp("macro_y", macro_y),
           cereal::make_nvp("density", density),
           cereal::make_nvp("district_name", district_name));
    }
};

/**
 * @brief Attached to buildings to track economic value influenced by zoning.
 */
struct PropertyComponent {
    int base_value = 1000;
    int current_market_value = 1000;
    std::string owner_faction;
    uint64_t last_tax_payment_turn = 0;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("base_value", base_value),
           cereal::make_nvp("current_market_value", current_market_value),
           cereal::make_nvp("owner_faction", owner_faction),
           cereal::make_nvp("last_tax_payment_turn", last_tax_payment_turn));
    }
};

/**
 * @brief Applied to Layer 0 Physics to simulate the "Heat Island Effect" in dense zones.
 */
struct HeatIslandComponent {
    float heat_retention_multiplier = 1.0f;
    float ambient_temp_offset = 0.0f;

    template <class Archive> void serialize(Archive& ar) {
        ar(cereal::make_nvp("heat_retention_multiplier", heat_retention_multiplier),
           cereal::make_nvp("ambient_temp_offset", ambient_temp_offset));
    }
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_ZONING_COMPONENTS_H
