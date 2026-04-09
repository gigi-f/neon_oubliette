#include "religion_system.h"
#include <iostream>
#include <fstream>
#include <random>
#include <algorithm>
#include "nlohmann/json.hpp"
#include "src/config/ConfigLoader.h"
#include "../components/lod_components.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"

namespace NeonOubliette {

ReligionSystem::ReligionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher)
{
    m_dispatcher.sink<WorshipEvent>().connect<&ReligionSystem::handleWorship>(*this);
    m_dispatcher.sink<ProselytizingEvent>().connect<&ReligionSystem::handleProselytizing>(*this);
    m_dispatcher.sink<HolyDayEvent>().connect<&ReligionSystem::handleHolyDay>(*this);
    m_dispatcher.sink<RaidEvent>().connect<&ReligionSystem::handleRaid>(*this);
}

void ReligionSystem::initialize() {
    // Initialize political influence tracking
    if (!m_registry.ctx().contains<ReligionInfluenceRegistry>()) {
        m_registry.ctx().emplace<ReligionInfluenceRegistry>();
    }

    loadReligions();
}

void ReligionSystem::update(double delta_time) {
    // [MOD] Get current tick from CityComponent instead of local increment
    auto city_view = m_registry.view<CityComponent>();
    if (!city_view.empty()) {
        m_current_tick = city_view.get<CityComponent>(city_view.front()).time_tick;
    } else {
        m_current_tick += 20; // Fallback
    }

    updateDevotion();
    updateInfluence();
    diffuseInfluence();
    checkHolyDays();
    updateProcessions();
    handleProcessionTension();
}

void ReligionSystem::loadReligions() {
    try {
        Config::ConfigLoader loader("data/configs/", "data/schemas/");
        
        std::string fullPath = "data/configs/religions.json";
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            std::cerr << "[ReligionSystem] Failed to open religions config: " << fullPath << std::endl;
            return;
        }

        nlohmann::json j;
        file >> j;

        if (!m_registry.ctx().contains<ReligionRegistryComponent>()) {
            m_registry.ctx().emplace<ReligionRegistryComponent>();
        }
        auto& registry_comp = *m_registry.ctx().find<ReligionRegistryComponent>();
        auto& influence_registry = *m_registry.ctx().find<ReligionInfluenceRegistry>();
        
        for (const auto& rel_json : j["religions"]) {
            ReligionRecord record = rel_json.get<ReligionRecord>();
            registry_comp.religions[record.religion_id] = record;
            
            // Initialize influence entry if missing
            if (!influence_registry.religions.contains(record.religion_id)) {
                influence_registry.religions[record.religion_id] = ReligionInfluenceComponent();
            }

            m_dispatcher.enqueue<LogEvent>({
                "Loaded religion: " + record.name + " (" + record.religion_id + ")",
                LogSeverity::INFO,
                "ReligionSystem"
            });
        }

        std::cout << "[ReligionSystem] Loaded " << registry_comp.religions.size() << " religions." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[ReligionSystem] Error loading religions: " << e.what() << std::endl;
    }
}

