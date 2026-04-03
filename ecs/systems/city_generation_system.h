#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CITY_GENERATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CITY_GENERATION_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../system_scheduler.h"
#include <string>
#include <set>

namespace NeonOubliette {

class CityGenerationSystem : public ISystem {
public:
    CityGenerationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}
    void update(double delta_time) override {}

    void generateCity(int width, int height) {
        std::set<std::pair<int, int>> structure_footprints;

        auto addBuilding = [&](std::string name, int x, int y, int w, int h, int floors, std::string color) {
            createBuildingShell(name, x, y, w, h, floors, color);
            for (int dx = 0; dx < w; ++dx) {
                for (int dy = 0; dy < h; ++dy) {
                    structure_footprints.insert({x + dx, y + dy});
                }
            }
        };

        addBuilding("Corporate HQ", 10, 2, 12, 8, 5, "#FF00FF");
        addBuilding("Slum Apartments", 30, 2, 10, 6, 2, "#AA5500");
        addBuilding("Data Haven", 45, 4, 6, 4, 1, "#00FF00");

        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                if (structure_footprints.count({x, y})) continue;

                if (y >= 14 && y <= 18) {
                    createTile(x, y, 0, TerrainType::STREET, ' ', "#222222");
                } else if (y == 13 || y == 19) {
                    createTile(x, y, 0, TerrainType::SIDEWALK, '.', "#444444");
                } else {
                    createTile(x, y, 0, TerrainType::GRASS, '"', "#004400");
                }
            }
        }

        // --- Volumetric Entities ---
        // Trucks: 8x2
        createEntity("Heavy Truck", 5, 15, 8, 2, "#888888", 'T', true);
        
        // Cars: 4x1
        createEntity("Sleek Sedan", 20, 17, 4, 1, "#00AAFF", 'C', true);
        createEntity("Yellow Taxi", 35, 15, 4, 1, "#FFFF00", 'C', true);

        // Small Entities: 1x1
        createEntity("Stray Dog", 5, 10, 1, 1, "#AA8855", 'd', false);
        createEntity("Mailman", 12, 13, 1, 1, "#5555FF", 'm', false);

        // --- Consumable Items (Phase 1.4) ---
        auto createFood = [&](int x, int y, std::string name) {
            auto e = m_registry.create();
            m_registry.emplace<NameComponent>(e, name);
            m_registry.emplace<PositionComponent>(e, x, y, 0);
            m_registry.emplace<ItemComponent>(e, 1, name);
            m_registry.emplace<RenderableComponent>(e, '%', "#00FF00", 0);
            m_registry.emplace<ConsumableComponent>(e, 30, 0); // restores 30 hunger
        };

        auto createWater = [&](int x, int y, std::string name) {
            auto e = m_registry.create();
            m_registry.emplace<NameComponent>(e, name);
            m_registry.emplace<PositionComponent>(e, x, y, 0);
            m_registry.emplace<ItemComponent>(e, 2, name);
            m_registry.emplace<RenderableComponent>(e, '~', "#0000FF", 0);
            m_registry.emplace<ConsumableComponent>(e, 0, 40); // restores 40 thirst
        };

        createFood(15, 14, "Synth-Bread");
        createFood(35, 14, "Vat-Meat");
        createWater(20, 18, "Purified Water");
        createWater(40, 18, "Recycled Slurry");
    }

private:
    void createTile(int x, int y, int layer, TerrainType type, char glyph, std::string color) {
        auto e = m_registry.create();
        m_registry.emplace<PositionComponent>(e, x, y, layer);
        m_registry.emplace<TerrainComponent>(e, type);
        m_registry.emplace<RenderableComponent>(e, glyph, color, layer);
        if (type == TerrainType::WALL) m_registry.emplace<ObstacleComponent>(e);
    }

    void createBuildingShell(std::string name, int x, int y, int w, int h, int floors, std::string color) {
        auto building = m_registry.create();
        m_registry.emplace<NameComponent>(building, name);
        m_registry.emplace<PositionComponent>(building, x, y, 0);
        m_registry.emplace<BuildingComponent>(building, floors, "GENERAL", 0);
        m_registry.emplace<SizeComponent>(building, w, h);

        for (int cur_x = x; cur_x < x + w; ++cur_x) {
            for (int cur_y = y; cur_y < y + h; ++cur_y) {
                bool is_edge = (cur_x == x || cur_x == x + w - 1 || cur_y == y || cur_y == y + h - 1);
                if (is_edge) {
                    if (cur_y == y + h - 1 && cur_x == x + (w/2)) {
                        auto door = m_registry.create();
                        m_registry.emplace<PositionComponent>(door, cur_x, cur_y, 0);
                        m_registry.emplace<RenderableComponent>(door, '+', "#FFFF00", 0);
                        m_registry.emplace<NameComponent>(door, name + " Entrance");
                        m_registry.emplace<BuildingEntranceComponent>(door, building, 0);
                    } else if ((cur_x + cur_y) % 4 == 0) {
                        createTile(cur_x, cur_y, 0, TerrainType::WINDOW, '~', "#00FFFF");
                    } else {
                        createTile(cur_x, cur_y, 0, TerrainType::WALL, '#', color);
                    }
                } else {
                    createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#111111");
                }
            }
        }
    }

    void createEntity(std::string name, int x, int y, int w, int h, std::string color, char glyph, bool is_obstacle) {
        auto e = m_registry.create();
        m_registry.emplace<NameComponent>(e, name);
        m_registry.emplace<PositionComponent>(e, x, y, 0);
        m_registry.emplace<SizeComponent>(e, w, h);
        m_registry.emplace<RenderableComponent>(e, glyph, color, 0);
        if (is_obstacle) m_registry.emplace<ObstacleComponent>(e);
    }

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_CITY_GENERATION_SYSTEM_H
