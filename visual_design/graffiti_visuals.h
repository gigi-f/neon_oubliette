#ifndef NEON_OUBLIETTE_GRAFFITI_VISUALS_H
#define NEON_OUBLIETTE_GRAFFITI_VISUALS_H

#include <string>
#include <map>
#include <vector>
#include <cstdint>

namespace NeonOubliette::Visuals {

struct GraffitiStyle {
    char glyph;
    uint32_t color;
    float base_density;
};

/**
 * @brief [K.4] Maps faction IDs to their specific graffiti visual metaphors.
 *        Reflects the vision_artist spec for Project Neon Oubliette.
 */
inline const std::map<std::string, GraffitiStyle> FACTION_TAGS = {
    {"GOVERNMENT", {'|', 0x55AAFF, 0.8f}}, // Aura-9 (Consensus)
    {"REBEL",      {'%', 0xFF5555, 0.6f}}, // Malware-Alpha (Entropic)
    {"MAW",        {'(', 0x55FF55, 0.7f}}, // The Architect (Maw)
    {"VOID",       {'0', 0xAA55FF, 0.5f}}, // The Signal (Void)
    {"SYNDICATE",  {'$', 0xFFCC33, 0.9f}}  // The Arbiter (Syndicate)
};

inline const std::vector<char> GENERIC_TAGS = {'*', '~', '#', 'x', '.', ':', ';'};
inline const std::vector<uint32_t> DECAY_COLORS = {0x555555, 0x444444, 0x333333, 0x664422};

} // namespace NeonOubliette::Visuals

#endif // NEON_OUBLIETTE_GRAFFITI_VISUALS_H
