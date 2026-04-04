#include "zoning_solver_system.h"
#include <random>
#include <algorithm>
#include <iostream>

namespace NeonOubliette {

ZoningSolverSystem::ZoningSolverSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    initialize_rules();
}

void ZoningSolverSystem::initialize() {}
void ZoningSolverSystem::update(double delta_time) { (void)delta_time; }

void ZoningSolverSystem::initialize_rules() {
    // Adjacency rules based on roadmap Phase 2.1
    m_adjacency_rules[ZoneType::CORPORATE] = {ZoneType::COMMERCIAL, ZoneType::TRANSIT, ZoneType::CORPORATE};
    m_adjacency_rules[ZoneType::COMMERCIAL] = {ZoneType::CORPORATE, ZoneType::RESIDENTIAL, ZoneType::TRANSIT, ZoneType::COMMERCIAL, ZoneType::PARK};
    m_adjacency_rules[ZoneType::RESIDENTIAL] = {ZoneType::COMMERCIAL, ZoneType::PARK, ZoneType::SLUM, ZoneType::RESIDENTIAL};
    m_adjacency_rules[ZoneType::SLUM] = {ZoneType::RESIDENTIAL, ZoneType::INDUSTRIAL, ZoneType::SLUM};
    m_adjacency_rules[ZoneType::INDUSTRIAL] = {ZoneType::SLUM, ZoneType::TRANSIT, ZoneType::INDUSTRIAL, ZoneType::AIRPORT};
    m_adjacency_rules[ZoneType::PARK] = {ZoneType::RESIDENTIAL, ZoneType::COMMERCIAL, ZoneType::PARK};
    m_adjacency_rules[ZoneType::TRANSIT] = {ZoneType::CORPORATE, ZoneType::COMMERCIAL, ZoneType::RESIDENTIAL, ZoneType::SLUM, ZoneType::INDUSTRIAL, ZoneType::PARK, ZoneType::TRANSIT, ZoneType::AIRPORT};
    m_adjacency_rules[ZoneType::AIRPORT] = {ZoneType::INDUSTRIAL, ZoneType::TRANSIT, ZoneType::AIRPORT};
    m_adjacency_rules[ZoneType::COLOSSEUM] = {ZoneType::COMMERCIAL, ZoneType::TRANSIT, ZoneType::PARK, ZoneType::CORPORATE, ZoneType::COLOSSEUM};
    
    // Add COLOSSEUM to existing rules
    m_adjacency_rules[ZoneType::COMMERCIAL].push_back(ZoneType::COLOSSEUM);
    m_adjacency_rules[ZoneType::TRANSIT].push_back(ZoneType::COLOSSEUM);
    m_adjacency_rules[ZoneType::PARK].push_back(ZoneType::COLOSSEUM);
    m_adjacency_rules[ZoneType::CORPORATE].push_back(ZoneType::COLOSSEUM);
}

void ZoningSolverSystem::solve_zoning(int macro_width, int macro_height) {
    std::vector<std::vector<SolverCell>> grid(macro_width, std::vector<SolverCell>(macro_height));
    
    // 1. Initial State: All cells can be any zone type
    std::set<ZoneType> all_types;
    for (int i = 1; i < (int)ZoneType::Count; ++i) all_types.insert(static_cast<ZoneType>(i));

    for (int x = 0; x < macro_width; ++x) {
        for (int y = 0; y < macro_height; ++y) {
            grid[x][y].possibilities = all_types;
        }
    }

    // 1.5 Seed an AIRPORT at the edges (0,0) and (width-1, height-1)
    grid[0][0].final_type = ZoneType::AIRPORT;
    grid[0][0].possibilities = {ZoneType::AIRPORT};
    grid[0][0].collapsed = true;
    propagate_constraints(grid, 0, 0);

    if (macro_width > 4 && macro_height > 4) {
        grid[macro_width-1][macro_height-1].final_type = ZoneType::AIRPORT;
        grid[macro_width-1][macro_height-1].possibilities = {ZoneType::AIRPORT};
        grid[macro_width-1][macro_height-1].collapsed = true;
        propagate_constraints(grid, macro_width-1, macro_height-1);
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    // 2. WFC Loop
    bool solving = true;
    while (solving) {
        // Find cell with lowest non-zero entropy
        int min_entropy = 999;
        int target_x = -1, target_y = -1;

        for (int x = 0; x < macro_width; ++x) {
            for (int y = 0; y < macro_height; ++y) {
                if (!grid[x][y].collapsed) {
                    int entropy = (int)grid[x][y].possibilities.size();
                    if (entropy < min_entropy) {
                        min_entropy = entropy;
                        target_x = x;
                        target_y = y;
                    }
                }
            }
        }

        if (target_x == -1) {
            solving = false; // All collapsed
            break;
        }

        // Collapse cell
        auto& cell = grid[target_x][target_y];
        if (cell.possibilities.empty()) {
            std::cerr << "[WFC ERROR] Contradiction at " << target_x << "," << target_y << std::endl;
            return; 
        }

        std::vector<ZoneType> choices(cell.possibilities.begin(), cell.possibilities.end());
        std::uniform_int_distribution<> dis(0, (int)choices.size() - 1);
        cell.final_type = choices[dis(gen)];
        cell.possibilities = {cell.final_type};
        cell.collapsed = true;

        // Propagate constraints
        propagate_constraints(grid, target_x, target_y);
    }

    // 3. Commit to ECS Registry
    for (int x = 0; x < macro_width; ++x) {
        for (int y = 0; y < macro_height; ++y) {
            auto entity = m_registry.create();
            m_registry.emplace<MacroZoneComponent>(entity, grid[x][y].final_type, x, y, 0.5f, "District " + std::to_string(x) + "-" + std::to_string(y));
        }
    }
}

bool ZoningSolverSystem::propagate_constraints(std::vector<std::vector<SolverCell>>& grid, int x, int y) {
    std::vector<std::pair<int, int>> stack = {{x, y}};
    int width = (int)grid.size();
    int height = (int)grid[0].size();

    while (!stack.empty()) {
        auto [cx, cy] = stack.back();
        stack.pop_back();

        const auto& current_possibilities = grid[cx][cy].possibilities;

        // Check 4-neighbors
        int dx[] = {0, 0, 1, -1};
        int dy[] = {1, -1, 0, 0};

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            if (nx >= 0 && nx < width && ny >= 0 && ny < height && !grid[nx][ny].collapsed) {
                auto& neighbor = grid[nx][ny];
                std::set<ZoneType> valid_neighbor_types;

                // Collect all types valid for neighbors based on current cell's possibilities
                for (auto type : current_possibilities) {
                    for (auto allowed : m_adjacency_rules[type]) {
                        valid_neighbor_types.insert(allowed);
                    }
                }

                // Intersection: neighbor.possibilities = neighbor.possibilities ∩ valid_neighbor_types
                std::set<ZoneType> next_possibilities;
                for (auto p : neighbor.possibilities) {
                    if (valid_neighbor_types.count(p)) {
                        next_possibilities.insert(p);
                    }
                }

                if (next_possibilities.size() < neighbor.possibilities.size()) {
                    neighbor.possibilities = next_possibilities;
                    stack.push_back({nx, ny});
                }
            }
        }
    }
    return true;
}

} // namespace NeonOubliette
