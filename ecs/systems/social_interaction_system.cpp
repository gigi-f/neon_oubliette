#include "social_interaction_system.h"
#include <iostream>
#include <cmath>
#include <unordered_map>
#include <random>

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

void SocialInteractionSystem::update(double delta_time) {
    (void)delta_time;
    
    // 1. Tick down yielding states and socialization need
    auto yield_view = m_registry.view<SocialHierarchyComponent, NeedsComponent>();
    for (auto entity : yield_view) {
        auto& hierarchy = yield_view.get<SocialHierarchyComponent>(entity);
        if (hierarchy.yield_ticks_remaining > 0) {
            hierarchy.yield_ticks_remaining--;
            if (hierarchy.yield_ticks_remaining == 0) {
                hierarchy.currently_yielding_to = entt::null;
            }
        }

        // Decay socialization need
        auto& needs = yield_view.get<NeedsComponent>(entity);
        needs.socialization = std::max(0.0f, needs.socialization - 0.05f);
    }

    // 2. Build spatial grid for O(1) proximity lookup
    constexpr int CELL_SIZE = 3;
    std::unordered_map<GridKey, std::vector<entt::entity>, GridKeyHash> grid;

    // Add agents to grid
    auto agent_view = m_registry.view<PositionComponent, SocialHierarchyComponent, AgentComponent, NPCComponent>();
    for (auto agent : agent_view) {
        const auto& pos = agent_view.get<PositionComponent>(agent);
        GridKey key{pos.x / CELL_SIZE, pos.y / CELL_SIZE, pos.layer_id};
        grid[key].push_back(agent);
    }

    // Add player to grid for forming relationships with NPCs
    auto player_view = m_registry.view<PositionComponent, PlayerComponent>();
    for (auto player : player_view) {
        const auto& pos = player_view.get<PositionComponent>(player);
        GridKey key{pos.x / CELL_SIZE, pos.y / CELL_SIZE, pos.layer_id};
        grid[key].push_back(player);
    }

    auto get_macro_id = [&](entt::entity e) -> uint64_t {
        if (auto* npc = m_registry.try_get<NPCComponent>(e)) return npc->macro_id;
        if (auto* player = m_registry.try_get<PlayerComponent>(e)) return player->macro_id;
        return 0;
    };

    // 3. Process social interactions
    for (auto& [key, agents] : grid) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                GridKey neighbor_key{key.cx + dx, key.cy + dy, key.layer};
                auto it = grid.find(neighbor_key);
                if (it == grid.end()) continue;

                for (auto agent_a : agents) {
                    const auto& pos_a = m_registry.get<PositionComponent>(agent_a);
                    uint64_t id_a = get_macro_id(agent_a);
                    if (id_a == 0) continue;

                    for (auto agent_b : it->second) {
                        if (agent_a >= agent_b) continue; // avoid duplicate & self pairs

                        const auto& pos_b = m_registry.get<PositionComponent>(agent_b);
                        uint64_t id_b = get_macro_id(agent_b);
                        if (id_b == 0) continue;

                        int dist = std::abs(pos_a.x - pos_b.x) + std::abs(pos_a.y - pos_b.y);
                        if (dist <= 2) {
                            // Yielding logic (if both have social hierarchy)
                            auto* h_a = m_registry.try_get<SocialHierarchyComponent>(agent_a);
                            auto* h_b = m_registry.try_get<SocialHierarchyComponent>(agent_b);
                            if (h_a && h_b) {
                                float delta = h_a->status - h_b->status;
                                if (delta > STATUS_DELTA_THRESHOLD) {
                                    handle_social_yielding(agent_b, agent_a);
                                } else if (delta < -STATUS_DELTA_THRESHOLD) {
                                    handle_social_yielding(agent_a, agent_b);
                                }
                            }

                            // Friendship & Relationship logic (G.1, G.3)
                            update_relationship(agent_a, id_a, agent_b, id_b);
                        }
                    }
                }
            }
        }
    }

    // 4. Coworker logic (G.4) - done every few turns
    static int coworker_check_timer = 0;
    if (++coworker_check_timer >= 10) {
        coworker_check_timer = 0;
        process_coworkers();
    }

    // 5. Gift-giving logic (G.5)
    static int gift_check_timer = 0;
    if (++gift_check_timer >= 50) {
        gift_check_timer = 0;
        process_gifts();
    }
}