void ReligionSystem::updateDevotion() {
    auto* registry_ptr = m_registry.ctx().find<ReligionRegistryComponent>();
    if (!registry_ptr) return;

    // Cache faction leader stances for faster lookup
    std::map<std::string, std::map<std::string, ReligionStance>> faction_stances;
    auto leader_view = m_registry.view<FactionComponent, FactionReligionStanceComponent>();
    for (auto leader_ent : leader_view) {
        auto& faction = leader_view.get<FactionComponent>(leader_ent);
        auto& stances = leader_view.get<FactionReligionStanceComponent>(leader_ent);
        faction_stances[faction.faction_id] = stances.stances;
    }

    // [H.2] Devotion decays slightly if not worshipping, and affects mood/faction drift
    auto view = m_registry.view<ReligiosityComponent, PositionComponent, AgeComponent>();
    auto chunk_view = m_registry.view<ChunkComponent, ReligionInfluenceFieldComponent>();
    int cs = get_chunk_size(m_registry);

    for (auto entity : view) {
        auto& religiosity = view.get<ReligiosityComponent>(entity);
        auto& pos = view.get<PositionComponent>(entity);
        auto& age = view.get<AgeComponent>(entity);
        
        // [J.5] Environmental Drift: Young agents are more influenced by local religion
        if (age.stage == LifeStage::CHILD || age.stage == LifeStage::YOUNG_ADULT) {
             for (auto chunk_ent : chunk_view) {
                auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
                if (pos.x >= chunk.chunk_x * cs && pos.x < (chunk.chunk_x + 1) * cs &&
                    pos.y >= chunk.chunk_y * cs && pos.y < (chunk.chunk_y + 1) * cs) {
                    auto& inf_field = chunk_view.get<ReligionInfluenceFieldComponent>(chunk_ent);
                    for (auto const& [religion_id, amount] : inf_field.influence) {
                        // Drift devotion toward local influence
                        if (religiosity.religion_id == religion_id) {
                            religiosity.devotion = std::min(100.0f, religiosity.devotion + amount * 0.005f);
                        } else if (amount > 50.0f && religiosity.devotion < 15.0f) {
                            // Small chance to switch to dominant local religion if current devotion is very low
                            if ((rand() % 1000) < 1) { // 0.1% chance per L4 tick
                                religiosity.religion_id = religion_id;
                                religiosity.devotion = 5.0f;
                            }
                        }
                    }
                    break;
                }
            }
        }
        
        // Simple decay: 0.1 per L4 tick if last worship was long ago
        if (m_current_tick - religiosity.last_worship_tick > 1000) {
            religiosity.devotion = std::max(0.0f, religiosity.devotion - 0.1f);
        }

        // [H.5] Faction Stance Interaction
        if (m_registry.all_of<Layer4PoliticalComponent>(entity)) {
            auto& pol = m_registry.get<Layer4PoliticalComponent>(entity);
            if (faction_stances.contains(pol.primary_faction)) {
                auto const& stances = faction_stances.at(pol.primary_faction);
                if (stances.contains(religiosity.religion_id)) {
                    ReligionStance stance = stances.at(religiosity.religion_id);
                    if (stance == ReligionStance::PATRON) {
                        religiosity.devotion = std::min(100.0f, religiosity.devotion + 0.2f);
                    } else if (stance == ReligionStance::SUPPRESSOR) {
                        religiosity.devotion = std::max(0.0f, religiosity.devotion - 0.5f);
                    }
                }
            }
        }

        // [H.2] Devotion affects mood (Frustration)
        if (m_registry.all_of<NeedsComponent>(entity)) {
            auto& needs = m_registry.get<NeedsComponent>(entity);
            // If recently worshipped, frustration reduces faster
            if (m_current_tick - religiosity.last_worship_tick < 200) {
                float relief = 0.5f * (religiosity.devotion / 100.0f);
                needs.frustration = std::max(0.0f, needs.frustration - relief);
            }
        }

        // [H.2] Faction alignment drift
        auto rel_it = registry_ptr->religions.find(religiosity.religion_id);
        if (rel_it != registry_ptr->religions.end()) {
            const auto& religion = rel_it->second;
            
            // Influence Faction Loyalty (Layer 4)
            if (m_registry.all_of<Layer4PoliticalComponent>(entity)) {
                auto& pol = m_registry.get<Layer4PoliticalComponent>(entity);
                
                for (auto const& [faction_id, modifier] : religion.faction_affinity_modifiers) {
                    if (pol.primary_faction == faction_id) {
                        // Drift loyalty toward the religion's stance
                        float drift = modifier * (religiosity.devotion / 100.0f) * 0.01f;
                        pol.faction_loyalty = std::clamp(pol.faction_loyalty + drift, 0.0f, 1.0f);
                    }
                }
            }

            // Influence Reputation Scores (Layer 2)
            if (m_registry.all_of<Layer2CognitiveComponent>(entity)) {
                auto& cog = m_registry.get<Layer2CognitiveComponent>(entity);
                for (auto const& [faction_id, modifier] : religion.faction_affinity_modifiers) {
                    float drift = modifier * (religiosity.devotion / 100.0f) * 0.05f;
                    cog.reputation_scores[faction_id] = std::clamp(cog.reputation_scores[faction_id] + drift, -100.0f, 100.0f);
                }
            }
        }
    }
}

