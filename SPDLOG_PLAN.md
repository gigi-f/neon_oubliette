# spdlog Implementation Plan

## Constraints
- Notcurses owns the terminal. `stdout`/`stderr` output will corrupt the display.
- **All logging must go to a rotating file sink only.** No console sink.
- Log file path: `build/bin/neon_oubliette.log` (next to the binary).

---

## Phase 1 — CMake Integration

Add to `CMakeLists.txt` after the `backward` block:

```cmake
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)
FetchContent_MakeAvailable(spdlog)
```

Add to `target_link_libraries`:
```cmake
target_link_libraries(neon_oubliette PRIVATE
    ${NOTCURSES_LIBRARIES}
    inkcpp
    inkcpp_compiler
    spdlog::spdlog
)
```

---

## Phase 2 — Central Logger Header

Create `src/logging/logger.h`:

```cpp
#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>

namespace NeonOubliette {

inline void init_logger(const std::string& log_path = "neon_oubliette.log") {
    auto logger = spdlog::rotating_logger_mt(
        "game", log_path,
        /*max_size=*/ 1024 * 1024 * 10,   // 10 MB
        /*max_files=*/ 3
    );
    logger->set_level(spdlog::level::debug);
    logger->set_pattern("[%H:%M:%S.%e] [%l] [%n] %v");
    spdlog::set_default_logger(logger);
}

} // namespace NeonOubliette
```

Call `NeonOubliette::init_logger()` at the top of `main()`, **before** `notcurses_init()`.

---

## Phase 3 — Log Level Policy

| Level | Use for |
|-------|---------|
| `trace` | Per-tick per-entity events (movement steps, task ticks) — disabled in release |
| `debug` | System events: task assignment, path found, crime flagged |
| `info` | Significant game state: agent spawned, building generated, crime committed |
| `warn` | Recoverable problems: component missing where expected, path not found |
| `error` | Logic failures: invalid entity access, null enum state, file load failure |
| `critical` | Fatal: config missing, Notcurses init failed |

Release builds: set default level to `warn` via CMake `-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_WARN`.
Debug builds: `debug` or `trace`.

---

## Phase 4 — Migrate Existing std::cerr / std::cout

Replace the scattered stream calls (~15 sites) found in:

| File | Current | Replace with |
|------|---------|--------------|
| `grammar_engine.cpp:19` | `std::cerr << "GrammarEngine: ...` | `spdlog::error(...)` |
| `grammar_engine.cpp:32` | `std::cerr << "GrammarEngine: Error parsing...` | `spdlog::error(...)` |
| `religion_system.cpp:56` | `std::cerr << "[ReligionSystem] Failed to open...` | `spdlog::error(...)` |
| `religion_system.cpp:85` | `std::cout << "[ReligionSystem] Loaded...` | `spdlog::info(...)` |
| `religion_system.cpp:88` | `std::cerr << "[ReligionSystem] Error loading...` | `spdlog::error(...)` |
| `agent_spawn_system.cpp:18` | `std::cerr << "[AgentSpawnSystem] Cannot spawn...` | `spdlog::warn(...)` |
| `agent_spawn_system.cpp:342` | `std::cerr << "[AgentSpawnSystem] spawnAgents...` | `spdlog::warn(...)` |
| `zoning_solver_system.cpp:106` | `std::cerr << "[WFC WARNING]...` | `spdlog::warn(...)` |
| `player_movement_system.cpp:13,20` | `std::cout << "PlayerMovementSystem...` | `spdlog::debug(...)` |
| `crafting_system.cpp:14` | `std::cout << "Crafting item...` | `spdlog::debug(...)` |

---

## Phase 5 — Instrument Unsafe Component Access

The `agent_action_system.cpp` crime task code uses raw `m_registry.get<>()` without guards. After adding spdlog, change these to `try_get` with `warn` logging:

```cpp
// Before (crashes if component missing):
auto& self_econ = m_registry.get<Layer3EconomicComponent>(entity);

// After:
auto* self_econ = m_registry.try_get<Layer3EconomicComponent>(entity);
if (!self_econ) {
    spdlog::warn("[AgentActionSystem] entity {:x} missing Layer3EconomicComponent for task {}",
                 (uint32_t)entity, (int)task.type);
    continue;
}
```

Same pattern for `CrimeRiskComponent` and the `NPCComponent` access in `conversation_system.cpp`.

This converts a silent crash into a logged warning.

---

## Phase 6 — Per-System Loggers (Optional, Later)

Once the default logger is working, optionally give high-volume systems a named child logger with its own level:

```cpp
auto action_log = spdlog::default_logger()->clone("agent_action");
action_log->set_level(spdlog::level::warn); // suppress trace spam
```

This lets you enable `trace` globally but silence noisy systems.

---

## Implementation Order

1. CMake + FetchContent (Phase 1)
2. `logger.h` + `main.cpp` call (Phase 2)
3. Migrate existing `std::cerr`/`std::cout` sites (Phase 4)
4. Instrument the unsafe `get<>()` calls in agent_action + conversation systems (Phase 5)
5. Fine-tune levels per system (Phase 6, later)
