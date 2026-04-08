#include "conversation_system.h"
#include "grammar_engine.h"
#include "dialogue_atoms.h"
#include <iostream>
#include <cmath>
#include <unordered_map>
#include <random>
#include "../event_declarations.h"

namespace NeonOubliette {

namespace {
    struct GridKey {
        int cx, cy, layer;
        bool operator==(const GridKey& o) const { return cx == o.cx && cy == o.cy && layer == o.layer; }
    };
    struct GridKeyHash {
        size_t operator()(const GridKey& k) const {
            size_t h = std::hash<int>()(k.cx);
            h ^= std::hash<int>()(k.cy) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(k.layer) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
}

ConversationSystem::ConversationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {}

void ConversationSystem::initialize() {}

void ConversationSystem::update(double delta_time) {
    (void)delta_time;
    
    process_ongoing_conversations();
    find_new_conversations();
}

void ConversationSystem::process_ongoing_conversations() {
    auto view = m_registry.view<ConversationComponent, NPCComponent>();
    
    std::vector<entt::entity> finished;

    for (auto entity : view) {
        auto& conv = view.get<ConversationComponent>(entity);
        conv.step_counter++;

        if (conv.step_counter >= conv.duration_ticks) {
            finished.push_back(entity);
        } else {
            // Periodic speech events during conversation [F.4]
            if (conv.step_counter % 2 == 1) {
                // Use GrammarEngine for real systemic dialogue [F.1]
                std::string utterance = Systems::GrammarEngine::instance().expand(conv.topic_atom_tag, m_registry, entity);
                
                SpeechEvent event{entity, utterance, 2, AudibilityLevel::CLEAR};

                // [F.6] Attach record if topic is rumors
                if (conv.topic_atom_tag == "TOPIC_RUMORS") {
                    if (auto* info = m_registry.try_get<InformationComponent>(entity)) {
                        if (!info->records.empty()) {
                            event.has_record = true;
                            event.record = info->records[rand() % info->records.size()];
                        }
                    }
                }

                m_dispatcher.trigger(event);
            }
        }
    }

    for (auto entity : finished) {
        // Partner processing may have already removed this entity's component
        if (!m_registry.all_of<ConversationComponent>(entity)) continue;

        auto& conv = m_registry.get<ConversationComponent>(entity);
        entt::entity partner = conv.partner;

        // Adjust affinity at the end of conversation
        if (m_registry.valid(partner)) {
            uint64_t id_a = m_registry.get<NPCComponent>(entity).macro_id;
            uint64_t id_b = m_registry.get<NPCComponent>(partner).macro_id;

            auto& rel_a = m_registry.get_or_emplace<RelationshipComponent>(entity);
            auto& rel_b = m_registry.get_or_emplace<RelationshipComponent>(partner);

            auto& rec_a = rel_a.records[id_b];
            auto& rec_b = rel_b.records[id_a];

            // Increase affinity (Socialization benefit)
            float affinity_gain = 2.0f;
            if (conv.topic_atom_tag == "GENERAL_CHITCHAT") affinity_gain = 1.0f;
            
            rec_a.affinity = std::min(100.0f, rec_a.affinity + affinity_gain);
            rec_b.affinity = std::min(100.0f, rec_b.affinity + affinity_gain);

            // [H.6] Proselytizing Outcome
            if (conv.topic_atom_tag == "TOPIC_PROSELYTIZING" && conv.is_initiator) {
                auto* relig_a = m_registry.try_get<ReligiosityComponent>(entity);
                auto* relig_b = m_registry.try_get<ReligiosityComponent>(partner);
                
                if (relig_a) {
                    // Check if target has a rival religion (different id and high devotion)
                    bool has_rival = (relig_b && relig_b->religion_id != relig_a->religion_id && relig_b->devotion > 15.0f);
                    
                    // Partner (target)'s affinity toward speaker (entity)
                    uint64_t speaker_macro_id = m_registry.get<NPCComponent>(entity).macro_id;
                    auto& rel_partner = m_registry.get_or_emplace<RelationshipComponent>(partner);
                    auto& rec_partner = rel_partner.records[speaker_macro_id];
                    
                    bool success = (rec_partner.affinity > 40.0f && !has_rival);
                    
                    m_dispatcher.enqueue<ProselytizingEvent>({entity, partner, relig_a->religion_id, success});
                }
            }

            // Clean up partner
            if (m_registry.all_of<ConversationComponent>(partner)) {
                m_registry.remove<ConversationComponent>(partner);
            }
        }
        
        m_registry.remove<ConversationComponent>(entity);
    }
}

void ConversationSystem::find_new_conversations() {
    // 1. Build spatial grid for O(1) proximity lookup
    constexpr int CELL_SIZE = 3;
    std::unordered_map<GridKey, std::vector<entt::entity>, GridKeyHash> grid;

    auto agent_view = m_registry.view<PositionComponent, AgentComponent, NPCComponent>();
    
    // Filter agents: must NOT already be in a conversation
    for (auto agent : agent_view) {
        if (m_registry.all_of<ConversationComponent>(agent)) continue;
        
        // Filter by state: LEISURE or IDLE task
        bool is_leisure = false;
        if (auto* schedule = m_registry.try_get<ScheduleComponent>(agent)) {
            // We don't have easy access to TimeOfDay here without querying weather, 
            // but we can check if they are "idling" or "wandering" as a proxy if leisure.
        }
        
        auto* task = m_registry.try_get<AgentTaskComponent>(agent);
        bool is_idle = !task || task->task_type == AgentTaskType::IDLE || task->task_type == AgentTaskType::WANDER;
        
        if (is_idle) {
            const auto& pos = agent_view.get<PositionComponent>(agent);
            GridKey key{pos.x / CELL_SIZE, pos.y / CELL_SIZE, pos.layer_id};
            grid[key].push_back(agent);
        }
    }

    // 2. Process pairs
    static std::mt19937 gen(1337);
    std::uniform_int_distribution<> dur_dist(CONVERSATION_MIN_DURATION, CONVERSATION_MAX_DURATION);

    for (auto& [key, agents] : grid) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                GridKey neighbor_key{key.cx + dx, key.cy + dy, key.layer};
                auto it = grid.find(neighbor_key);
                if (it == grid.end()) continue;

                for (auto agent_a : agents) {
                    if (!m_registry.valid(agent_a) || m_registry.all_of<ConversationComponent>(agent_a)) continue;
                    
                    const auto& pos_a = m_registry.get<PositionComponent>(agent_a);

                    for (auto agent_b : it->second) {
                        if (agent_a == agent_b) continue;
                        if (!m_registry.valid(agent_b) || m_registry.all_of<ConversationComponent>(agent_b)) continue;

                        const auto& pos_b = m_registry.get<PositionComponent>(agent_b);
                        
                        int dist = std::abs(pos_a.x - pos_b.x) + std::abs(pos_a.y - pos_b.y);
                        if (dist <= CONVERSATION_MAX_DIST) {
                            // Start conversation
                            std::string topic = select_shared_topic(agent_a, agent_b);
                            uint32_t duration = dur_dist(gen);

                            m_registry.emplace<ConversationComponent>(agent_a, agent_b, topic, 0u, duration, true);
                            m_registry.emplace<ConversationComponent>(agent_b, agent_a, topic, 0u, duration, false);
                            
                            // Agents face each other [F.4]
                            if (auto* orient_a = m_registry.try_get<OrientationComponent>(agent_a)) {
                                if (pos_b.x > pos_a.x) orient_a->facing = Direction::EAST;
                                else if (pos_b.x < pos_a.x) orient_a->facing = Direction::WEST;
                                else if (pos_b.y > pos_a.y) orient_a->facing = Direction::SOUTH;
                                else if (pos_b.y < pos_a.y) orient_a->facing = Direction::NORTH;
                            }
                            if (auto* orient_b = m_registry.try_get<OrientationComponent>(agent_b)) {
                                if (pos_a.x > pos_b.x) orient_b->facing = Direction::EAST;
                                else if (pos_a.x < pos_b.x) orient_b->facing = Direction::WEST;
                                else if (pos_a.y > pos_b.y) orient_b->facing = Direction::SOUTH;
                                else if (pos_a.y < pos_b.y) orient_b->facing = Direction::NORTH;
                            }

                            // Visual/Audio cue [F.4]
                            std::string greeting = Systems::GrammarEngine::instance().expand("greeting", m_registry, agent_a);
                            m_dispatcher.trigger(SpeechEvent{agent_a, greeting, 2, AudibilityLevel::CLEAR});
                            
                            // Break out to next agent_a (it's now in a conversation)
                            goto next_agent_a;
                        }
                    }
                    next_agent_a:;
                }
            }
        }
    }
}

std::string ConversationSystem::select_shared_topic(entt::entity a, entt::entity b) {
    auto& library = Systems::DialogueAtomLibrary::instance();
    auto atoms_a = library.query_active_atoms(m_registry, a);
    auto atoms_b = library.query_active_atoms(m_registry, b);

    std::vector<const Systems::DialogueAtom*> shared;
    for (auto atom_a : atoms_a) {
        for (auto atom_b : atoms_b) {
            if (atom_a->tag == atom_b->tag) {
                shared.push_back(atom_a);
                break;
            }
        }
    }

    if (!shared.empty()) {
        auto best = shared[0];
        for (auto s : shared) {
            if (s->weight > best->weight) best = s;
        }
        return best->tag;
    }

    if (!atoms_a.empty() || !atoms_b.empty()) {
        const Systems::DialogueAtom* best = nullptr;
        for (auto s : atoms_a) if (!best || s->weight > best->weight) best = s;
        for (auto s : atoms_b) if (!best || s->weight > best->weight) best = s;
        if (best) return best->tag;
    }

    return "GENERAL_CHITCHAT";
}

} // namespace NeonOubliette