void ReligionSystem::updateInfluence() {
    auto* influence_registry = m_registry.ctx().find<ReligionInfluenceRegistry>();
    if (!influence_registry) return;

    auto* rel_registry = m_registry.ctx().find<ReligionRegistryComponent>();
    if (!rel_registry) return;

    // Reset influence counters for this tick
    for (auto& [id, infl] : influence_registry->religions) {
        infl.global_influence = 0.0f;
    }

    // [J.5] Chunk-level influence reset
    auto chunk_view = m_registry.view<ChunkComponent, ReligionInfluenceFieldComponent>();
    for (auto chunk_ent : chunk_view) {
        auto& inf_field = chunk_view.get<ReligionInfluenceFieldComponent>(chunk_ent);
        for (auto& [rel, amount] : inf_field.influence) {
            amount *= 0.95f; // 5% decay per L4 tick
        }
    }

    // Accumulate influence from agents
    int cs = get_chunk_size(m_registry);
    auto agent_view = m_registry.view<ReligiosityComponent, PositionComponent>();
    for (auto agent : agent_view) {
        auto& religiosity = agent_view.get<ReligiosityComponent>(agent);
        auto& pos = agent_view.get<PositionComponent>(agent);

        if (influence_registry->religions.contains(religiosity.religion_id)) {
            float inf_contrib = (religiosity.devotion / 10.0f);
            influence_registry->religions[religiosity.religion_id].global_influence += inf_contrib;

            // [J.5] Chunk-level accumulation
            for (auto chunk_ent : chunk_view) {
                auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
                if (pos.x >= chunk.chunk_x * cs && pos.x < (chunk.chunk_x + 1) * cs &&
                    pos.y >= chunk.chunk_y * cs && pos.y < (chunk.chunk_y + 1) * cs) {
                    auto& inf_field = chunk_view.get<ReligionInfluenceFieldComponent>(chunk_ent);
                    inf_field.influence[religiosity.religion_id] += inf_contrib * 0.1f;
                    break;
                }
            }
        }
    }

    // Check for suppression tension and emergent factions
    // A religion is "suppressed" if any of its followers are being suppressed by their faction.
    // Simpler: a religion is suppressed if a major faction (GOVERNMENT) has it marked as SUPPRESSOR.
    
    std::set<std::string> suppressed_religions;
    auto leader_view = m_registry.view<FactionReligionStanceComponent, FactionComponent>();
    for (auto leader_ent : leader_view) {
        auto& faction = leader_view.get<FactionComponent>(leader_ent);
        if (faction.faction_id == "GOVERNMENT") {
            auto& stances = leader_view.get<FactionReligionStanceComponent>(leader_ent);
            for (auto const& [rel_id, stance] : stances.stances) {
                if (stance == ReligionStance::SUPPRESSOR) {
                    suppressed_religions.insert(rel_id);
                }
            }
        }
    }

    for (auto& [id, infl] : influence_registry->religions) {
        if (suppressed_religions.contains(id)) {
            // Increase tension if suppressed and has high influence
            if (infl.global_influence > 100.0f) {
                infl.suppression_tension += (infl.global_influence / 100.0f);
                
                // [H.5] Emergent Politicization
                if (infl.suppression_tension > 500.0f) {
                    spawnEmergentFaction(id);
                    infl.suppression_tension = 0.0f; // Reset after spawning
                }
            }
        } else {
            // Decay tension if not suppressed
            infl.suppression_tension = std::max(0.0f, infl.suppression_tension - 1.0f);
        }
    }
}

