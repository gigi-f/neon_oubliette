#ifndef NEON_OUBLIETTE_COMMAND_BUFFER_H
#define NEON_OUBLIETTE_COMMAND_BUFFER_H

#include <entt/entt.hpp>
#include <functional>
#include <vector>

namespace NeonOubliette {

// Command is a std::function that takes an entt::registry& and returns void.
// This allows systems to defer any registry-mutating operations.
using Command = std::function<void(entt::registry&)>;

/**
 * @brief A CommandBuffer serialises all mutating operations into a deterministic queue.
 *        Each building in the macro registry owns one CommandBuffer for its micro_registry.
 *
 * Architectural Reference:
 * /home/gianfiorenzo/Documents/ag2_workspace/neon_oubliette/architecture/command_buffer_design.md
 */
struct CommandBuffer {
    std::vector<Command> commands; ///< Per-building buffer of deferred commands.
    // size_t index = 0;                       ///< (Removed, not strictly needed for apply-all then clear pattern)

    /**
     * @brief Pushes a new command (lambda) into the buffer.
     * @param c The command to be executed later.
     */
    void push(Command c) {
        commands.emplace_back(std::move(c));
    }

    /**
     * @brief Applies all stored commands to the given registry.
     *        Clears the buffer after application.
     * @param r The registry to apply commands to (typically a micro_registry).
     */
    void apply(entt::registry& r) {
        for (size_t i = 0; i < commands.size(); ++i) {
            commands[i](r);
        }
        commands.clear();
        // index = 0; // Reset index if it was present
    }
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_COMMAND_BUFFER_H
