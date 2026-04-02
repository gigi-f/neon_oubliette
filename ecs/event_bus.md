# Event Bus Specification

## 1. Overview

The Event Bus is a lightweight, type‑safe publish‑subscribe system used for intra‑world communication between ECS systems and for cross‑registry event propagation. It supports:

* **Scoped events** – events that are only visible within a single registry (macro or a specific region).
* **Global events** – events that are broadcast to all registries.
* **Event lifetimes** – events can be short‑lived (only during the tick) or persisted until explicitly cleared.

## 2. Design Goals

| Goal | Rationale |
|------|-----------|
| **Deterministic ordering** | All subscribers are notified in a deterministic order per event type, ensuring reproducibility.
| **Zero‑copy payload** | Event data is passed by value, but large data is wrapped in `std::shared_ptr` or reference‑counted handles to avoid copying.
| **Cross‑registry bridge** | Enables a `MonsterDied` event in a micro region to inform the macro economy without manual event forwarding.
| **Minimal overhead** | Use `entt::dispatcher` for most intra‑registry events; a custom lightweight bus for cross‑registry events.
| **Extensible** | New event types are added by defining a struct and registering it via `EventBus::register_event<T>()`.

## 3. API Outline

```cpp
// Base template for events
struct EventBase {};

// Example event
struct MonsterDied : public EventBase {
    entt::entity entity;
    int    level;
    uint64_t region_id;
};
```

### EventBus Class

```cpp
class EventBus {
public:
    // Register a new event type
    template <typename Event>
    void register_event();

    // Emit an event to the current registry
    template <typename Event>
    void emit(const Event& ev);

    // Emit an event to all registries (global)
    template <typename Event>
    void broadcast(const Event& ev);

    // Subscribe to an event
    template <typename Event>
    std::size_t subscribe(std::function<void(const Event&)> callback);

    // Unsubscribe
    template <typename Event>
    void unsubscribe(std::size_t id);

    // Retrieve events for a given tick (used by systems)
    template <typename Event>
    const std::vector<Event>& get_events() const;

private:
    // Internal storage: per‑event‑type vector of callbacks and events
    // Use type-erased containers (std::any) to keep a generic implementation
    ...
};
```

### Cross‑Registry Bridging

Each `RegionRegistry` holds a pointer to its parent `MacroRegistry`'s `EventBus`. When a region emits a global event, it forwards the event to the macro bus via `macro_bus_->broadcast(ev);` The macro bus then dispatches it to all listening macro systems.

## 4. Lifetime Management

Events are stored in per‑tick buffers. At the start of a tick, all buffers are cleared. Systems pull events via `get_events<T>()` and process them immediately. If a system needs to preserve an event beyond the current tick, it copies the data into its own persistent storage.

## 5. Implementation Notes

* **Thread Safety** – Emitting events is single‑threaded (only the main simulation thread modifies the bus). Subscribers are also called on the main thread.
* **Performance** – The dispatcher uses a simple `std::unordered_map<std::type_index, std::vector<std::function<void(const void*)>>>` to avoid per‑event type dynamic allocations.
* **Integration** – Each `entt::registry` has its own `EventBus` instance stored in `registry.ctx().emplace<EventBus>()`.

## 6. Usage Example

```cpp
// In a region system
auto& bus = region_registry.ctx().get<EventBus>();
bus.emit(MonsterDied{entt::entity{42}, 3, region_id});

// In macro system
auto& macro_bus = macro_registry.ctx().get<EventBus>();
macro_bus.subscribe<MonsterDied>([](const MonsterDied& ev){
    // Update global monster count
});
```

---

**Next** – Proceed to the System Scheduler spec, then to the rendering pipeline documentation.