void ReligionSystem::handleRaid(const RaidEvent& event) {
    // [H.5] Faction suppression raid
    // Deduct devotion from agents of that religion in the chunk
    
    // Find agents in chunk
    auto agent_view = m_registry.view<ReligiosityComponent, PositionComponent>();
    int count = 0;
    for (auto agent : agent_view) {
        auto& religiosity = agent_view.get<ReligiosityComponent>(agent);
        if (religiosity.religion_id == event.target_religion_id) {
            auto& pos = agent_view.get<PositionComponent>(agent);
            // Chebyshev distance within 20 tiles of building
            int dist = std::max(std::abs(pos.x - event.x), std::abs(pos.y - event.y));
            if (dist < 20) {
                religiosity.devotion = std::max(0.0f, religiosity.devotion - 20.0f);
                count++;
            }
        }
    }

    m_dispatcher.enqueue<LogEvent>({
        "Raid on " + event.target_religion_id + " at (" + std::to_string(event.x) + "," + std::to_string(event.y) + ")! " + std::to_string(count) + " agents affected.",
        LogSeverity::WARNING,
        "ReligionSystem"
    });

    m_dispatcher.enqueue<MilestoneEvent>({
        "RELIGIOUS_RAID",
        "A faction raid targeted a place of worship for " + event.target_religion_id + ".",
        "", // Faction A (instigator handled via event)
        event.target_religion_id,
        event.target_building,
        3.0f
    });

    // Increase tension globally for this religion
    auto* influence_registry = m_registry.ctx().find<ReligionInfluenceRegistry>();
    if (influence_registry && influence_registry->religions.contains(event.target_religion_id)) {
        influence_registry->religions[event.target_religion_id].suppression_tension += 50.0f;
    }
}

void ReligionSystem::spawnEmergentFaction(const std::string& religion_id) {
    auto* rel_registry = m_registry.ctx().find<ReligionRegistryComponent>();
    if (!rel_registry || !rel_registry->religions.contains(religion_id)) return;
    
    const auto& religion = rel_registry->religions.at(religion_id);
    
    m_dispatcher.enqueue<LogEvent>({
        "EMERGENT FACTION: " + religion.name + " has become a political force!",
        LogSeverity::CRITICAL,
        "ReligionSystem"
    });

    m_dispatcher.enqueue<MilestoneEvent>({
        "POLITICIZATION",
        "The " + religion.name + " has officially organized into a political faction due to sustained suppression.",
        "GOVERNMENT",
        religion_id,
        entt::null,
        5.0f
    });

    // Create a new faction leader entity
    auto leader = m_registry.create();
    m_registry.emplace<NameComponent>(leader, "The High Priest of " + religion.name);
    m_registry.emplace<FactionComponent>(leader, religion_id + "_PARTY", "COLLECTIVE", 100, 1.0f);
    m_registry.emplace<FactionLeaderComponent>(leader, "High Priest", FactionArchetype::ENTROPIC_DRIFT, 2.0f);
    m_registry.emplace<FactionDirectiveComponent>(leader, DirectiveType::UTILITY_BURST, 100, 2.0f);
    auto& stance = m_registry.emplace<FactionReligionStanceComponent>(leader);
    stance.stances[religion_id] = ReligionStance::PATRON;
    stance.stances["SYNTH_GOSPEL"] = ReligionStance::SUPPRESSOR; // Rivalry
}

void ReligionSystem::checkHolyDays() {
    auto* registry_ptr = m_registry.ctx().find<ReligionRegistryComponent>();
    if (!registry_ptr) return;

    for (auto& [id, religion] : registry_ptr->religions) {
        for (auto holy_tick : religion.holy_day_ticks) {
            // Check if we are within the L4 tick window of a holy day
            if (m_current_tick >= holy_tick && m_current_tick < holy_tick + 20) {
                m_dispatcher.enqueue<HolyDayEvent>({
                    id,
                    holy_tick,
                    "A holy day for " + religion.name + " has begun."
                });
                
                m_dispatcher.enqueue<LogEvent>({
                    "Holy day started for " + religion.name,
                    LogSeverity::INFO,
                    "ReligionSystem"
                });
            }
        }
    }
}

