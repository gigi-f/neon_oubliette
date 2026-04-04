#include "faction_system.h"
#include <algorithm>
#include <cmath>
#include "../components/lod_components.h"

namespace NeonOubliette {

FactionSystem::FactionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher)
{
    m_dispatcher.sink<ChangeFactionStandingEvent>().connect<&FactionSystem::handleChangeFactionStanding>(*this);
    m_dispatcher.sink<AgentFactionReputationEvent>().connect<&FactionSystem::handleAgentReputation>(*this);
}

void FactionSystem::initialize() {
    // 1. Create the four faction leader entities if they don't exist
    auto leader_view = m_registry.view<FactionLeaderComponent>();
    if (leader_view.empty()) {
        // --- Aura-9 (CONSENSUS) ---
        auto aura9 = m_registry.create();
        m_registry.emplace<NameComponent>(aura9, "Aura-9");
        m_registry.emplace<FactionComponent>(aura9, "GOVERNMENT", 100, 1.0f);
        m_registry.emplace<FactionLeaderComponent>(aura9, "Aura-9", FactionArchetype::CONSENSUS, 2.0f);
        m_registry.emplace<FactionDirectiveComponent>(aura9, DirectiveType::ROUTINE_ENFORCEMENT, 100, 1.5f);

        // --- Malware-Alpha (ENTROPIC_DRIFT) ---
        auto malware = m_registry.create();
        m_registry.emplace<NameComponent>(malware, "Malware-Alpha");
        m_registry.emplace<FactionComponent>(malware, "REBEL", 50, 0.8f);
        m_registry.emplace<FactionLeaderComponent>(malware, "Malware-Alpha", FactionArchetype::ENTROPIC_DRIFT, 1.2f);
        m_registry.emplace<FactionDirectiveComponent>(malware, DirectiveType::UTILITY_BURST, 50, 2.0f);

        // --- The Architect (SILICON_MAW) ---
        auto architect = m_registry.create();
        m_registry.emplace<NameComponent>(architect, "The Architect");
        m_registry.emplace<FactionComponent>(architect, "MAW", 75, 1.5f);
        m_registry.emplace<FactionLeaderComponent>(architect, "The Architect", FactionArchetype::SILICON_MAW, 3.0f);
        m_registry.emplace<FactionDirectiveComponent>(architect, DirectiveType::BIOLOGICAL_OVERRIDE, 200, 1.0f);

        // --- The Signal (VOID_WALKERS) ---
        auto signal = m_registry.create();
        m_registry.emplace<NameComponent>(signal, "The Signal");
        m_registry.emplace<FactionComponent>(signal, "VOID", 30, 0.5f);
        m_registry.emplace<FactionLeaderComponent>(signal, "The Signal", FactionArchetype::VOID_WALKERS, 5.0f);
        m_registry.emplace<FactionDirectiveComponent>(signal, DirectiveType::SYNCHRONICITY, 300, 1.0f, "", 100, 100, 0);
        
        // --- The Arbiter (SYNDICATE) ---
        auto arbiter = m_registry.create();
        m_registry.emplace<NameComponent>(arbiter, "The Arbiter");
        m_registry.emplace<FactionComponent>(arbiter, "SYNDICATE", 60, 1.2f);
        m_registry.emplace<FactionLeaderComponent>(arbiter, "The Arbiter", FactionArchetype::SYNDICATE, 2.5f);
        m_registry.emplace<FactionDirectiveComponent>(arbiter, DirectiveType::UTILITY_BURST, 150, 1.5f);
    }
}

