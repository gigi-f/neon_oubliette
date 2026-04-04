#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_PHYSICS_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_PHYSICS_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include <map>
#include <vector>

namespace NeonOubliette {

struct MaterialProperties {
    float thermal_conductivity; // 0 to 1, rate of equalization with neighbors
    float thermal_mass;         // 1 to infinity, resistance to change
    float damage_threshold_high; // C at which integrity drops
    float damage_threshold_low;  // C at which integrity drops (freeze)
    float damage_rate;           // integrity per tick per degree past threshold
    float ambient_equalization;  // rate of returning to weather temp
};

/**
 * @brief Manages Layer 0 Physics, primarily thermodynamics and structural integrity.
 *        Emplements material-specific heat transfer and environmental interactions.
 */
class PhysicsSystem : public ISimulationSystem {
public:
    PhysicsSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {
        
        // Define Material Properties
        // Conductivity: how fast it shares heat. Mass: how hard it is to change temp.
        m_material_props[MaterialType::CONCRETE]    = {0.05f, 10.0f, 800.0f, -40.0f, 0.0001f, 0.02f};
        m_material_props[MaterialType::FLESH]       = {0.20f, 2.0f,  45.0f,  0.0f,   0.005f,  0.10f};
        m_material_props[MaterialType::STEEL]       = {0.80f, 5.0f,  1200.0f,-100.0f, 0.0001f, 0.05f};
        m_material_props[MaterialType::PLASTIC]     = {0.02f, 1.0f,  150.0f, -20.0f, 0.001f,  0.01f};
        m_material_props[MaterialType::GLASS]       = {0.03f, 4.0f,  500.0f, -30.0f, 0.0005f, 0.01f};
        m_material_props[MaterialType::ELECTRONICS] = {0.30f, 1.5f,  85.0f,  -20.0f, 0.002f,  0.05f};
        m_material_props[MaterialType::WATER]       = {0.60f, 50.0f, 100.0f, 0.0f,   0.0f,    0.20f};
    }

    void initialize() override {}

