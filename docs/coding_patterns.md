# Neon Oubliette Coding Patterns and Standards

This document outlines the architectural and coding standards established during the engine stabilization phase. Future LLM assistants should adhere to these patterns to maintain consistency.

## 1. Namespaces (PascalCase)

- **`NeonOubliette`**: The primary namespace for ALL core engine classes, ECS components, and events.
  - *Unified Architecture*: Components (e.g., `PositionComponent`) and Events should reside directly in this namespace to avoid fragmentation.
- **`NeonOubliette::Systems`**: ECS system implementations inheriting from `ISystem`.
- **`NeonOubliette::Config`**: All configuration loading, validation, and schema handling logic.
- **`NeonOubliette::ECS`**: [LEGACY] Previously used for components. Now maintained as a namespace alias for backward compatibility. New code should use the top-level `NeonOubliette` namespace.

**Rule**: Always use the fully qualified namespace for clear disambiguation. For components, prefer the unified `NeonOubliette::` namespace over the legacy `ECS::` alias.

## 2. ECS System Pattern

All systems MUST:

1. Inherit from `NeonOubliette::ISystem`.
2. Implement `initialize()` and `update(double delta_time)`.
3. Be registered in `ecs/system_registration.cpp`.
4. Use `entt::registry&` and `entt::dispatcher&` passed during construction.

```cpp
namespace NeonOubliette::Systems {
class MyNewSystem : public ISystem {
public:
    MyNewSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    void initialize() override;
    void update(double delta_time) override;
private:
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
};
}
```

## 3. Notcurses Integration

- **Type**: Always use `struct notcurses*` for the context, not the `Notcurses` alias (which is sometimes missing or inconsistent in Notcurses C headers).
- **Initialization**: Use `notcurses_init(&options, nullptr)`. The second argument is a `FILE*` for logging, which should be `nullptr` for default behavior.
- **Loglevel**: Use `NCLOGLEVEL_PANIC` for production/minimal noise.

## 4. Configuration Loading

- **ADL Helpers**: When adding new config structures, ensure `from_json` helper declarations are present in `ConfigLoader.h` (to support Argument-Dependent Lookup by `nlohmann::json`).
- **Validation**: Every configuration load should specify a corresponding JSON schema.

## 5. Coding Style

- **Naming**: 
  - Classes/Namespaces: `PascalCase`.
  - Functions: `snake_case` (e.g., `register_all_systems`).
  - Variables: `snake_case` (e.g., `delta_time`).
  - Private Members: `snake_case_` with trailing underscore (e.g., `registry_`).
- **Headers**:
  - Use `#ifndef` guards.
  - Keep includes sorted and move implementation-specific includes to `.cpp` files.
  - `system_registration.h` includes for systems should stay inside the header guard.

## 6. Repository Structure

- **`external/`**: Contains header-only or source-included third-party libraries (`entt`, `nlohmann_json`, `json-schema-validator`).
- **`ecs/systems/`**: Contains all system implementations.
- **`data/configs/`** and **`data/schemas/`**: Paths should be managed via `ConfigLoader`.