void FactionSystem::update(double delta_time) {
    // L4 update (every 20 turns)
    
    // 1. Update Leaders and Directives
    updateLeaders();
    
    // 2. Apply Directives to Agents (Layer 4 effects)
    applyDirectivesToAgents();

    // 3. Calculate base influence from agents in chunks
    auto chunk_view = m_registry.view<ChunkComponent, FactionInfluenceFieldComponent>();
    auto agent_view = m_registry.view<PositionComponent, Layer4PoliticalComponent>();
    
    // Reset/Decay influence
    for (auto chunk_ent : chunk_view) {
        auto& inf_field = chunk_view.get<FactionInfluenceFieldComponent>(chunk_ent);
        for (auto& [faction, influence] : inf_field.influence) {
            influence *= 0.95f; // 5% decay per L4 tick
        }
    }

    // Accumulate from agents
    for (auto agent_ent : agent_view) {
        auto& pos = agent_view.get<PositionComponent>(agent_ent);
        auto& pol = agent_view.get<Layer4PoliticalComponent>(agent_ent);
        
        // Find chunk for this agent (macro-cell based)
        // Note: Logic assumes chunk mapping exists or we iterate chunks
        // Simplified: iterate chunks and check bounds
        for (auto chunk_ent : chunk_view) {
            auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
            // Assuming chunk_x/y are in macro-cell units (40x40 tiles)
            int min_x = chunk.chunk_x * 40;
            int max_x = (chunk.chunk_x + 1) * 40;
            int min_y = chunk.chunk_y * 40;
            int max_y = (chunk.chunk_y + 1) * 40;

            if (pos.x >= min_x && pos.x < max_x && pos.y >= min_y && pos.y < max_y) {
                auto& inf_field = chunk_view.get<FactionInfluenceFieldComponent>(chunk_ent);
                inf_field.influence[pol.primary_faction] += 2.0f * pol.faction_loyalty;
                break;
            }
        }
    }

    // 2. Diffuse influence
    diffuseInfluence();

    // 3. Aggregate global influence back to the faction leaders
    auto leader_view = m_registry.view<FactionLeaderComponent, FactionComponent>();
    for (auto leader_ent : leader_view) {
        auto& faction = leader_view.get<FactionComponent>(leader_ent);
        float total_inf = 0.0f;
        for (auto chunk_ent : chunk_view) {
            auto& inf_field = chunk_view.get<FactionInfluenceFieldComponent>(chunk_ent);
            if (inf_field.influence.contains(faction.faction_id)) {
                total_inf += inf_field.influence.at(faction.faction_id);
            }
        }
        // Normalize: Assume ~500 influence is 'standard' (500 agents * 1.0 loyalty / chunks)
        // This makes the stock market react to the population's density and loyalty.
        faction.influence = 0.5f + (total_inf / 500.0f);
    }
}

void FactionSystem::diffuseInfluence() {
    // Diffusion logic: factions spread influence to neighboring chunks
    auto chunk_view = m_registry.view<ChunkComponent, FactionInfluenceFieldComponent>();
    
    struct InfluenceDelta {
        entt::entity chunk;
        std::string faction;
        float amount;
    };
    std::vector<InfluenceDelta> deltas;

    for (auto ent : chunk_view) {
        auto& chunk = chunk_view.get<ChunkComponent>(ent);
        auto& inf = chunk_view.get<FactionInfluenceFieldComponent>(ent);

        for (auto& [faction, amount] : inf.influence) {
            if (amount < 1.0f) continue;
            
            float spread = amount * 0.1f; // Spread 10% to neighbors
            // Find neighbors (this is O(N^2) here, but Chunks are sparse-ish. Better to have a map)
            // For now, simple implementation
            for (auto target_ent : chunk_view) {
                if (ent == target_ent) continue;
                auto& target_chunk = chunk_view.get<ChunkComponent>(target_ent);
                int dx = std::abs(chunk.chunk_x - target_chunk.chunk_x);
                int dy = std::abs(chunk.chunk_y - target_chunk.chunk_y);
                if (dx <= 1 && dy <= 1) {
                    deltas.push_back({target_ent, faction, spread});
                }
            }
        }
    }

    for (auto& delta : deltas) {
        m_registry.get<FactionInfluenceFieldComponent>(delta.chunk).influence[delta.faction] += delta.amount;
    }
}

void FactionSystem::handleChangeFactionStanding(const ChangeFactionStandingEvent& event) {
    // Modify global faction-to-faction relations (not fully implemented yet)
    m_dispatcher.enqueue<LogEvent>({
        "Global relation change: " + event.acting_faction_id + " -> " + event.target_faction_id + " " + std::to_string(event.standing_change),
        LogSeverity::INFO,
        "FactionSystem"
    });
}

