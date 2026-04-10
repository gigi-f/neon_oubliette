#ifndef NEON_OUBLIETTE_INPUT_SYSTEM_H
#define NEON_OUBLIETTE_INPUT_SYSTEM_H

#include <cstdint>

#include <entt/entt.hpp>

#include "system_scheduler.h" // For ISystem interface

namespace NeonOubliette {
namespace Systems {

// Key definitions decoupled from notcurses
enum EngineKey : uint32_t {
    Key_Up = 0x10001,
    Key_Down = 0x10002,
    Key_Left = 0x10003,
    Key_Right = 0x10004,
    Key_Enter = 0x10005,
    Key_Esc = 0x10006,
    Key_Tab = 0x10007,
};

// Generic Input Event that backends (Notcurses or SDL2) will enqueue
struct EngineInputEvent {
    uint32_t key_id;   // Can be ascii char or EngineKey
    bool is_press;
};

class InputSystem : public ISystem {
public:
    InputSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    ~InputSystem() = default;

    void initialize() override;
    void update(double delta_time) override;

    // Direct receiver for decoupled input
    void handle_input_event(const EngineInputEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    // Buffer for inputs to process during update()
    std::vector<EngineInputEvent> pending_inputs_;
};

} // namespace Systems
} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_INPUT_SYSTEM_H
