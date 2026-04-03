#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_BUILDING_GENERATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_BUILDING_GENERATION_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"
#include <random>

namespace NeonOubliette {

class BuildingGenerationSystem : public ISystem {
public:
    BuildingGenerationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher), m_gen(m_rd()) {}

    void initialize() override {
        m_dispatcher.sink<BuildingEntranceEvent>().connect<&BuildingGenerationSystem::handleEntrance>(this);
    }

    void update(double delta_time) override {}

    void handleEntrance(const BuildingEntranceEvent& event) {
        if (!m_registry.valid(event.building)) return;

        auto& interior = m_registry.get_or_emplace<InteriorGeneratedComponent>(event.building);
        
        if (!interior.is_generated) {
            generateInterior(event.building, interior, event.entry_x, event.entry_y, event.entry_layer);
        }

        if (m_registry.all_of<PositionComponent>(event.visitor) && !interior.floor_entities.empty()) {
            auto& pos = m_registry.get<PositionComponent>(event.visitor);
            auto& player_layer = m_registry.get_or_emplace<PlayerCurrentLayerComponent>(event.visitor);

            int building_idx = static_cast<int>(entt::to_integral(event.building));
            pos.layer_id = (building_idx * 100); 
            pos.x = 5; // Interior start point (Exit Door)
            pos.y = 5;
            player_layer.current_z = pos.layer_id;

            m_dispatcher.enqueue<HUDNotificationEvent>("Entered Interior", 2.0f, "#AAAAFF");
        }
    }

private:
    void createTile(int x, int y, int layer, TerrainType type, char glyph, std::string color, bool is_obstacle = false) {
        auto entity = m_registry.create();
        m_registry.emplace<PositionComponent>(entity, x, y, layer);
        m_registry.emplace<TerrainComponent>(entity, type);
        m_registry.emplace<RenderableComponent>(entity, glyph, color, layer);
        if (is_obstacle) {
            m_registry.emplace<ObstacleComponent>(entity);
        }
    }

    void generateInterior(entt::entity building, InteriorGeneratedComponent& interior, int ex, int ey, int el) {
        auto const& b_data = m_registry.get<BuildingComponent>(building);
        int building_idx = static_cast<int>(entt::to_integral(building));

        // Use a 11x11 interior for all buildings for now, consistent with macro perimeters
        int width = 11;
        int height = 11;

        for (int i = 0; i < b_data.height; ++i) {
            int layer_id = (building_idx * 100) + i;
            
            auto floor_ent = m_registry.create();
            m_registry.emplace<FloorComponent>(floor_ent, i);
            m_registry.emplace<NameComponent>(floor_ent, "Floor " + std::to_string(i));
            
            for (int x = 0; x < width; ++x) {
                for (int y = 0; y < height; ++y) {
                    bool is_wall = (x == 0 || x == width - 1 || y == 0 || y == height - 1);
                    if (is_wall) {
                        createTile(x, y, layer_id, TerrainType::WALL, '#', "#666666", true);
                    } else {
                        createTile(x, y, layer_id, TerrainType::CONCRETE_FLOOR, '.', "#333333", false);
                    }
                }
            }

            // Create Exit Door on Floor 0 that leads to specific city coords
            if (i == 0) {
                auto door = m_registry.create();
                m_registry.emplace<PositionComponent>(door, 5, 5, layer_id);
                m_registry.emplace<RenderableComponent>(door, 'D', "#FFFF00", layer_id);
                m_registry.emplace<NameComponent>(door, "Exit to City");
                m_registry.emplace<PortalComponent>(door, ex, ey, el, true);
            }

            // Stairs at (1, 1)
            if (i < b_data.height - 1) {
                auto stairs_up = m_registry.create();
                m_registry.emplace<PositionComponent>(stairs_up, 1, 1, layer_id);
                m_registry.emplace<RenderableComponent>(stairs_up, '>', "#FFFFFF", layer_id);
                m_registry.emplace<StairsComponent>(stairs_up, layer_id + 1);
            }
            if (i > 0) {
                auto stairs_down = m_registry.create();
                m_registry.emplace<PositionComponent>(stairs_down, 1, 1, layer_id);
                m_registry.emplace<RenderableComponent>(stairs_down, '<', "#FFFFFF", layer_id);
                m_registry.emplace<StairsComponent>(stairs_down, layer_id - 1);
            }

            interior.floor_entities.push_back(floor_ent);
        }

        interior.is_generated = true;
        m_dispatcher.enqueue<LogEvent>("Generated Interior for Building " + std::to_string(building_idx), LogSeverity::INFO, "BuildingGen");
    }

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    std::random_device m_rd;
    std::mt19937 m_gen;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_BUILDING_GENERATION_SYSTEM_H
