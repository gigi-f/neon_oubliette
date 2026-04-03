#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_ENVIRONMENTAL_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_ENVIRONMENTAL_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include <random>

namespace NeonOubliette {

/**
 * @brief Processes Layer 0 Environmental simulation, including the global weather cycle.
 */
class EnvironmentalSystem : public ISimulationSystem {
public:
    EnvironmentalSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
        // Ensure a singleton weather entity exists
        auto view = m_registry.view<WeatherComponent>();
        if (view.empty()) {
            auto entity = m_registry.create();
            m_registry.emplace<WeatherComponent>(entity, WeatherState::CLEAR, 0.0f, 100u);
        }
    }

    void update(double delta_time) override {
        auto view = m_registry.view<WeatherComponent>();
        for (auto entity : view) {
            auto& weather = view.get<WeatherComponent>(entity);
            
            if (weather.ticks_remaining > 0) {
                weather.ticks_remaining--;
            } else {
                transition_weather(weather);
            }
            
            apply_weather_effects(weather);
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L0_Physics;
    }

private:
    void transition_weather(WeatherComponent& weather) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 100);
        std::uniform_int_distribution<uint32_t> disTicks(100, 500);

        int roll = dis(gen);
        
        // Simple Markov-ish transition logic
        if (weather.state == WeatherState::CLEAR) {
            if (roll < 70) weather.state = WeatherState::OVERCAST;
            else if (roll < 90) weather.state = WeatherState::SMOG;
            else weather.state = WeatherState::RAIN;
        } else if (weather.state == WeatherState::OVERCAST) {
            if (roll < 40) weather.state = WeatherState::CLEAR;
            else if (roll < 80) weather.state = WeatherState::RAIN;
            else weather.state = WeatherState::HEAVY_RAIN;
        } else if (weather.state == WeatherState::RAIN) {
            if (roll < 30) weather.state = WeatherState::HEAVY_RAIN;
            else if (roll < 60) weather.state = WeatherState::OVERCAST;
            else weather.state = WeatherState::CLEAR;
        } else {
            weather.state = WeatherState::CLEAR;
        }

        weather.ticks_remaining = disTicks(gen);
        weather.intensity = (float)dis(gen) / 100.0f;
        
        std::string weather_name = "Clear Skies";
        switch(weather.state) {
            case WeatherState::OVERCAST: weather_name = "Cloudy / Overcast"; break;
            case WeatherState::RAIN: weather_name = "Acidic Drizzle"; break;
            case WeatherState::HEAVY_RAIN: weather_name = "Heavy Downpour"; break;
            case WeatherState::ACID_RAIN: weather_name = "ACID RAIN WARNING"; break;
            case WeatherState::SMOG: weather_name = "Industrial Smog"; break;
            case WeatherState::ELECTRICAL_STORM: weather_name = "Electrical Storm"; break;
            default: break;
        }
        
        m_dispatcher.trigger(HUDNotificationEvent{"Weather Shift: " + weather_name, 3.0f, "#00FFFF"});
    }

    void apply_weather_effects(const WeatherComponent& weather) {
        // Apply effects to all Layer 0 Physics components in the world
        auto physics_view = m_registry.view<Layer0PhysicsComponent>();
        for (auto e : physics_view) {
            auto& phys = physics_view.get<Layer0PhysicsComponent>(e);
            
            switch(weather.state) {
                case WeatherState::RAIN:
                case WeatherState::HEAVY_RAIN:
                    phys.temperature_celsius -= 0.01f * weather.intensity;
                    break;
                case WeatherState::ACID_RAIN:
                    phys.structural_integrity -= 0.001f * weather.intensity;
                    break;
                case WeatherState::SMOG:
                    // Smog might increase temperature slightly (greenhouse)
                    phys.temperature_celsius += 0.005f * weather.intensity;
                    break;
                default: break;
            }
        }
    }

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_ENVIRONMENTAL_SYSTEM_H