void FactionSystem::handleAgentReputation(const AgentFactionReputationEvent& event) {
    if (m_registry.all_of<Layer2CognitiveComponent>(event.agent_entity)) {
        auto& cog = m_registry.get<Layer2CognitiveComponent>(event.agent_entity);
        cog.reputation_scores[event.faction_id] += event.change_amount;
        
        m_dispatcher.enqueue<LogEvent>({
            "Agent reputation change: " + event.faction_id + " " + std::to_string(event.change_amount),
            LogSeverity::DEBUG,
            "FactionSystem"
        });
    }
}

void FactionSystem::updateLeaders() {
    auto view = m_registry.view<FactionLeaderComponent, FactionDirectiveComponent>();
    for (auto ent : view) {
        auto& leader = view.get<FactionLeaderComponent>(ent);
        auto& directive = view.get<FactionDirectiveComponent>(ent);

        if (directive.duration_ticks > 0) {
            directive.duration_ticks--;
        } else {
            // Pick a new directive based on archetype
            switch(leader.archetype) {
                case FactionArchetype::CONSENSUS:
                    directive.active_directive = (rand() % 2 == 0) ? DirectiveType::ROUTINE_ENFORCEMENT : DirectiveType::CRACKDOWN;
                    directive.duration_ticks = 100 + (rand() % 100);
                    break;
                case FactionArchetype::ENTROPIC_DRIFT:
                    directive.active_directive = DirectiveType::UTILITY_BURST;
                    directive.duration_ticks = 50 + (rand() % 50);
                    directive.magnitude = 2.0f + (float)(rand() % 5);
                    break;
                case FactionArchetype::SILICON_MAW:
                    directive.active_directive = DirectiveType::BIOLOGICAL_OVERRIDE;
                    directive.duration_ticks = 200 + (rand() % 200);
                    break;
                case FactionArchetype::VOID_WALKERS:
                    directive.active_directive = DirectiveType::SYNCHRONICITY;
                    directive.duration_ticks = 50 + (rand() % 50);
                    directive.target_x = rand() % 200;
                    directive.target_y = rand() % 200;
                    break;
                case FactionArchetype::SYNDICATE:
                    directive.active_directive = DirectiveType::UTILITY_BURST;
                    directive.duration_ticks = 80 + (rand() % 80);
                    directive.magnitude = 3.0f;
                    break;
                default:
                    directive.active_directive = DirectiveType::NONE;
            }
        }
    }
}

void FactionSystem::applyDirectivesToAgents() {
    // This logic modifies the agent's internal state based on their faction leader's directive
    auto agent_view = m_registry.view<Layer4PoliticalComponent, Layer1BiologyComponent, NeedsComponent>();
    auto leader_view = m_registry.view<FactionLeaderComponent, FactionDirectiveComponent, FactionComponent>();

    for (auto agent_ent : agent_view) {
        auto& pol = agent_view.get<Layer4PoliticalComponent>(agent_ent);
        auto& bio = agent_view.get<Layer1BiologyComponent>(agent_ent);
        auto& needs = agent_view.get<NeedsComponent>(agent_ent);

        // Find the agent's leader
        for (auto leader_ent : leader_view) {
            auto& f_comp = leader_view.get<FactionComponent>(leader_ent);
            if (f_comp.faction_id == pol.primary_faction) {
                auto& directive = leader_view.get<FactionDirectiveComponent>(leader_ent);
                
                // Mechanical override: BIOLOGICAL_OVERRIDE (SILICON_MAW)
                if (directive.active_directive == DirectiveType::BIOLOGICAL_OVERRIDE) {
                    bio.metabolic_rate = 0.5f; // Half hunger/thirst decay
                    // If they are MAW, they might have special needs.
                    // For now, simple metabolic reduction.
                } else {
                    bio.metabolic_rate = 1.0f; // Reset if directive ends
                }

                // Synchronicity (VOID_WALKERS)
                if (directive.active_directive == DirectiveType::SYNCHRONICITY) {
                    // Force the agent's goal to the target location
                    if (m_registry.all_of<GoalComponent>(agent_ent)) {
                        auto& goal = m_registry.get<GoalComponent>(agent_ent);
                        goal.target_x = directive.target_x;
                        goal.target_y = directive.target_y;
                        goal.target_layer = directive.target_layer;
                    }
                }
            }
        }
    }
}
} // namespace NeonOubliette