    void update(double delta_time) override {
        (void)delta_time;
        
        // 1. Get Ambient State
        float global_ambient_temp = 20.0f;
        auto weather_view = m_registry.view<WeatherComponent>();
        if (!weather_view.empty()) {
            global_ambient_temp = weather_view.get<WeatherComponent>(weather_view.front()).ambient_temperature;
        }

        // 2. Build Spatial Map for Tile-based Conduction & Cooling Fields
        std::map<PositionComponent, entt::entity> tile_map;
        std::vector<PositionComponent> water_tiles;
        auto tile_view = m_registry.view<PositionComponent, TerrainComponent, Layer0PhysicsComponent>();
        for (auto entity : tile_view) {
            const auto& pos = tile_view.get<PositionComponent>(entity);
            tile_map[pos] = entity;
            const auto& phys = tile_view.get<Layer0PhysicsComponent>(entity);
            if (phys.material == MaterialType::WATER) {
                water_tiles.push_back(pos);
            }
        }

        // 3. Adjacent Heat Transfer (Grid Conduction)
        std::map<entt::entity, float> temp_deltas;
        
        for (auto entity : tile_view) {
            const auto& pos = tile_view.get<PositionComponent>(entity);
            auto& phys = tile_view.get<Layer0PhysicsComponent>(entity);
            const auto& props = m_material_props[phys.material];

            float neighbor_sum = 0.0f;
            int neighbors_found = 0;

            int dx[] = {0, 0, 1, -1};
            int dy[] = {1, -1, 0, 0};
            for (int i = 0; i < 4; ++i) {
                PositionComponent n_pos(pos.x + dx[i], pos.y + dy[i], pos.layer_id);
                auto it = tile_map.find(n_pos);
                if (it != tile_map.end()) {
                    const auto& n_phys = m_registry.get<Layer0PhysicsComponent>(it->second);
                    neighbor_sum += n_phys.temperature_celsius;
                    neighbors_found++;
                }
            }

            if (neighbors_found > 0) {
                float avg_neighbor_temp = neighbor_sum / (float)neighbors_found;
                float equalization_delta = (avg_neighbor_temp - phys.temperature_celsius) * props.thermal_conductivity;
                temp_deltas[entity] = equalization_delta;
            }
        }

        // Apply Conduction Deltas
        for (auto const& [entity, delta] : temp_deltas) {
            auto& phys = m_registry.get<Layer0PhysicsComponent>(entity);
            const auto& props = m_material_props[phys.material];
            phys.temperature_celsius += delta / props.thermal_mass;
        }

        // 4. Update All Physics Entities (Equalization with Tile and Ambient, and Damage)
        // Pre-compute a grid of nearest water distance to avoid O(water_tiles) per entity
        struct WaterInfluence { float dist; };
        std::map<PositionComponent, WaterInfluence> water_influence;
        for (const auto& w_pos : water_tiles) {
            for (int wdx = -4; wdx <= 4; ++wdx) {
                for (int wdy = -4; wdy <= 4; ++wdy) {
                    float d_sq = (float)(wdx*wdx + wdy*wdy);
                    if (d_sq >= 16.0f) continue;
                    PositionComponent target(w_pos.x + wdx, w_pos.y + wdy, w_pos.layer_id);
                    auto it = water_influence.find(target);
                    if (it == water_influence.end() || d_sq < it->second.dist) {
                        water_influence[target] = {d_sq};
                    }
                }
            }
        }

        auto all_phys_view = m_registry.view<PositionComponent, Layer0PhysicsComponent>();
        for (auto entity : all_phys_view) {
            const auto& pos = all_phys_view.get<PositionComponent>(entity);
            auto& phys = all_phys_view.get<Layer0PhysicsComponent>(entity);
            const auto& props = m_material_props[phys.material];

            // A. LOCAL AMBIENT (With River Cooling Fields)
            float local_ambient = global_ambient_temp;
            if (phys.material != MaterialType::WATER) {
                auto wi = water_influence.find(pos);
                if (wi != water_influence.end()) {
                    float dist = std::sqrt(wi->second.dist);
                    float cooling_factor = 1.0f - (dist / 4.0f);
                    local_ambient -= cooling_factor * 10.0f;
                }
            }

            // B. Ambient Equalization (Convection/Radiation)
            float ambient_delta = (local_ambient - phys.temperature_celsius);
            phys.temperature_celsius += ambient_delta * props.ambient_equalization / props.thermal_mass;

            // C. Tile Equalization (Conduction from ground)
            if (!m_registry.all_of<TerrainComponent>(entity)) {
                auto it = tile_map.find(pos);
                if (it != tile_map.end()) {
                    const auto& tile_phys = m_registry.get<Layer0PhysicsComponent>(it->second);
                    float tile_delta = (tile_phys.temperature_celsius - phys.temperature_celsius);
                    phys.temperature_celsius += tile_delta * 0.1f / props.thermal_mass;
                }
            }

            // C. Integrity Damage from Heat
            if (phys.temperature_celsius > props.damage_threshold_high) {
                float excess = phys.temperature_celsius - props.damage_threshold_high;
                phys.structural_integrity -= excess * props.damage_rate;
            } else if (phys.temperature_celsius < props.damage_threshold_low) {
                float excess = props.damage_threshold_low - phys.temperature_celsius;
                phys.structural_integrity -= excess * props.damage_rate;
            }

            // D. Clamping
            if (phys.structural_integrity < 0.0f) phys.structural_integrity = 0.0f;
            if (phys.structural_integrity > 1.0f) phys.structural_integrity = 1.0f;
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L0_Physics;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    std::map<MaterialType, MaterialProperties> m_material_props;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_PHYSICS_SYSTEM_H