void ReligionSystem::handleWorship(const WorshipEvent& event) {
    if (m_registry.all_of<ReligiosityComponent>(event.worshipper)) {
        auto& religiosity = m_registry.get<ReligiosityComponent>(event.worshipper);
        if (religiosity.religion_id == event.religion_id) {
            religiosity.devotion = std::min(100.0f, religiosity.devotion + 5.0f);
            religiosity.last_worship_tick = event.tick;
            
            m_dispatcher.enqueue<LogEvent>({
                "Agent worshipped: " + event.religion_id + ", devotion now " + std::to_string(religiosity.devotion),
                LogSeverity::DEBUG,
                "ReligionSystem"
            });

            // [P.2] Reputation impact for Player
            if (m_registry.all_of<PlayerComponent>(event.worshipper)) {
                // Find factions that patronize this religion
                auto leader_view = m_registry.view<FactionReligionStanceComponent, FactionComponent>();
                for (auto leader_ent : leader_view) {
                    auto& stances = leader_view.get<FactionReligionStanceComponent>(leader_ent);
                    if (stances.stances.contains(event.religion_id)) {
                        auto stance = stances.stances.at(event.religion_id);
                        if (stance == ReligionStance::PATRON) {
                            auto& faction = leader_view.get<FactionComponent>(leader_ent);
                            m_dispatcher.enqueue<AgentFactionReputationEvent>({
                                event.worshipper,
                                faction.faction_id,
                                1.0f // Small boost per worship
                            });
                        }
                    }
                }
            }
        }
    }
}

void ReligionSystem::handleProselytizing(const ProselytizingEvent& event) {
    if (event.success) {
        // [H.6] Assign target to religion with minimal devotion
        std::string religion_name = event.religion_id;
        auto* reg_comp = m_registry.ctx().find<ReligionRegistryComponent>();
        if (reg_comp && reg_comp->religions.contains(event.religion_id)) {
            religion_name = reg_comp->religions.at(event.religion_id).name;
        }

        if (!m_registry.all_of<ReligiosityComponent>(event.target)) {
            m_registry.emplace<ReligiosityComponent>(event.target, event.religion_id, 5.0f, 0ULL, false);
        } else {
            auto& religiosity = m_registry.get<ReligiosityComponent>(event.target);
            if (religiosity.religion_id != event.religion_id) {
                // Potential conversion logic if devotion is low
                if (religiosity.devotion < 15.0f) {
                    religiosity.religion_id = event.religion_id;
                    religiosity.devotion = 5.0f;
                }
            }
        }

        m_dispatcher.enqueue<LogEvent>({
            "Proselytizing SUCCESS: Agent converted to " + religion_name,
            LogSeverity::INFO,
            "ReligionSystem"
        });

        // Small affinity boost on success
        auto* npc_init = m_registry.try_get<NPCComponent>(event.initiator);
        if (npc_init) {
            auto& rel_target = m_registry.get_or_emplace<RelationshipComponent>(event.target);
            auto& record = rel_target.records[npc_init->macro_id];
            record.target_macro_id = npc_init->macro_id;
            record.affinity = std::min(100.0f, record.affinity + 5.0f);
        }

    } else {
        // [H.6] Failed proselytizing lowers the affinity
        auto* npc_init = m_registry.try_get<NPCComponent>(event.initiator);
        if (npc_init) {
            auto& rel_target = m_registry.get_or_emplace<RelationshipComponent>(event.target);
            auto& record = rel_target.records[npc_init->macro_id];
            record.target_macro_id = npc_init->macro_id;
            record.affinity = std::max(-100.0f, record.affinity - 10.0f);
        }

        m_dispatcher.enqueue<LogEvent>({
            "Proselytizing FAILURE: Target rejected " + event.religion_id,
            LogSeverity::INFO,
            "ReligionSystem"
        });
    }
}

void ReligionSystem::handleHolyDay(const HolyDayEvent& event) {
    // [H.4] Holy Day starts a procession
    startProcession(event.religion_id, event.holy_day_id);
}

