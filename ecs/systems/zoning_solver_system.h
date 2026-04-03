#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_ZONING_SOLVER_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_ZONING_SOLVER_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"
#include "../components/zoning_components.h"
#include <vector>
#include <set>
#include <map>

namespace NeonOubliette {

/**
 * @brief Implements a Wave Function Collapse (WFC) lite solver to assign
 *        logical urban zones to a macro-grid.
 */
class ZoningSolverSystem : public ISystem {
public:
    ZoningSolverSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;

    /**
     * @brief Generates a logical macro-grid of zones.
     * @param macro_width Width in macro-cells
     * @param macro_height Height in macro-cells
     */
    void solve_zoning(int macro_width, int macro_height);

private:
    struct SolverCell {
        std::set<ZoneType> possibilities;
        bool collapsed = false;
        ZoneType final_type = ZoneType::VOID;
    };

    void initialize_rules();
    bool propagate_constraints(std::vector<std::vector<SolverCell>>& grid, int x, int y);
    
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    
    std::map<ZoneType, std::vector<ZoneType>> m_adjacency_rules;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_ZONING_SOLVER_SYSTEM_H
