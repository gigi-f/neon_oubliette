#include "faction_system.h"
#include <algorithm>
#include <cmath>
#include "../components/lod_components.h"
#include "../components/religion_components.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"

namespace NeonOubliette {

FactionSystem::FactionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher)
{
    m_dispatcher.sink<ChangeFactionStandingEvent>().connect<&FactionSystem::handleChangeFactionStanding>(*this);
    m_dispatcher.sink<AgentFactionReputationEvent>().connect<&FactionSystem::handleAgentReputation>(*this);
    m_dispatcher.sink<BackroomDealEvent>().connect<&FactionSystem::handleBackroomDeal>(*this);
    m_dispatcher.sink<CrisisEffectEvent>().connect<&FactionSystem::handleCrisisEffect>(*this);
}

void FactionSystem::initialize() {
    // 0. Ensure Global Tension singleton
    auto tension_view = m_registry.view<GlobalFactionTensionComponent>();
    if (tension_view.empty()) {
        auto entity = m_registry.create();
        m_registry.emplace<GlobalFactionTensionComponent>(entity);
    }

    // 0.1 Ensure Public Opinion singleton
    auto opinion_view = m_registry.view<PublicOpinionComponent>();
    if (opinion_view.empty()) {
        auto entity = m_registry.create();
        m_registry.emplace<PublicOpinionComponent>(entity);
    }

    // 1. Create the four faction leader entities if they don't exist
    auto leader_view = m_registry.view<FactionLeaderComponent>();
    if (leader_view.empty()) {
        // --- Aura-9 (CONSENSUS) ---
        auto aura9 = m_registry.create();
        m_registry.emplace<NameComponent>(aura9, "Aura-9");
        m_registry.emplace<FactionComponent>(aura9, "GOVERNMENT", "CORPORATE", 100, 1.0f);
        m_registry.emplace<FactionLeaderComponent>(aura9, "Aura-9", FactionArchetype::CONSENSUS, 2.0f);
        m_registry.emplace<FactionDirectiveComponent>(aura9, DirectiveType::ROUTINE_ENFORCEMENT, 100, 1.5f);
        auto& stance9 = m_registry.emplace<FactionReligionStanceComponent>(aura9);
        stance9.stances["SYNTH_GOSPEL"] = ReligionStance::PATRON;
        stance9.stances["STREET_PANTHEON"] = ReligionStance::SUPPRESSOR;
        stance9.stances["HIERODULE_CULT"] = ReligionStance::SUPPRESSOR;

        // --- Malware-Alpha (ENTROPIC_DRIFT) ---
        auto malware = m_registry.create();
        m_registry.emplace<NameComponent>(malware, "Malware-Alpha");
        m_registry.emplace<FactionComponent>(malware, "REBEL", "COLLECTIVE", 50, 0.8f);
        m_registry.emplace<FactionLeaderComponent>(malware, "Malware-Alpha", FactionArchetype::ENTROPIC_DRIFT, 1.2f);
        m_registry.emplace<FactionDirectiveComponent>(malware, DirectiveType::UTILITY_BURST, 50, 2.0f);
        auto& stanceM = m_registry.emplace<FactionReligionStanceComponent>(malware);
        stanceM.stances["ASCETICS_OF_WIRE"] = ReligionStance::PATRON;
        stanceM.stances["SYNTH_GOSPEL"] = ReligionStance::SUPPRESSOR;

        // --- The Architect (SILICON_MAW) ---
        auto architect = m_registry.create();
        m_registry.emplace<NameComponent>(architect, "The Architect");
        m_registry.emplace<FactionComponent>(architect, "MAW", "CACOGEN", 75, 1.5f);
        m_registry.emplace<FactionLeaderComponent>(architect, "The Architect", FactionArchetype::SILICON_MAW, 3.0f);
        m_registry.emplace<FactionDirectiveComponent>(architect, DirectiveType::BIOLOGICAL_OVERRIDE, 200, 1.0f);
        auto& stanceA = m_registry.emplace<FactionReligionStanceComponent>(architect);
        stanceA.stances["THE_VOID_EYE"] = ReligionStance::PATRON;
        stanceA.stances["HIERODULE_CULT"] = ReligionStance::NEUTRAL;

        // --- The Signal (VOID_WALKERS) ---
        auto signal = m_registry.create();
        m_registry.emplace<NameComponent>(signal, "The Signal");
        m_registry.emplace<FactionComponent>(signal, "VOID", "HIERODULE", 30, 0.5f);
        m_registry.emplace<FactionLeaderComponent>(signal, "The Signal", FactionArchetype::VOID_WALKERS, 5.0f);
        m_registry.emplace<FactionDirectiveComponent>(signal, DirectiveType::SYNCHRONICITY, 300, 1.0f, "", 100, 100, 0);
        auto& stanceS = m_registry.emplace<FactionReligionStanceComponent>(signal);
        stanceS.stances["HIERODULE_CULT"] = ReligionStance::PATRON;
        stanceS.stances["SOL_INVICTUS"] = ReligionStance::PATRON;

        // --- The Arbiter (SYNDICATE) ---
        auto arbiter = m_registry.create();
        m_registry.emplace<NameComponent>(arbiter, "The Arbiter");
        m_registry.emplace<FactionComponent>(arbiter, "SYNDICATE", "SYNDICATE", 60, 1.2f);
        m_registry.emplace<FactionLeaderComponent>(arbiter, "The Arbiter", FactionArchetype::SYNDICATE, 2.5f);
        m_registry.emplace<FactionDirectiveComponent>(arbiter, DirectiveType::UTILITY_BURST, 150, 1.5f);
        auto& stanceAr = m_registry.emplace<FactionReligionStanceComponent>(arbiter);
        stanceAr.stances["STREET_PANTHEON"] = ReligionStance::PATRON;
    }
}

