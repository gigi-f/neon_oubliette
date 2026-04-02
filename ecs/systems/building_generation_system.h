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
            generateInterior(event.building, interior);
        }

        if (m_registry.all_of<PositionComponent>(event.visitor) && !interior.floor_entities.empty()) {
            auto& pos = m_registry.get<PositionComponent>(event.visitor);
            auto& player_layer = m_registry.get_or_emplace<PlayerCurrentLayerComponent>(event.visitor);

            int building_idx = static_cast<int>(entt::to_integral(event.building));
            pos.layer_id = (building_idx * 100); 
            pos.x = 2;
            pos.y = 2;
            player_layer.current_z = pos.layer_id;

            m_dispatcher.enqueue<HUDNotificationEvent>("Entered Interior: Floor 0", 2.0f, "#AAAAFF");
            m_dispatcher.enqueue<LogEvent>("Entity transitioned to interior layer " + std::to_string(pos.layer_id), LogSeverity::INFO, "BuildingGen");
        }
    }

private:
    void generateInterior(entt::entity building, InteriorGeneratedComponent& interior) {
        auto const& b_data = m_registry.get<BuildingComponent>(building);
        int building_idx = static_cast<int>(entt::to_integral(building));

        for (int i = 0; i < b_data.height; ++i) {
            int layer_id = (building_idx * 100) + i;
            
            auto floor = m_registry.create();
            m_registry.emplace<FloorComponent>(floor, i);
            m_registry.emplace<NameComponent>(floor, "Floor " + std::to_string(i));
            
            std::uniform_int_distribution<> room_dist(3, 6);
            int num_rooms = room_dist(m_gen);
            for (int r = 0; r < num_rooms; ++r) {
                auto room = m_registry.create();
                m_registry.emplace<RoomComponent>(room, static_cast<uint32_t>(r), 5);
                m_registry.emplace<PositionComponent>(room, 2 + (r * 4), 5, layer_id);
                m_registry.emplace<RenderableComponent>(room, '#', "#333333", layer_id);
                m_registry.emplace<ConditionComponent>(room, 1.0f);
                
                if (b_data.zoning == "RESIDENTIAL") {
                    m_registry.emplace<ApartmentComponent>(room, i, r, false);
                } else {
                    m_registry.emplace<WorkstationComponent>(room);
                }

                m_registry.get<FloorComponent>(floor).rooms.push_back(room);
            }

            auto stairs = m_registry.create();
            m_registry.emplace<PositionComponent>(stairs, 0, 0, layer_id);
            m_registry.emplace<RenderableComponent>(stairs, (i == b_data.height - 1) ? '<' : 'X', "#FFFFFF", layer_id);
            
            if (i < b_data.height - 1) {
                m_registry.emplace<StairsComponent>(stairs, layer_id + 1);
            } else if (i > 0) {
                m_registry.emplace<StairsComponent>(stairs, layer_id - 1);
            }

            interior.floor_entities.push_back(floor);
        }

        interior.is_generated = true;
        m_dispatcher.enqueue<LogEvent>("Generated " + std::to_string(b_data.height) + " floors for building " + std::to_string(building_idx), LogSeverity::INFO, "BuildingGen");
    }

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    std::random_device m_rd;
    std::mt19937 m_gen;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_BUILDING_GENERATION_SYSTEM_H
