#ifndef NEON_OUBLIETTE_SYSTEM_SCHEDULER_H
#define NEON_OUBLIETTE_SYSTEM_SCHEDULER_H

#include <entt/entt.hpp>
#include <map>
#include <memory>
#include <vector>

namespace NeonOubliette {

// Forward declaration of RenderingSystem to avoid circular dependency with its own header
namespace Systems {
class RenderingSystem;
}

/**
 * @brief Interface for all ECS systems.
 *        Systems should inherit from this interface to be managed by the SystemScheduler.
 */
class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void initialize() = 0;
    virtual void update(double delta_time) = 0;
    // virtual void shutdown() = 0; // Consider adding a shutdown method if systems need explicit cleanup
};

/**
 * @brief Manages and orchestrates the execution of various ECS systems
 *        across different phases of the simulation loop.
 */
class SystemScheduler {
public:
    /**
     * @brief Defines the distinct phases of the simulation loop.
     *        Systems are executed in the order of these phases.
     */
    enum class Phase {
        Input,     // Handle player input and other external events
        Macro,     // High-level simulation logic (e.g., AI, pathfinding, complex interactions)
        Micro,     // Fine-grained simulation logic (e.g., physics, component updates)
        PostMicro, // Systems that depend on Micro updates but precede rendering
        Output,    // Rendering and other output-related operations
        NumPhases  // Sentinel value for iteration/array sizing
    };

    SystemScheduler(entt::registry& registry, entt::dispatcher& event_dispatcher);
    ~SystemScheduler() = default;

    /**
     * @brief Adds a system to a specific phase of the scheduler.
     * @param phase The phase during which the system should be updated.
     * @param system A unique pointer to the system to be added.
     */
    void add_system(Phase phase, std::unique_ptr<ISystem> system);

    /**
     * @brief Initializes all registered systems.
     *        This should be called once after all systems have been added.
     */
    void initialize_all_systems();

    /**
     * @brief Executes all systems belonging to a specific phase.
     * @param phase The phase to run.
     * @param registry The main EnTT registry.
     * @param event_dispatcher The global event dispatcher.
     * @param delta_time The time elapsed since the last frame.
     */
    void run_phase(Phase phase, entt::registry& registry, entt::dispatcher& event_dispatcher, double delta_time);

private:
    std::map<Phase, std::vector<std::unique_ptr<ISystem>>> systems_;
    entt::registry& registry_;
    entt::dispatcher& event_dispatcher_;

    // Helper to convert Phase enum to string for logging/debugging
    static const char* phase_to_string(Phase phase);
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_SYSTEM_SCHEDULER_H