void ReligionSystem::startProcession(const std::string& religion_id, uint32_t holy_day_id) {
    // Find a starting place of worship
    entt::entity start_building = entt::null;
    auto worship_view = m_registry.view<WorshipPlaceComponent, PositionComponent>();
    for (auto entity : worship_view) {
        if (worship_view.get<WorshipPlaceComponent>(entity).religion_id == religion_id) {
            start_building = entity;
            break;
        }
    }

    if (start_building == entt::null) return;

    // Generate a route
    auto route = generateProcessionRoute(start_building);
    if (route.empty()) return;

    // Create the procession entity
    auto procession_ent = m_registry.create();
    auto& proc = m_registry.emplace<ProcessionComponent>(procession_ent);
    proc.religion_id = religion_id;
    proc.holy_day_id = holy_day_id;
    proc.route = route;

    // Pick a leader (highest devotion agent near the building)
    entt::entity leader = entt::null;
    float max_devotion = -1.0f;
    const auto& b_pos = m_registry.get<PositionComponent>(start_building);
    
    auto agent_view = m_registry.view<ReligiosityComponent, PositionComponent, AgentComponent>();
    for (auto agent : agent_view) {
        auto& religiosity = agent_view.get<ReligiosityComponent>(agent);
        if (religiosity.religion_id == religion_id) {
            auto& a_pos = agent_view.get<PositionComponent>(agent);
            if (a_pos.layer_id == b_pos.layer_id) {
                int dist = std::abs(a_pos.x - b_pos.x) + std::abs(a_pos.y - b_pos.y);
                if (dist < 30) {
                    if (religiosity.devotion > max_devotion) {
                        max_devotion = religiosity.devotion;
                        leader = agent;
                    }
                }
            }
        }
    }

    if (leader != entt::null) {
        proc.leader = leader;
        m_registry.emplace<InProcessionComponent>(leader, religion_id, procession_ent, true);
        
        // Give the leader a Patrol task to follow the route
        auto& patrol = m_registry.get_or_emplace<PatrolComponent>(leader);
        patrol.waypoints = route;
        patrol.current_waypoint_index = 0;
        
        auto& task = m_registry.get_or_emplace<AgentTaskComponent>(leader);
        task.task_type = AgentTaskType::PATROL;
        
        m_dispatcher.enqueue<SpeechEvent>({leader, "The time has come! Follow the light!", 40, AudibilityLevel::CLEAR});
        
        // Recruit followers (nearby high devotion agents)
        for (auto agent : agent_view) {
            if (agent == leader) continue;
            auto& religiosity = agent_view.get<ReligiosityComponent>(agent);
            if (religiosity.religion_id == religion_id && religiosity.devotion > 40.0f) {
                auto& a_pos = agent_view.get<PositionComponent>(agent);
                if (a_pos.layer_id == b_pos.layer_id) {
                    int dist = std::abs(a_pos.x - b_pos.x) + std::abs(a_pos.y - b_pos.y);
                    if (dist < 15) {
                        m_registry.emplace<InProcessionComponent>(agent, religion_id, procession_ent, false);
                        proc.followers.push_back(agent);
                        
                        // Tell follower to follow leader
                        auto& follow = m_registry.get_or_emplace<FollowComponent>(agent);
                        follow.target = leader;
                        follow.target_distance = 1 + (proc.followers.size() / 3);

                        auto& task = m_registry.get_or_emplace<AgentTaskComponent>(agent);
                        task.task_type = AgentTaskType::FOLLOW_LEADER;
                    }
                }
            }
        }
    } else {
        // No leader found, destroy the procession entity
        m_registry.destroy(procession_ent);
    }
}

std::vector<PositionComponent> ReligionSystem::generateProcessionRoute(entt::entity start_building) {
    std::vector<PositionComponent> route;
    if (!m_registry.all_of<PositionComponent>(start_building)) return route;

    const auto& b_pos = m_registry.get<PositionComponent>(start_building);
    route.push_back(b_pos);

    // Simple route: 3 random points within 20 tiles
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> d(-15, 15);

    for (int i = 0; i < 3; ++i) {
        route.push_back({
            std::clamp(b_pos.x + d(gen), 0, 200), // Assuming world size 200
            std::clamp(b_pos.y + d(gen), 0, 200),
            b_pos.layer_id
        });
    }
    
    // Return to start
    route.push_back(b_pos);
    return route;
}

