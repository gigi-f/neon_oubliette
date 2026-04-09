Convert pathfinding traversability/cost checks to O(1) grid lookups.
Evidence: neon_oubliette/ecs/systems/pathfinding_system.cpp, neon_oubliette/ecs/systems/pathfinding_system.cpp, neon_oubliette/ecs/systems/pathfinding_system.cpp.
Benefit: 5x to 20x faster path requests, major reduction in per-step spikes from NPC routing.
Density impact: preserved by keeping same agent count and decisions; only path query cost drops.

Reuse the same occupancy index in movement collision and interaction checks.
Evidence: neon_oubliette/ecs/systems/movement_system.cpp, neon_oubliette/ecs/systems/movement_system.cpp.
Benefit: 2x to 8x faster movement-heavy turns, less input-to-action lag.
Density impact: none.

Make PopulationSystem demographic/migration logic incremental rather than full rescans each update.
Evidence: neon_oubliette/ecs/system_registration.cpp, neon_oubliette/ecs/systems/population_system.h, neon_oubliette/ecs/systems/population_system.h.
Benefit: 3x to 15x for that subsystem at scale.
Density impact: preserved by maintaining full macro population state, just updating aggregates incrementally.

Index AgentDecision nearest-queries (nature, crime targets, worship, resource nodes) by chunk/layer.
Evidence: neon_oubliette/ecs/systems/agent_decision_system.cpp, neon_oubliette/ecs/systems/agent_decision_system.cpp, neon_oubliette/ecs/systems/agent_decision_system.cpp.
Benefit: 2x to 6x faster turn decision phase.
Density impact: none; same decision richness, faster candidate retrieval.

Eliminate chunk boundary hitches via amortized streaming budget + object pooling.
Evidence: neon_oubliette/ecs/systems/chunk_streaming_system.cpp, neon_oubliette/ecs/systems/chunk_streaming_system.cpp, neon_oubliette/ecs/systems/chunk_streaming_system.cpp.
Benefit: large reduction in stutter spikes; often 2x to 4x better worst-case step latency during movement across chunks.
Density impact: preserved; entities are still fully represented, just streamed without burst stalls.

Batch logging asynchronously instead of per-event open/write/close.
Evidence: neon_oubliette/ecs/systems/turn_manager_system.cpp, neon_oubliette/ecs/systems/logging_system.cpp.
Benefit: 10% to 40% less jitter on action-heavy turns.
Density impact: none.

Apply cadence budgets to heavy macro systems (as already done for L2-L4 simulation layers).
Evidence: neon_oubliette/ecs/simulation_coordinator.cpp, neon_oubliette/ecs/system_registration.cpp.
Benefit: 2x to 4x overall stability/throughput improvement when many systems are active.
Density impact: preserved by using statistical/interpolated updates rather than deleting simulation content.