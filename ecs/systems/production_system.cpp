#include "production_system.h"
#include "../components/components.h"
#include "../event_declarations.h"
#include <random>

namespace NeonOubliette {

ProductionSystem::ProductionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    m_dispatcher.sink<TurnEvent>().connect<&ProductionSystem::handleTurnEvent>(*this);
}

void ProductionSystem::update(double delta_time) {
    (void)delta_time;
    // Tick-based logic is in handleTurnEvent
}

void ProductionSystem::handleTurnEvent(const TurnEvent& event) {
    (void)event;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    auto factory_view = m_registry.view<FactoryComponent, PositionComponent>();
    
    for (auto factory_entity : factory_view) {
        auto& factory = factory_view.get<FactoryComponent>(factory_entity);
        auto* disruption = m_registry.try_get<SupplyChainDisruptionComponent>(factory_entity);
        
        if (!factory.is_active || factory.available_recipes.empty()) continue;
        
        const auto& recipe = factory.available_recipes[factory.active_recipe_index];
        
        // --- [O.3] Supply Chain: Transport Bottleneck ---
        // Checks for inputs; bottleneck can simulate loss or delay
        bool has_inputs = true;
        for (const auto& [mat_type, amount] : recipe.inputs) {
            // Apply bottleneck penalty to current stock (simulating spoilage/loss/delay)
            if (disruption && disruption->transport_bottleneck > 0.5f && dis(gen) < 0.05f) {
                 factory.input_stockpile[mat_type] *= 0.95f; // Small drain due to bottleneck
            }

            if (factory.input_stockpile[mat_type] < amount) {
                has_inputs = false;
                break;
            }
        }
        
        if (!has_inputs) {
            // Keep active but don't progress if missing inputs
            continue;
        }
        
        // --- [O.3] Supply Chain: Labor Strikes ---
        float strike_modifier = disruption ? (1.0f - disruption->labor_strike) : 1.0f;

        // Calculate labor contribution
        float turn_contribution = 0.0f;
        auto agent_view = m_registry.view<AgentTaskComponent, PositionComponent, WorkOrderComponent>();
        const auto& f_pos = factory_view.get<PositionComponent>(factory_entity);
        
        for (auto agent_entity : agent_view) {
            const auto& a_task = agent_view.get<AgentTaskComponent>(agent_entity);
            const auto& a_pos = agent_view.get<PositionComponent>(agent_entity);
            const auto& a_work = agent_view.get<WorkOrderComponent>(agent_entity);
            
            if (a_task.task_type == AgentTaskType::PRODUCE_GOODS && 
                a_work.factory_entity == factory_entity &&
                a_pos.x == f_pos.x && a_pos.y == f_pos.y && a_pos.layer_id == f_pos.layer_id) {
                
                // Individual chance for a worker to "slack" due to strike conditions
                if (disruption && dis(gen) < disruption->labor_strike * 0.5f) {
                    continue; 
                }
                turn_contribution += a_work.contribution_per_tick;
            }
        }
        
        if (turn_contribution > 0) {
            // Apply factory-level strike modifier and base efficiency
            factory.current_progress += turn_contribution * recipe.efficiency_base * factory.base_efficiency * strike_modifier;
            
            // --- [O.3] Supply Chain: Sabotage ---
            // Direct loss of progress due to sabotage
            if (disruption && disruption->sabotage_risk > 0.3f && dis(gen) < disruption->sabotage_risk * 0.1f) {
                factory.current_progress *= 0.5f; // Half the progress is lost
                m_dispatcher.trigger<HUDNotificationEvent>({"Industrial sabotage at " + factory.owning_faction + " facility!", 3.0f, "#FF5500"});
            }

            if (factory.current_progress >= recipe.work_required) {
                // Consume inputs
                for (const auto& [mat_type, amount] : recipe.inputs) {
                    factory.input_stockpile[mat_type] -= amount;
                }
                
                // --- Produce the output item entity ---
                auto item = m_registry.create();
                m_registry.emplace<PositionComponent>(item, f_pos.x, f_pos.y, f_pos.layer_id);
                m_registry.emplace<ItemComponent>(item, recipe.output_item_type_id, recipe.output_name);
                m_registry.emplace<NameComponent>(item, recipe.output_name);
                m_registry.emplace<RenderableComponent>(item, recipe.output_glyph, recipe.output_color, f_pos.layer_id);
                m_registry.emplace<ItemValueComponent>(item, 50); // Base value
                m_registry.emplace<ItemMarketCategoryComponent>(item, recipe.output_category);
                m_registry.emplace<ItemMaterialComponent>(item, recipe.output_material);
                
                // If the factory has a storage container, place it inside (Phase O.2 bonus)
                if (m_registry.valid(factory.storage_container) && m_registry.all_of<ContainerComponent>(factory.storage_container)) {
                    auto& container = m_registry.get<ContainerComponent>(factory.storage_container);
                    container.contained_items.push_back(item);
                }
                
                // Trigger production event
                m_dispatcher.trigger<ProductionCompletedEvent>({factory_entity, recipe.output_item_type_id, recipe.output_name});
                
                // Reset progress
                factory.current_progress = 0.0f;

                // Visual feedback
                m_dispatcher.enqueue<SpeechEvent>({factory_entity, "Output cycle finished: " + recipe.output_name, 15, AudibilityLevel::CLEAR});
                m_dispatcher.trigger<HUDNotificationEvent>({"Factory produced " + recipe.output_name, 2.0f, "#00FFCC"});
            }
        }
    }
}

} // namespace NeonOubliette
