#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_MILESTONE_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_MILESTONE_COMPONENTS_H

#include <cereal/archives/binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <entt/entt.hpp>
#include <string>
#include <vector>

namespace NeonOubliette {

/**
 * @brief [NEW CLASS] A single major simulation event record.
 */
struct MilestoneRecord {
    uint64_t tick = 0;
    std::string type;        // e.g., "FACTION_DEAL", "FACTION_FEUD", "CONVERSION", "MARKET_CRASH"
    std::string description;
    std::string faction_a_id;
    std::string faction_b_id;
    entt::entity actor = entt::null;
    float importance = 1.0f; // 0.0 to 5.0 scale

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(tick), CEREAL_NVP(type), CEREAL_NVP(description), 
           CEREAL_NVP(faction_a_id), CEREAL_NVP(faction_b_id), 
           CEREAL_NVP(actor), CEREAL_NVP(importance));
    }
};

/**
 * @brief [NEW CLASS] Stores the global history of simulation milestones.
 *        Usually attached to a global "World" or "City" entity.
 */
struct MilestoneComponent {
    std::vector<MilestoneRecord> history;
    uint32_t max_milestones = 100; // Limit history to prevent bloat

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(history), CEREAL_NVP(max_milestones));
    }
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_MILESTONE_COMPONENTS_H