void SocialInteractionSystem::update_relationship(entt::entity a, uint64_t id_a, entt::entity b, uint64_t id_b) {
    auto& rel_a = m_registry.get_or_emplace<RelationshipComponent>(a);
    auto& rel_b = m_registry.get_or_emplace<RelationshipComponent>(b);

    auto& rec_a = rel_a.records[id_b];
    auto& rec_b = rel_b.records[id_a];

    // Initialize if new
    if (rec_a.target_macro_id == 0) {
        rec_a.target_macro_id = id_b;
        rec_a.tier = RelationshipTier::ACQUAINTANCE;
        rec_a.affinity = 0.0f;
    }
    if (rec_b.target_macro_id == 0) {
        rec_b.target_macro_id = id_a;
        rec_b.tier = RelationshipTier::ACQUAINTANCE;
        rec_b.affinity = 0.0f;
    }

    // Increase affinity for proximity/positive interaction
    rec_a.affinity = std::min(100.0f, rec_a.affinity + 0.1f);
    rec_b.affinity = std::min(100.0f, rec_b.affinity + 0.1f);
    
    rec_a.last_interaction_tick = 0; // TurnManager should ideally provide this, but 0 is placeholder
    rec_b.last_interaction_tick = 0;

    // Socialization need fulfilled
    if (auto* needs_a = m_registry.try_get<NeedsComponent>(a)) needs_a->socialization = std::min(100.0f, needs_a->socialization + 0.5f);
    if (auto* needs_b = m_registry.try_get<NeedsComponent>(b)) needs_b->socialization = std::min(100.0f, needs_b->socialization + 0.5f);

    // [F.2] The Rumor Mill: Swapping information
    if (rand() % 10 == 0) {
        swap_rumors(a, b);
    }

    // [J.2] Birth System: Reproduction check
    if (rec_a.tier >= RelationshipTier::FAMILY && rec_a.affinity > 80.0f) {
        if (rand() % 500 == 0) { // low probability per social tick
            handle_reproduction(a, b);
        }
    }

    // Tier up: Acquaintance -> Friend
    if (rec_a.tier == RelationshipTier::ACQUAINTANCE && rec_a.affinity > 50.0f) {
        rec_a.tier = RelationshipTier::FRIEND;
        rec_b.tier = RelationshipTier::FRIEND;
    }
}

void SocialInteractionSystem::process_coworkers() {
    auto work_view = m_registry.view<WorkplaceComponent, NPCComponent>();
    std::unordered_map<entt::entity, std::vector<entt::entity>> workplace_groups;

    for (auto entity : work_view) {
        auto& work = work_view.get<WorkplaceComponent>(entity);
        if (m_registry.valid(work.building_entity)) {
            workplace_groups[work.building_entity].push_back(entity);
        }
    }

    for (auto& [building, workers] : workplace_groups) {
        for (size_t i = 0; i < workers.size(); ++i) {
            for (size_t j = i + 1; j < workers.size(); ++j) {
                entt::entity a = workers[i];
                entt::entity b = workers[j];
                uint64_t id_a = work_view.get<NPCComponent>(a).macro_id;
                uint64_t id_b = work_view.get<NPCComponent>(b).macro_id;

                auto& rel_a = m_registry.get_or_emplace<RelationshipComponent>(a);
                auto& rel_b = m_registry.get_or_emplace<RelationshipComponent>(b);

                auto& rec_a = rel_a.records[id_b];
                auto& rec_b = rel_b.records[id_a];

                if (rec_a.target_macro_id == 0) {
                    rec_a.target_macro_id = id_b;
                    rec_a.tier = RelationshipTier::COWORKER;
                    rec_a.affinity = 10.0f;
                }
                if (rec_b.target_macro_id == 0) {
                    rec_b.target_macro_id = id_a;
                    rec_b.tier = RelationshipTier::COWORKER;
                    rec_b.affinity = 10.0f;
                }
                
                // Slowly increase affinity for sharing a workplace
                rec_a.affinity = std::min(100.0f, rec_a.affinity + 0.05f);
                rec_b.affinity = std::min(100.0f, rec_b.affinity + 0.05f);
            }
        }
    }
}