void ReligionSystem::updateProcessions() {
    auto proc_view = m_registry.view<ProcessionComponent>();
    std::vector<entt::entity> to_destroy;

    for (auto proc_ent : proc_view) {
        auto& proc = proc_view.get<ProcessionComponent>(proc_ent);
        
        // If leader is gone or not in procession anymore
        if (!m_registry.valid(proc.leader) || !m_registry.all_of<InProcessionComponent>(proc.leader)) {
            proc.is_active = false;
        } else {
            // Check if leader finished the patrol
            auto& patrol = m_registry.get<PatrolComponent>(proc.leader);
            if (patrol.waypoints.empty()) {
                proc.is_active = false;
            }
        }

        if (!proc.is_active) {
            // Clean up followers
            for (auto follower : proc.followers) {
                if (m_registry.valid(follower)) {
                    m_registry.remove<InProcessionComponent>(follower);
                    m_registry.remove<FollowComponent>(follower);
                    m_registry.remove<AgentTaskComponent>(follower);
                }
            }
            if (m_registry.valid(proc.leader)) {
                m_registry.remove<InProcessionComponent>(proc.leader);
                m_registry.remove<PatrolComponent>(proc.leader);
                m_registry.remove<AgentTaskComponent>(proc.leader);
            }
            to_destroy.push_back(proc_ent);
        }
    }

    for (auto ent : to_destroy) {
        m_registry.destroy(ent);
    }
}

void ReligionSystem::handleProcessionTension() {
    auto proc_view = m_registry.view<ProcessionComponent>();
    
    // Only check if we have multiple processions
    if (proc_view.size() < 2) return;

    for (auto it1 = proc_view.begin(); it1 != proc_view.end(); ++it1) {
        auto& proc1 = proc_view.get<ProcessionComponent>(*it1);
        if (!m_registry.valid(proc1.leader)) continue;
        const auto& pos1 = m_registry.get<PositionComponent>(proc1.leader);

        for (auto it2 = std::next(it1); it2 != proc_view.end(); ++it2) {
            auto& proc2 = proc_view.get<ProcessionComponent>(*it2);
            if (!m_registry.valid(proc2.leader)) continue;
            
            if (proc1.religion_id == proc2.religion_id) continue;

            const auto& pos2 = m_registry.get<PositionComponent>(proc2.leader);
            
            if (pos1.layer_id == pos2.layer_id) {
                int dist = std::abs(pos1.x - pos2.x) + std::abs(pos1.y - pos2.y);
                if (dist < 5) {
                    // Tension!
                    m_dispatcher.enqueue<ReligiousTensionEvent>({
                        proc1.religion_id,
                        proc2.religion_id,
                        entt::null, // location building if any
                        10.0f
                    });

                    m_dispatcher.enqueue<SpeechEvent>({proc1.leader, "Blasphemers! Clear the path!", 20, AudibilityLevel::CLEAR});
                    m_dispatcher.enqueue<SpeechEvent>({proc2.leader, "Your gods are false! Repent!", 20, AudibilityLevel::CLEAR});
                    
                    m_dispatcher.enqueue<LogEvent>({
                        "Religious tension between " + proc1.religion_id + " and " + proc2.religion_id,
                        LogSeverity::WARNING,
                        "ReligionSystem"
                    });
                }
            }
        }
    }
}

void ReligionSystem::diffuseInfluence() {
    auto chunk_view = m_registry.view<ChunkComponent, ReligionInfluenceFieldComponent>();
    
    struct InfluenceDelta {
        entt::entity chunk;
        std::string religion;
        float amount;
    };
    std::vector<InfluenceDelta> deltas;

    for (auto ent : chunk_view) {
        auto& chunk = chunk_view.get<ChunkComponent>(ent);
        auto& inf = chunk_view.get<ReligionInfluenceFieldComponent>(ent);

        for (auto& [rel, amount] : inf.influence) {
            if (amount < 1.0f) continue;
            
            float spread = amount * 0.1f; // Spread 10% to neighbors
            for (auto target_ent : chunk_view) {
                if (ent == target_ent) continue;
                auto& target_chunk = chunk_view.get<ChunkComponent>(target_ent);
                int dx = std::abs(chunk.chunk_x - target_chunk.chunk_x);
                int dy = std::abs(chunk.chunk_y - target_chunk.chunk_y);
                if (dx <= 1 && dy <= 1) {
                    deltas.push_back({target_ent, rel, spread});
                }
            }
        }
    }

    for (auto& delta : deltas) {
        m_registry.get<ReligionInfluenceFieldComponent>(delta.chunk).influence[delta.religion] += delta.amount;
    }
}

} // namespace NeonOubliette
