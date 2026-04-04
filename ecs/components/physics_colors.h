#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_PHYSICS_COLORS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_PHYSICS_COLORS_H

#include <string>

namespace NeonOubliette {

namespace Colors {
    // Thermal Palette
    inline const std::string CRYO_CRITICAL = "#00FFFF";
    inline const std::string SUB_ZERO      = "#B0E0E6";
    inline const std::string COLD          = "#87CEEB";
    inline const std::string FEVER         = "#FFD700";
    inline const std::string SCALDING      = "#FF8C00";
    inline const std::string IGNITION      = "#FF4500";
    inline const std::string PLASMA        = "#FFFFFF";

    // Weather Palette
    inline const std::string RAIN          = "#0055FF";
    inline const std::string ACID_RAIN     = "#7FFF00";
    inline const std::string SMOG          = "#708090";
    inline const std::string LIGHTNING     = "#FFFFFF";

    // Environmental Effects
    inline const std::string COOLING_MIST  = "#1E90FF";
}

/**
 * @brief Maps a temperature to a hex color string based on the vision_artist spec.
 */
inline std::string map_temperature_to_color(float temp, const std::string& original_color) {
    if (temp < -40.0f) return Colors::CRYO_CRITICAL;
    if (temp < 0.0f)   return Colors::SUB_ZERO;
    if (temp < 15.0f)  return Colors::COLD;
    if (temp < 35.0f)  return original_color;
    if (temp < 60.0f)  return Colors::FEVER;
    if (temp < 100.0f) return Colors::SCALDING;
    if (temp < 800.0f) return Colors::IGNITION;
    return Colors::PLASMA;
}

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_PHYSICS_COLORS_H
