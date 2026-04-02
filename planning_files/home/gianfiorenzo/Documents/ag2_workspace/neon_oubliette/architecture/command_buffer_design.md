# Command Buffer Design

## Purpose
A **Command Buffer** serialises all mutating operations performed by micro‑systems into a deterministic queue. This guarantees
- *No race conditions* – all writes happen on the owning thread.
- *Determinism* – buffers are drained in a fixed order (building ID, then command index).
- *Extensibility* – new systems can push arbitrary lambdas without touching the registry directly.

## Structure
```cpp
using Command = std::function<void(entt::registry&)>

struct CommandBuffer {
    std::vector<Command> commands;           // per‑building buffer
    size_t index = 0;                       // next command to execute
    void push(Command c) { commands.emplace_back(std::move(c)); }
    void apply(entt::registry& r) {
        for (size_t i=0;i<commands.size();++i) commands[i](r);
        commands.clear(); index=0;
    }
};
```
Each `BuildingComponent` holds a `CommandBuffer` reference. Micro‑systems obtain the buffer via `entity.registry.get<CommandBuffer>()`.

## Threading Model
1. **Micro‑systems** run in parallel across buildings.
2. Each system pushes to its building’s buffer.
3. After all micro‑systems finish, the **CommandBufferApply** phase iterates over buildings in ascending ID order and calls `apply()`.

## Determinism
- Commands are queued *in the order the system iterates* over the `BuildingComponent` view. The view is sorted by entity ID.
- No system mutates the registry during execution; all changes are deferred.
- The buffer can contain *any* operation – component add/erase, tag, etc., ensuring full flexibility.

## API
```cpp
// Push a simple component add
buffer.push([e, component](entt::registry& r){ r.emplace_or_replace<Comp>(e, component); });

// Push an event
buffer.push([event](entt::registry& r){ r.ctx<EventBus>().publish(event); });
```

## Cleanup
After `apply()`, the buffer is cleared and ready for the next tick. If a system needs persistent state, it keeps it in the owning entity.