void SocialInteractionSystem::process_gifts() {
    auto view = m_registry.view<RelationshipComponent, InventoryComponent, PositionComponent>();
    for (auto entity : view) {
        auto& rel = view.get<RelationshipComponent>(entity);
        auto& inv = view.get<InventoryComponent>(entity);
        const auto& pos = view.get<PositionComponent>(entity);

        if (inv.contained_items.empty()) continue;

        for (auto& [id, record] : rel.records) {
            // Gift-giving only for close friends/family and occasionally
            if (record.tier >= RelationshipTier::FRIEND && record.affinity > 70.0f) {
                if (rand() % 100 == 0) { // Check periodically, so 1% chance every 50 ticks
                    // Find the partner if they are nearby
                    auto partner_view = m_registry.view<NPCComponent, PositionComponent, InventoryComponent>();
                    for (auto partner : partner_view) {
                        const auto& p_npc = partner_view.get<NPCComponent>(partner);
                        if (p_npc.macro_id == id) {
                            const auto& p_pos = partner_view.get<PositionComponent>(partner);
                            int dist = std::abs(pos.x - p_pos.x) + std::abs(pos.y - p_pos.y);
                            if (dist <= 1 && pos.layer_id == p_pos.layer_id) {
                                // Transfer an item
                                auto item = inv.contained_items.back();
                                inv.contained_items.pop_back();
                                partner_view.get<InventoryComponent>(partner).contained_items.push_back(item);
                                
                                // Increase affinity further
                                record.affinity = std::min(100.0f, record.affinity + 5.0f);
                                
                                // Event or Speech
                                m_dispatcher.trigger(SpeechEvent{entity, "Here, take this. You look like you need it."});
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
}

void SocialInteractionSystem::handle_agent_death(const AgentDeathEvent& event) {
    auto view = m_registry.view<RelationshipComponent, NeedsComponent>();
    for (auto entity : view) {
        auto& rel = view.get<RelationshipComponent>(entity);
        auto& needs = view.get<NeedsComponent>(entity);

        auto it = rel.records.find(event.macro_id);
        if (it != rel.records.end()) {
            // Grief: Frustration increases, socialization drops
            if (it->second.tier == RelationshipTier::FAMILY) {
                needs.frustration += 30.0f;
                needs.socialization -= 40.0f;
                m_dispatcher.trigger(SpeechEvent{entity, "I just heard... they're gone. I can't believe it."});
            } else if (it->second.tier == RelationshipTier::FRIEND) {
                needs.frustration += 10.0f;
                needs.socialization -= 20.0f;
            }
            
            // Tier change to reflect mourning? Maybe later.
        }
    }
}

void SocialInteractionSystem::handle_social_yielding(entt::entity yielder, entt::entity master) {
    auto& hierarchy = m_registry.get<SocialHierarchyComponent>(yielder);
    
    // Only update if not already yielding or yielding to someone lower status
    if (hierarchy.currently_yielding_to == master) {
        hierarchy.yield_ticks_remaining = YIELD_DURATION_TICKS;
        return;
    }
    
    hierarchy.currently_yielding_to = master;
    hierarchy.yield_ticks_remaining = YIELD_DURATION_TICKS;

    // Apply mechanical effect: Frustration increase for autonomous agents
    if (hierarchy.is_autonomous) {
        if (auto* needs = m_registry.try_get<NeedsComponent>(yielder)) {
            needs->frustration += 0.5f;
        }
    }
}

void SocialInteractionSystem::swap_rumors(entt::entity a, entt::entity b) {
    auto* info_a = m_registry.try_get<InformationComponent>(a);
    auto* info_b = m_registry.try_get<InformationComponent>(b);

    if (!info_a && !info_b) return;

    // A -> B
    if (info_a && !info_a->records.empty()) {
        auto& rumor = info_a->records[rand() % info_a->records.size()];
        auto& target_info = m_registry.get_or_emplace<InformationComponent>(b);
        bool duplicate = false;
        for (const auto& r : target_info.records) {
            if (r.content_tag == rumor.content_tag) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            auto new_rumor = rumor;
            new_rumor.hops++;
            new_rumor.veracity *= 0.95f;
            target_info.records.push_back(new_rumor);
            m_dispatcher.trigger(InformationPropagationEvent{a, b, new_rumor});
        }
    }

    // B -> A
    if (info_b && !info_b->records.empty()) {
        auto& rumor = info_b->records[rand() % info_b->records.size()];
        auto& target_info = m_registry.get_or_emplace<InformationComponent>(a);
        bool duplicate = false;
        for (const auto& r : target_info.records) {
            if (r.content_tag == rumor.content_tag) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            auto new_rumor = rumor;
            new_rumor.hops++;
            new_rumor.veracity *= 0.95f;
            target_info.records.push_back(new_rumor);
            m_dispatcher.trigger(InformationPropagationEvent{b, a, new_rumor});
        }
    }
}


void SocialInteractionSystem::handle_reproduction(entt::entity a, entt::entity b) {
    // Check if either is already pregnant
    if (m_registry.all_of<ReproductionComponent>(a) && m_registry.get<ReproductionComponent>(a).is_pregnant) return;
    if (m_registry.all_of<ReproductionComponent>(b) && m_registry.get<ReproductionComponent>(b).is_pregnant) return;

    auto* age_a = m_registry.try_get<AgeComponent>(a);
    auto* age_b = m_registry.try_get<AgeComponent>(b);

    if (!age_a || !age_b) return;

    // Species compatibility check
    auto* bio_a = m_registry.try_get<Layer1BiologyComponent>(a);
    auto* bio_b = m_registry.try_get<Layer1BiologyComponent>(b);
    if (!bio_a || !bio_b || bio_a->species != bio_b->species) return;

    // Age suitability (Adults only)
    if (age_a->stage != LifeStage::ADULT && age_a->stage != LifeStage::YOUNG_ADULT) return;
    if (age_b->stage != LifeStage::ADULT && age_b->stage != LifeStage::YOUNG_ADULT) return;

    // Pick one as the "mother"
    entt::entity mother = (rand() % 2 == 0) ? a : b;
    entt::entity partner = (mother == a) ? b : a;

    auto& repro = m_registry.get_or_emplace<ReproductionComponent>(mother);
    repro.is_pregnant = true;
    repro.partner = partner;
    
    // Gestation: approx 10,000 to 20,000 ticks for a human (scaled down for gameplay)
    repro.gestation_ticks_remaining = 5000 + (rand() % 5000);
    repro.genetic_health = 1.0f - (age_a->biological_wear + age_b->biological_wear) * 0.2f;

    m_dispatcher.enqueue<LogEvent>({"Birth process initiated between agents.", LogSeverity::INFO, "Social"});
}

} // namespace NeonOubliette
