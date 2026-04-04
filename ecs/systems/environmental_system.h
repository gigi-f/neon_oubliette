#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_ENVIRONMENTAL_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_ENVIRONMENTAL_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include <random>

namespace NeonOubliette {

/**
 * @brief Processes Layer 0 Environmental simulation, including the global weather cycle.
 *        Updates the global weather singleton which other systems (Physics, Biology) read from.
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
            m_registry.emplace<WeatherComponent>(entity, WeatherState::CLEAR, 0.0f, 20.0f, 100u);
        }
    }

    void update(double delta_time) override {
        (void)delta_time;
        auto view = m_registry.view<WeatherComponent>();
        for (auto entity : view) {
            auto& weather = view.get<WeatherComponent>(entity);
            
            // 1. Weather cycle
            if (weather.ticks_remaining > 0) {
                weather.ticks_remaining--;
            } else {
                transition_weather(weather);
            }
            
            // 2. Day/Night cycle
            update_time_of_day(weather);

            update_ambient_temperature(weather);
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L0_Physics;
    }

private:
    void update_time_of_day(WeatherComponent& weather) {
        weather.time_of_day_ticks++;
        
        uint32_t current_period_duration = 0;
        TimeOfDay next_time = weather.time_of_day;

        switch (weather.time_of_day) {
            case TimeOfDay::DAWN:
                current_period_duration = WeatherComponent::DAWN_DURATION;
                if (weather.time_of_day_ticks >= current_period_duration) next_time = TimeOfDay::DAY;
                break;
            case TimeOfDay::DAY:
                current_period_duration = WeatherComponent::DAY_DURATION;
                if (weather.time_of_day_ticks >= current_period_duration) next_time = TimeOfDay::DUSK;
                break;
            case TimeOfDay::DUSK:
                current_period_duration = WeatherComponent::DUSK_DURATION;
                if (weather.time_of_day_ticks >= current_period_duration) next_time = TimeOfDay::NIGHT;
                break;
            case TimeOfDay::NIGHT:
                current_period_duration = WeatherComponent::NIGHT_DURATION;
                if (weather.time_of_day_ticks >= current_period_duration) next_time = TimeOfDay::DAWN;
                break;
        }

        if (next_time != weather.time_of_day) {
            weather.time_of_day = next_time;
            weather.time_of_day_ticks = 0;
            
            std::string time_name = "Daylight";
            std::string time_color = "#FFFF00";
            switch(weather.time_of_day) {
                case TimeOfDay::DAWN: time_name = "Dawn"; time_color = "#FF8C00"; break;
                case TimeOfDay::DAY: time_name = "Day"; time_color = "#FFFFE0"; break;
                case TimeOfDay::DUSK: time_name = "Dusk"; time_color = "#8A2BE2"; break;
                case TimeOfDay::NIGHT: time_name = "Night"; time_color = "#191970"; break;
            }
            
            m_dispatcher.trigger(HUDNotificationEvent{"Time of Day: " + time_name, 3.0f, time_color});
            m_dispatcher.trigger(TimeOfDayChangeEvent{weather.time_of_day});
        }
    }

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

    void update_ambient_temperature(WeatherComponent& weather) {
        float target_temp = 20.0f;
        switch(weather.state) {
            case WeatherState::CLEAR: target_temp = 20.0f; break;
            case WeatherState::OVERCAST: target_temp = 15.0f; break;
            case WeatherState::RAIN: target_temp = 12.0f; break;
            case WeatherState::HEAVY_RAIN: target_temp = 8.0f; break;
            case WeatherState::ACID_RAIN: target_temp = 18.0f; break;
            case WeatherState::SMOG: target_temp = 28.0f; break;
            case WeatherState::ELECTRICAL_STORM: target_temp = 22.0f; break;
            default: break;
        }

        // Slowly drift toward target
        float drift = (target_temp - weather.ambient_temperature) * 0.01f;
        weather.ambient_temperature += drift;
    }

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_ENVIRONMENTAL_SYSTEM_H