void FactionSystem::update(double delta_time) {
    // L4 update (every 20 turns)
    
    // 0. Update Global Tension and Relations
    updatePoliticalClimate();

    // 1. Update Leaders and Directives
    updateLeaders();

    // 2. Update agent affinities and check for conversions
    updateAgentAffinities();
    
    // 3. Apply Directives to Agents (Layer 4 effects)
    applyDirectivesToAgents();

    // 4. Calculate base influence from agents in chunks
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

    // 5. Diffuse influence
    diffuseInfluence();

    // 6. Update Reputation Decay (P.4)
    updateReputationDecay();

    // 7. Aggregate global influence back to the faction leaders
    auto leader_view = m_registry.view<FactionLeaderComponent, FactionComponent, FactionReligionStanceComponent>();
    for (auto leader_ent : leader_view) {
        auto& faction = leader_view.get<FactionComponent>(leader_ent);
        auto& religion_stances = leader_view.get<FactionReligionStanceComponent>(leader_ent);

        float total_inf = 0.0f;
        for (auto chunk_ent : chunk_view) {
            auto& inf_field = chunk_view.get<FactionInfluenceFieldComponent>(chunk_ent);
            if (inf_field.influence.contains(faction.faction_id)) {
                float chunk_inf = inf_field.influence.at(faction.faction_id);
                total_inf += chunk_inf;

                // [H.5] SUPPRESSOR Raids
                // If faction has high influence in this chunk and is a suppressor of a religion
                // check if that religion has a worship place here.
                if (chunk_inf > 50.0f) {
                    for (auto& [rel_id, stance] : religion_stances.stances) {
                        if (stance == ReligionStance::SUPPRESSOR) {
                            // Check for worship places of this religion in this chunk
                            auto worship_view = m_registry.view<WorshipPlaceComponent, PositionComponent>();
                            for (auto w_ent : worship_view) {
                                auto& wp = worship_view.get<WorshipPlaceComponent>(w_ent);
                                if (wp.religion_id == rel_id) {
                                    auto& w_pos = worship_view.get<PositionComponent>(w_ent);
                                    
                                    // Simple chunk check:
                                    auto& chunk = m_registry.get<ChunkComponent>(chunk_ent);
                                    if (w_pos.x >= chunk.chunk_x * 40 && w_pos.x < (chunk.chunk_x + 1) * 40 &&
                                        w_pos.y >= chunk.chunk_y * 40 && w_pos.y < (chunk.chunk_y + 1) * 40) {
                                        
                                        // 5% chance to raid per L4 tick
                                        if ((rand() % 100) < 5) {
                                            m_dispatcher.enqueue<RaidEvent>({
                                                leader_ent,
                                                rel_id,
                                                w_ent,
                                                w_pos.x, w_pos.y
                                            });
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        // Normalize: Assume ~500 influence is 'standard' (500 agents * 1.0 loyalty / chunks)
        faction.influence = 0.5f + (total_inf / 500.0f);
    }
}

void FactionSystem::updatePoliticalClimate() {
    auto tension_view = m_registry.view<GlobalFactionTensionComponent>();
    if (tension_view.empty()) return;
    auto& tension = tension_view.get<GlobalFactionTensionComponent>(tension_view.front());

    // 1. Calculate tension based on influence overlap in chunks
    auto chunk_view = m_registry.view<FactionInfluenceFieldComponent>();
    for (auto chunk_ent : chunk_view) {
        auto& inf = chunk_view.get<FactionInfluenceFieldComponent>(chunk_ent);
        if (inf.influence.size() < 2) continue;

        for (auto it1 = inf.influence.begin(); it1 != inf.influence.end(); ++it1) {
            for (auto it2 = std::next(it1); it2 != inf.influence.end(); ++it2) {
                // High overlap in a single chunk increases tension
                float overlap = std::min(it1->second, it2->second);
                if (overlap > 10.0f) {
                    std::pair<std::string, std::string> pair = {it1->first, it2->first};
                    if (pair.first > pair.second) std::swap(pair.first, pair.second);
                    
                    tension.relations[pair] -= overlap * 0.01f; // Slow deterioration
                }
            }
        }
    }

    // 2. Natural relation recovery (Drift towards neutral 0)
    for (auto& [pair, standing] : tension.relations) {
        standing *= 0.99f;
    }

    // 3. Update public opinion [L.4]
    auto opinion_view = m_registry.view<PublicOpinionComponent>();
    if (!opinion_view.empty()) {
        auto& opinion = opinion_view.get<PublicOpinionComponent>(opinion_view.front());
        auto pol_view = m_registry.view<Layer4PoliticalComponent, Layer2CognitiveComponent>();
        
        std::map<std::string, float> aggregate_approval;
        std::map<std::string, int> faction_counts;

        for (auto entity : pol_view) {
            auto& pol = pol_view.get<Layer4PoliticalComponent>(entity);
            auto& cog = pol_view.get<Layer2CognitiveComponent>(entity);
            
            if (cog.reputation_scores.contains(pol.primary_faction)) {
                aggregate_approval[pol.primary_faction] += cog.reputation_scores.at(pol.primary_faction);
                faction_counts[pol.primary_faction]++;
            }
        }

        for (auto& [f, total] : aggregate_approval) {
            opinion.faction_approval[f] = total / (float)faction_counts[f];
        }
    }
}

void FactionSystem::handleCrisisEffect(const CrisisEffectEvent& event) {
    if (event.type == CrisisType::POLITICAL_UNREST) {
        // Boost Rebel/Syndicate influence and decrease Government influence
        auto chunk_view = m_registry.view<FactionInfluenceFieldComponent>();
        for (auto entity : chunk_view) {
            auto& inf = chunk_view.get<FactionInfluenceFieldComponent>(entity);
            if (inf.influence.contains("REBEL")) inf.influence["REBEL"] += event.intensity * 2.0f;
            if (inf.influence.contains("SYNDICATE")) inf.influence["SYNDICATE"] += event.intensity * 2.0f;
            if (inf.influence.contains("GOVERNMENT")) inf.influence["GOVERNMENT"] -= event.intensity * 1.0f;
        }

        // Increase agent frustration globally during unrest
        auto agent_view = m_registry.view<NeedsComponent>();
        for (auto entity : agent_view) {
            agent_view.get<NeedsComponent>(entity).frustration += event.intensity * 0.5f;
        }
    } else if (event.type == CrisisType::FACTION_WAR) {
        // Warring factions gain influence in chunks they are already present
        // Identify warring factions from Global Tension
        auto tension_view = m_registry.view<GlobalFactionTensionComponent>();
        if (tension_view.empty()) return;
        auto& tension = tension_view.get<GlobalFactionTensionComponent>(tension_view.front());

        for (auto const& [pair, standing] : tension.relations) {
            if (standing < -50.0f) {
                // These factions are effectively at war
                auto chunk_view = m_registry.view<FactionInfluenceFieldComponent>();
                for (auto entity : chunk_view) {
                    auto& inf = chunk_view.get<FactionInfluenceFieldComponent>(entity);
                    if (inf.influence.contains(pair.first) && inf.influence.contains(pair.second)) {
                        // Winner takes some influence from loser in each chunk based on pulse
                        if (inf.influence[pair.first] > inf.influence[pair.second]) {
                            float shift = event.intensity * 5.0f;
                            inf.influence[pair.first] += shift;
                            inf.influence[pair.second] = std::max(0.0f, inf.influence[pair.second] - shift);
                        } else {
                            float shift = event.intensity * 5.0f;
                            inf.influence[pair.second] += shift;
                            inf.influence[pair.first] = std::max(0.0f, inf.influence[pair.first] - shift);
                        }
                    }
                }
            }
        }
    }
}

void FactionSystem::handleChangeFactionStanding(const ChangeFactionStandingEvent& event) {
    auto tension_view = m_registry.view<GlobalFactionTensionComponent>();
    if (!tension_view.empty()) {
        auto& tension = tension_view.get<GlobalFactionTensionComponent>(tension_view.front());
        std::pair<std::string, std::string> pair = {event.acting_faction_id, event.target_faction_id};
        if (pair.first > pair.second) std::swap(pair.first, pair.second);
        tension.relations[pair] += event.standing_change;
        tension.relations[pair] = std::clamp(tension.relations[pair], -100.0f, 100.0f);
    }

    m_dispatcher.enqueue<LogEvent>({
        "Global relation change: " + event.acting_faction_id + " -> " + event.target_faction_id + " " + std::to_string(event.standing_change),
        LogSeverity::INFO,
        "FactionSystem"
    });

    if (std::abs(event.standing_change) >= 20.0f) {
        std::string type = (event.standing_change > 0) ? "ALLIANCE_SHIFT" : "FACTION_FEUD";
        std::string desc = "Relation between " + event.acting_faction_id + " and " + event.target_faction_id + 
                           (event.standing_change > 0 ? " improved significantly." : " deteriorated into a feud.");
        
        m_dispatcher.enqueue<MilestoneEvent>({
            type,
            desc,
            event.acting_faction_id,
            event.target_faction_id,
            entt::null,
            3.0f // High importance
        });
    }
}

void FactionSystem::handleAgentReputation(const AgentFactionReputationEvent& event) {
    if (m_registry.all_of<Layer2CognitiveComponent>(event.agent_entity)) {
        auto& cog = m_registry.get<Layer2CognitiveComponent>(event.agent_entity);
        cog.reputation_scores[event.faction_id] += event.change_amount;
        
        m_dispatcher.enqueue<LogEvent>({
            "Agent cognitive reputation change: " + event.faction_id + " " + std::to_string(event.change_amount),
            LogSeverity::DEBUG,
            "FactionSystem"
        });
    }
    
    // [P.1] Handle ReputationComponent specifically for Player or entities with fame tracking
    if (m_registry.all_of<ReputationComponent>(event.agent_entity)) {
        auto& rep = m_registry.get<ReputationComponent>(event.agent_entity);
        rep.faction_standing[event.faction_id] += event.change_amount;
        
        // Fame increases reputation shift (fame * change_amount)
        // This simulates actions of famous people being more impactful.
        // For now, let's say fame scales reputation shifts.
        // rep.faction_standing[event.faction_id] += event.change_amount * rep.fame; // Optional: discuss with designer

        m_dispatcher.enqueue<LogEvent>({
            "ReputationComponent change: " + event.faction_id + " " + std::to_string(event.change_amount),
            LogSeverity::INFO,
            "FactionSystem"
        });
    }
}

void FactionSystem::updateLeaders() {
    auto view = m_registry.view<FactionLeaderComponent, FactionDirectiveComponent>();
    
    // Check if any political crisis is active
    bool crisis_active = false;
    float crisis_severity = 0.0f;
    auto crisis_view = m_registry.view<CrisisComponent>();
    if (!crisis_view.empty()) {
        auto& cc = crisis_view.get<CrisisComponent>(crisis_view.front());
        for (const auto& ac : cc.active_crises) {
            if (ac.type == CrisisType::POLITICAL_UNREST || ac.type == CrisisType::FACTION_WAR) {
                crisis_active = true;
                crisis_severity = std::max(crisis_severity, ac.severity);
            }
        }
    }

    for (auto ent : view) {
        auto& leader = view.get<FactionLeaderComponent>(ent);
        auto& directive = view.get<FactionDirectiveComponent>(ent);

        if (directive.duration_ticks > 0) {
            directive.duration_ticks--;
        } else {
            // Pick a new directive based on archetype
            switch(leader.archetype) {
                case FactionArchetype::CONSENSUS:
                    // Aura-9 (Government) becomes more repressive during crises
                    if (crisis_active) {
                        directive.active_directive = DirectiveType::CRACKDOWN;
                        directive.duration_ticks = 200;
                        directive.magnitude = 2.0f;
                    } else {
                        directive.active_directive = (rand() % 2 == 0) ? DirectiveType::ROUTINE_ENFORCEMENT : DirectiveType::CRACKDOWN;
                        directive.duration_ticks = 100 + (rand() % 100);
                    }
                    break;
                case FactionArchetype::ENTROPIC_DRIFT:
                    // Malware-Alpha exploits chaos
                    directive.active_directive = DirectiveType::UTILITY_BURST;
                    directive.duration_ticks = 50 + (rand() % 50);
                    directive.magnitude = (crisis_active ? 4.0f : 2.0f) + (float)(rand() % 5);
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
                    // Syndicate fuels unrest
                    directive.active_directive = (crisis_active && (rand() % 2 == 0)) ? DirectiveType::UTILITY_BURST : DirectiveType::NONE;
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

void FactionSystem::handleBackroomDeal(const BackroomDealEvent& event) {
    if (!m_registry.valid(event.faction_a) || !m_registry.valid(event.faction_b)) return;
    
    auto& name_a = m_registry.get<NameComponent>(event.faction_a).name;
    auto& name_b = m_registry.get<NameComponent>(event.faction_b).name;
    
    m_dispatcher.enqueue<MilestoneEvent>({
        "FACTION_DEAL",
        "A backroom deal was struck between " + name_a + " and " + name_b + ".",
        name_a,
        name_b,
        entt::null,
        4.0f // Very important
    });
}

void FactionSystem::updateAgentAffinities() {
    auto view = m_registry.view<Layer4PoliticalComponent, Layer2CognitiveComponent, PositionComponent, AgeComponent>();
    auto chunk_view = m_registry.view<ChunkComponent, FactionInfluenceFieldComponent>();

    for (auto entity : view) {
        auto& pol = view.get<Layer4PoliticalComponent>(entity);
        auto& cog = view.get<Layer2CognitiveComponent>(entity);
        auto& pos = view.get<PositionComponent>(entity);
        auto& age = view.get<AgeComponent>(entity);
        
        // [J.5] Environmental Drift: Young agents are more influenced by the local faction
        if (age.stage == LifeStage::CHILD || age.stage == LifeStage::YOUNG_ADULT) {
             for (auto chunk_ent : chunk_view) {
                auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
                if (pos.x >= chunk.chunk_x * 40 && pos.x < (chunk.chunk_x + 1) * 40 &&
                    pos.y >= chunk.chunk_y * 40 && pos.y < (chunk.chunk_y + 1) * 40) {
                    auto& inf_field = chunk_view.get<FactionInfluenceFieldComponent>(chunk_ent);
                    for (auto const& [faction_id, amount] : inf_field.influence) {
                        // Drift reputation scores toward local influence
                        float drift = amount * 0.005f; // Slow drift per L4 tick
                        cog.reputation_scores[faction_id] += drift;
                    }
                    break;
                }
            }
        }
        
        // Find faction with highest reputation
        std::string current_faction = pol.primary_faction;
        float best_score = -1000.0f;
        if (cog.reputation_scores.contains(current_faction)) {
            best_score = cog.reputation_scores.at(current_faction);
        }
        
        std::string best_faction = current_faction;
        bool changed = false;
        
        for (auto& [faction_id, score] : cog.reputation_scores) {
            if (score > best_score + 25.0f) { // Require significant lead to switch
                best_score = score;
                best_faction = faction_id;
                changed = true;
            }
        }
        
        if (changed) {
            std::string old_faction = current_faction;
            pol.primary_faction = best_faction;
            
            auto& name = m_registry.get<NameComponent>(entity).name;
            m_dispatcher.enqueue<MilestoneEvent>({
                "CONVERSION",
                name + " has converted from " + old_faction + " to " + best_faction + ".",
                old_faction,
                best_faction,
                entity,
                2.0f // Medium importance
            });
        }
    }
}

void FactionSystem::diffuseInfluence() {
    auto chunk_view = m_registry.view<ChunkComponent, FactionInfluenceFieldComponent>();
    
    // Multi-pass diffusion (simplified)
    for (auto entity : chunk_view) {
        auto& chunk = chunk_view.get<ChunkComponent>(entity);
        auto& inf_field = chunk_view.get<FactionInfluenceFieldComponent>(entity);
        
        for (auto target_ent : chunk_view) {
            if (entity == target_ent) continue;
            auto& target_chunk = chunk_view.get<ChunkComponent>(target_ent);
            
            int dx = std::abs(chunk.chunk_x - target_chunk.chunk_x);
            int dy = std::abs(chunk.chunk_y - target_chunk.chunk_y);
            
            if (dx <= 1 && dy <= 1) {
                auto& target_inf = chunk_view.get<FactionInfluenceFieldComponent>(target_ent);
                for (auto const& [f, amount] : inf_field.influence) {
                    // Diffuse 5% to neighbors
                    target_inf.influence[f] += amount * 0.05f;
                }
            }
        }
    }
}

void FactionSystem::updateReputationDecay() {
    auto rep_view = m_registry.view<ReputationComponent>();
    
    for (auto entity : rep_view) {
        auto& rep = rep_view.get<ReputationComponent>(entity);
        
        // 1. Fame Decay: Actions of the past fade unless refreshed.
        // 2% decay per L4 tick (approx 10% every 100 turns).
        rep.fame *= 0.98f;
        if (rep.fame < 0.01f) rep.fame = 0.0f;

        // 2. Standing Decay (Drift toward Neutral)
        // High Fame slows down the decay of standing (people remember your deeds).
        // 0 Fame -> 5% decay per L4 tick.
        // 1.0 Fame -> 2.5% decay per L4 tick.
        float fame_protection = 1.0f - (rep.fame * 0.5f);
        float decay_rate = 0.95f + (0.025f * rep.fame); 

        for (auto& [faction_id, standing] : rep.faction_standing) {
            // Only decay if non-zero
            if (std::abs(standing) > 0.1f) {
                standing *= decay_rate;
                
                // Clamp very small values to zero
                if (std::abs(standing) < 0.1f) {
                    standing = 0.0f;
                }
            }
        }

        // [P.4 Recovery Logic] 
        // Recovery is handled by ensuring that pro-faction events (AgentFactionReputationEvent)
        // add a significant enough 'bump' to outpace the decay for active players.
        // We could also implement a "Pardon" mechanism if we want specific recovery tasks.
    }
}

} // namespace NeonOubliette
