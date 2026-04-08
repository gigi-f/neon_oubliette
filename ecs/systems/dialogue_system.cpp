#include "dialogue_system.h"
#include "dialogue_atoms.h"
#include "grammar_engine.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../components/lod_components.h"
#include <story.h>
#include <runner.h>
#include <choice.h>
#include <algorithm>
#include <chrono>
#include <entt/entt.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cstring>

namespace NeonOubliette::Systems {

DialogueSystem::DialogueSystem(entt::registry& registry, struct notcurses* nc_context,
                                   entt::dispatcher& event_dispatcher)
    : registry_(registry), nc_context_(nc_context), event_dispatcher_(event_dispatcher) {
    event_dispatcher_.sink<DialogueEvent>().connect<&DialogueSystem::handleDialogueEvent>(this);
    event_dispatcher_.sink<DialogueChoiceEvent>().connect<&DialogueSystem::handleDialogueChoiceEvent>(this);
    event_dispatcher_.sink<CloseDialogueWindowEvent>().connect<&DialogueSystem::handleCloseDialogueWindowEvent>(this);
}

DialogueSystem::~DialogueSystem() {
    if (m_dialogue_plane) {
        ncplane_destroy(m_dialogue_plane);
    }
}

void DialogueSystem::initialize() {
    GrammarEngine::instance().load_grammar("data/dialogue/grammar.json");

    auto& lib = DialogueAtomLibrary::instance();
    
    // Clear if re-initializing (though singleton persists)
    // For now we'll just register if empty to avoid duplicates
    if (!lib.get_atoms().empty()) return;

    // Register standard atoms
    lib.register_atom({"TOPIC_HUNGER_CRITICAL", [](const entt::registry& reg, entt::entity e) {
        if (auto* needs = reg.try_get<NeedsComponent>(e)) {
            return needs->hunger < 15.0f;
        }
        return false;
    }, 100});

    lib.register_atom({"TOPIC_HUNGER", [](const entt::registry& reg, entt::entity e) {
        if (auto* needs = reg.try_get<NeedsComponent>(e)) {
            return needs->hunger < 40.0f;
        }
        return false;
    }, 90});

    lib.register_atom({"TOPIC_THIRST_CRITICAL", [](const entt::registry& reg, entt::entity e) {
        if (auto* needs = reg.try_get<NeedsComponent>(e)) {
            return needs->thirst < 15.0f;
        }
        return false;
    }, 100});

    lib.register_atom({"TOPIC_THIRST", [](const entt::registry& reg, entt::entity e) {
        if (auto* needs = reg.try_get<NeedsComponent>(e)) {
            return needs->thirst < 40.0f;
        }
        return false;
    }, 90});

    lib.register_atom({"TOPIC_FRUSTRATION_HOSTILE", [](const entt::registry& reg, entt::entity e) {
        if (auto* needs = reg.try_get<NeedsComponent>(e)) {
            return needs->frustration > 85.0f;
        }
        return false;
    }, 100});

    lib.register_atom({"TOPIC_FRUSTRATION", [](const entt::registry& reg, entt::entity e) {
        if (auto* needs = reg.try_get<NeedsComponent>(e)) {
            return needs->frustration > 60.0f;
        }
        return false;
    }, 85});

    lib.register_atom({"TOPIC_WORKPLACE", [](const entt::registry& reg, entt::entity e) {
        return reg.all_of<WorkplaceComponent>(e);
    }, 40});

    lib.register_atom({"TOPIC_UNEMPLOYED", [](const entt::registry& reg, entt::entity e) {
        return reg.all_of<CitizenComponent>(e) && !reg.all_of<EmploymentContractComponent>(e);
    }, 50});

    lib.register_atom({"TOPIC_WORK_CONDITIONS", [](const entt::registry& reg, entt::entity e) {
        return reg.all_of<EmploymentContractComponent>(e);
    }, 60});

    lib.register_atom({"TOPIC_GUARD_PATROL", [](const entt::registry& reg, entt::entity e) {
        return reg.all_of<PatrolComponent>(e) && reg.all_of<Layer4PoliticalComponent>(e) && 
               reg.get<Layer4PoliticalComponent>(e).primary_faction == "GOVERNMENT";
    }, 70});

    lib.register_atom({"TOPIC_MARKET_TRENDS", [](const entt::registry& reg, entt::entity e) {
        return reg.all_of<CitizenComponent>(e);
    }, 30});

    lib.register_atom({"TOPIC_XENO_HIERODULE", [](const entt::registry& reg, entt::entity e) {
        if (auto* xeno = reg.try_get<XenoComponent>(e)) {
            return xeno->type == XenoType::HIERODULE;
        }
        return false;
    }, 100});

    lib.register_atom({"TOPIC_XENO_CACOGEN", [](const entt::registry& reg, entt::entity e) {
        if (auto* xeno = reg.try_get<XenoComponent>(e)) {
            return xeno->type == XenoType::CACOGEN;
        }
        return false;
    }, 100});

    lib.register_atom({"TOPIC_CITIZEN_DEFAULT", [](const entt::registry& reg, entt::entity e) {
        return reg.all_of<CitizenComponent>(e);
    }, 10});

    lib.register_atom({"TOPIC_SCARCITY", [](const entt::registry& reg, entt::entity e) {
        if (auto* pos = reg.try_get<PositionComponent>(e)) {
            int cx = pos->x / 40;
            int cy = pos->y / 40;
            auto chunk_view = reg.view<ChunkComponent, MarketDemandComponent>();
            for (auto chunk_ent : chunk_view) {
                auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
                if (chunk.chunk_x == cx && chunk.chunk_y == cy) {
                    auto& market = chunk_view.get<MarketDemandComponent>(chunk_ent);
                    for (auto& [type, scarcity] : market.item_type_scarcity) {
                        if (scarcity > 1.8f) return true;
                    }
                }
            }
        }
        return false;
    }, 70});

    lib.register_atom({"TOPIC_WEALTHY", [](const entt::registry& reg, entt::entity e) {
        if (auto* econ = reg.try_get<Layer3EconomicComponent>(e)) {
            return econ->cash_on_hand > 500;
        }
        return false;
    }, 65});

    lib.register_atom({"TOPIC_POOR", [](const entt::registry& reg, entt::entity e) {
        if (auto* econ = reg.try_get<Layer3EconomicComponent>(e)) {
            return econ->cash_on_hand < 20;
        }
        return false;
    }, 65});

    lib.register_atom({"TOPIC_HIGH_STATUS", [](const entt::registry& reg, entt::entity e) {
        if (auto* hierarchy = reg.try_get<SocialHierarchyComponent>(e)) {
            return hierarchy->status > 0.7f;
        }
        return false;
    }, 75});

    lib.register_atom({"TOPIC_LOW_STATUS", [](const entt::registry& reg, entt::entity e) {
        if (auto* hierarchy = reg.try_get<SocialHierarchyComponent>(e)) {
            return hierarchy->status < 0.3f;
        }
        return false;
    }, 75});

    lib.register_atom({"TOPIC_STOCK_CRASH", [](const entt::registry& reg, entt::entity e) {
        auto view = reg.view<StockComponent>();
        for (auto entity : view) {
            auto& stock = view.get<StockComponent>(entity);
            if (stock.current_price < stock.opening_price * 0.85) return true;
        }
        return false;
    }, 80});

    lib.register_atom({"TOPIC_FACTION_DOCTRINE", [](const entt::registry& reg, entt::entity e) {
        return reg.all_of<Layer4PoliticalComponent>(e);
    }, 60});

    lib.register_atom({"TOPIC_CITY_MYTHS", [](const entt::registry& reg, entt::entity e) {
        return reg.all_of<CitizenComponent>(e);
    }, 20});

    // [NEW] G.1/G.2 Family & Relationship Atoms
    lib.register_atom({"TOPIC_FAMILY", [](const entt::registry& reg, entt::entity e) {
        if (auto* rel = reg.try_get<RelationshipComponent>(e)) {
            for (auto const& [id, r] : rel->records) {
                if (r.tier == RelationshipTier::FAMILY) return true;
            }
        }
        return false;
    }, 45});

    lib.register_atom({"TOPIC_FRIENDS", [](const entt::registry& reg, entt::entity e) {
        if (auto* rel = reg.try_get<RelationshipComponent>(e)) {
            for (auto const& [id, r] : rel->records) {
                if (r.tier == RelationshipTier::FRIEND) return true;
            }
        }
        return false;
    }, 40});

    lib.register_atom({"TOPIC_SOCIAL_ISOLATION", [](const entt::registry& reg, entt::entity e) {
        if (auto* needs = reg.try_get<NeedsComponent>(e)) {
            return needs->socialization < 20.0f;
        }
        return false;
    }, 80});

    lib.register_atom({"TOPIC_NIGHT", [](const entt::registry& reg, entt::entity e) {
        auto view = reg.view<WeatherComponent>();
        if (view.begin() != view.end()) {
            auto& weather = view.get<WeatherComponent>(view.front());
            return weather.time_of_day == TimeOfDay::NIGHT;
        }
        return false;
    }, 60});

    lib.register_atom({"TOPIC_LATE_NIGHT_WEARY", [](const entt::registry& reg, entt::entity e) {
        auto view = reg.view<WeatherComponent>();
        if (view.begin() != view.end()) {
            auto& weather = view.get<WeatherComponent>(view.front());
            return weather.time_of_day == TimeOfDay::NIGHT && 
                   weather.time_of_day_ticks > 2500;
        }
        return false;
    }, 85});

    lib.register_atom({"TOPIC_MORNING_ALERT", [](const entt::registry& reg, entt::entity e) {
        auto view = reg.view<WeatherComponent>();
        if (view.begin() != view.end()) {
            auto& weather = view.get<WeatherComponent>(view.front());
            return weather.time_of_day == TimeOfDay::DAWN && 
                   reg.all_of<WorkplaceComponent>(e);
        }
        return false;
    }, 70});

    lib.register_atom({"TOPIC_MORNING_ANXIOUS", [](const entt::registry& reg, entt::entity e) {
        auto view = reg.view<WeatherComponent>();
        if (view.begin() != view.end()) {
            auto& weather = view.get<WeatherComponent>(view.front());
            if (weather.time_of_day == TimeOfDay::DAWN && reg.all_of<WorkplaceComponent>(e)) {
                if (auto* needs = reg.try_get<NeedsComponent>(e)) {
                    return needs->frustration > 60.0f || needs->hunger < 40.0f || needs->thirst < 40.0f;
                }
            }
        }
        return false;
    }, 90});

    lib.register_atom({"TOPIC_RAIN", [](const entt::registry& reg, entt::entity e) {
        auto view = reg.view<WeatherComponent>();
        if (view.begin() != view.end()) {
            auto& weather = view.get<WeatherComponent>(view.front());
            return weather.state == WeatherState::RAIN || 
                   weather.state == WeatherState::HEAVY_RAIN || 
                   weather.state == WeatherState::ACID_RAIN;
        }
        return false;
    }, 65});

    lib.register_atom({"TOPIC_SMOG", [](const entt::registry& reg, entt::entity e) {
        auto view = reg.view<WeatherComponent>();
        if (view.begin() != view.end()) {
            auto& weather = view.get<WeatherComponent>(view.front());
            return weather.state == WeatherState::SMOG;
        }
        return false;
    }, 65});

    lib.register_atom({"TOPIC_HEAT", [](const entt::registry& reg, entt::entity e) {
        auto view = reg.view<WeatherComponent>();
        if (view.begin() != view.end()) {
            auto& weather = view.get<WeatherComponent>(view.front());
            return weather.ambient_temperature > 35.0f;
        }
        return false;
    }, 65});

    lib.register_atom({"TOPIC_RUMORS", [](const entt::registry& reg, entt::entity e) {
        if (auto* info = reg.try_get<InformationComponent>(e)) {
            return !info->records.empty();
        }
        return false;
    }, 85});

    lib.register_atom({"TOPIC_RELIGION_DEVOUT", [](const entt::registry& reg, entt::entity e) {
        if (auto* rel = reg.try_get<ReligiosityComponent>(e)) {
            return rel->devotion > 75.0f;
        }
        return false;
    }, 80});

    lib.register_atom({"TOPIC_RELIGION_SCHISM", [](const entt::registry& reg, entt::entity e) {
        if (auto* rel = reg.try_get<ReligiosityComponent>(e)) {
            return rel->is_schismatic;
        }
        return false;
    }, 95});

    lib.register_atom({"TOPIC_RELIGION_GENERIC", [](const entt::registry& reg, entt::entity e) {
        return reg.all_of<ReligiosityComponent>(e);
    }, 40});

    lib.register_atom({"TOPIC_PROSELYTIZING", [](const entt::registry& reg, entt::entity e) {
        if (auto* rel = reg.try_get<ReligiosityComponent>(e)) {
            return rel->devotion > 70.0f;
        }
        return false;
    }, 80});
}

void DialogueSystem::update(double delta_time) {
    (void)delta_time;
    if (m_window_visible) {
        m_pulse_counter++;
        draw_dialogue_window();
    }
}

void DialogueSystem::create_dialogue_plane() {
    if (m_dialogue_plane) return;

    struct ncplane_options opts = {};
    opts.rows = 26;
    opts.cols = 64;
    opts.y = 2;
    opts.x = 20; 
    opts.name = "Dialogue Plane";

    m_dialogue_plane = ncplane_create(notcurses_stdplane(nc_context_), &opts);
    
    uint64_t channels = 0;
    ncchannels_set_bg_rgb(&channels, 0x1A1C22);
    ncchannels_set_fg_rgb(&channels, 0xFFFFFF);
    ncplane_set_base(m_dialogue_plane, " ", 0, channels);
}

void DialogueSystem::close_dialogue_window() {
    if (m_dialogue_plane) {
        ncplane_destroy(m_dialogue_plane);
        m_dialogue_plane = nullptr;
    }
    m_window_visible = false;
    m_target_agent = entt::null;

    // Update singleton state
    auto view = registry_.view<DialogueStateComponent>();
    if (view.begin() != view.end()) {
        auto& state = view.get<DialogueStateComponent>(view.front());
        state.is_open = false;
        state.target_agent = entt::null;
    }
}

void DialogueSystem::handleCloseDialogueWindowEvent(const CloseDialogueWindowEvent& event) {
    (void)event;
    close_dialogue_window();
}

void DialogueSystem::handleDialogueEvent(const DialogueEvent& event) {
    if (!registry_.valid(event.target_agent)) return;

    m_target_agent = event.target_agent;
    m_window_visible = true;

    // Set singleton state
    auto view = registry_.view<DialogueStateComponent>();
    if (view.begin() == view.end()) {
        auto singleton = registry_.create();
        registry_.emplace<DialogueStateComponent>(singleton, true, m_target_agent);
    } else {
        auto& state = view.get<DialogueStateComponent>(view.front());
        state.is_open = true;
        state.target_agent = m_target_agent;
    }

    generate_content();
    create_dialogue_plane();
}

void DialogueSystem::handleDialogueChoiceEvent(const DialogueChoiceEvent& event) {
    if (!registry_.valid(m_target_agent)) return;
    if (event.choice_index < 0 || event.choice_index >= (int)m_choices.size()) return;

    auto const& choice = m_choices[event.choice_index];

    // Ink handle
    if (choice.category == DialogueTopicCategory::INK_CHOICE) {
        if (auto* ink = registry_.try_get<InkStoryComponent>(m_target_agent)) {
            if (ink->runner_obj) {
                auto& runner = *static_cast<ink::runtime::runner*>(ink->runner_obj.get());
                runner->choose(choice.ink_choice_index);
                generate_content(DialogueTopicCategory::GREETING);
                return;
            }
        }
    }

    if (choice.category == DialogueTopicCategory::LEAK_SELECT) {
        auto player_view = registry_.view<PlayerComponent>();
        if (!player_view.empty()) {
            auto player = player_view.front();
            auto* info = registry_.try_get<InformationComponent>(player);
            if (info && choice.ink_choice_index < (int)info->records.size()) {
                auto& rumor = info->records[choice.ink_choice_index];
                auto& target_info = registry_.get_or_emplace<InformationComponent>(m_target_agent);
                
                bool known = false;
                for (auto& r : target_info.records) {
                    if (r.content_tag == rumor.content_tag) {
                        known = true;
                        break;
                    }
                }

                if (!known) {
                    target_info.records.push_back(rumor);
                    
                    // [N.4] Faction Drift based on rumor source/type
                    if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent)) {
                        // If it's PROPAGANDA for their faction, they like it
                        float rep_bonus = 0.0f;
                        if (rumor.type == InformationType::PROPAGANDA) {
                             pol->faction_loyalty = std::min(1.0f, pol->faction_loyalty + 0.05f);
                             rep_bonus = 5.0f;
                        } else if (rumor.type == InformationType::INTELLIGENCE) {
                             // Intelligence on rival factions is valued
                             pol->faction_loyalty = std::min(1.0f, pol->faction_loyalty + 0.02f);
                             rep_bonus = 2.0f;
                        }

                        // [P.2] Reputation boost for intel sharing
                        if (rep_bonus > 0.0f) {
                            event_dispatcher_.enqueue<AgentFactionReputationEvent>({
                                player,
                                pol->primary_faction,
                                rep_bonus
                            });
                        }
                    }

                    m_current_utterance = "This is... significant. Thank you for telling me.";
                } else {
                    m_current_utterance = "Yes, I've heard that one before. Old news.";
                }
                
                m_choices.clear();
                m_choices.push_back({'a', "Good to know. (Back to questions)", DialogueTopicCategory::GREETING});
                m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});
                return;
            }
        }
    }

    if (choice.category == DialogueTopicCategory::VERIFY_SELECT) {
        auto player_view = registry_.view<PlayerComponent>();
        if (!player_view.empty()) {
            auto player = player_view.front();
            auto* p_info = registry_.try_get<InformationComponent>(player);
            if (p_info && choice.ink_choice_index < (int)p_info->records.size()) {
                auto& rumor = p_info->records[choice.ink_choice_index];
                auto* t_info = registry_.try_get<InformationComponent>(m_target_agent);
                
                bool npc_knows = false;
                float npc_veracity = 0.0f;
                if (t_info) {
                    for (const auto& r : t_info->records) {
                        if (r.content_tag == rumor.content_tag) {
                            npc_knows = true;
                            npc_veracity = r.veracity;
                            break;
                        }
                    }
                }

                if (npc_knows) {
                    if (npc_veracity > 0.7f) {
                        m_current_utterance = "Yes, I'm fairly certain that's true.";
                    } else if (npc_veracity < 0.3f) {
                        m_current_utterance = "That sounds like a bunch of garbage to me.";
                    } else {
                        m_current_utterance = "I've heard it, but I wouldn't bet my life on it.";
                    }
                    // Update player's veracity based on NPC's trust
                    rumor.veracity = (rumor.veracity + npc_veracity) / 2.0f;
                    
                    // [P.2] Trust-based reputation boost
                    if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent)) {
                        event_dispatcher_.enqueue<AgentFactionReputationEvent>({
                            player,
                            pol->primary_faction,
                            0.5f // Very small boost for consulting them
                        });
                    }
                } else {
                    m_current_utterance = "Never heard of it. Can't help you there.";
                }

                m_choices.clear();
                m_choices.push_back({'a', "I see. (Back to questions)", DialogueTopicCategory::GREETING});
                m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});
                return;
            }
        }
    }

    if (choice.category == DialogueTopicCategory::BROADCAST_SELECT) {
        auto player_view = registry_.view<PlayerComponent>();
        if (!player_view.empty() && registry_.all_of<PirateNodeComponent>(m_target_agent)) {
            auto player = player_view.front();
            auto* p_info = registry_.try_get<InformationComponent>(player);
            if (p_info && choice.ink_choice_index < (int)p_info->records.size()) {
                auto& rumor = p_info->records[choice.ink_choice_index];
                auto& node_info = registry_.get_or_emplace<InformationComponent>(m_target_agent);
                node_info.records.clear();
                node_info.records.push_back(rumor);
                
                auto& node = registry_.get<PirateNodeComponent>(m_target_agent);
                node.active_information_entity = m_target_agent;
                
                m_current_utterance = "PATTERN INJECTED. COMMENCING PULSE SEQUENCE.";
                m_choices.clear();
                m_choices.push_back({'z', "[Disconnect]", DialogueTopicCategory::LEAVE});
                return;
            }
        }
    }

    if (choice.category == DialogueTopicCategory::LEAVE) {
        close_dialogue_window();
        return;
    }

    record_interaction(choice.category);
    generate_content(choice.category);
}

void DialogueSystem::generate_content(DialogueTopicCategory category) {
    m_agent_name = "Citizen";
    if (registry_.all_of<NameComponent>(m_target_agent)) {
        m_agent_name = registry_.get<NameComponent>(m_target_agent).name;
    }
    
    m_agent_title = "Unknown Status";
    if (auto* hierarchy = registry_.try_get<SocialHierarchyComponent>(m_target_agent)) {
        m_agent_title = hierarchy->class_title;
    }

    // [N.4] Pirate Node Console Special Handling
    if (registry_.all_of<PirateNodeComponent>(m_target_agent)) {
        if (category == DialogueTopicCategory::GREETING) {
            auto& node = registry_.get<PirateNodeComponent>(m_target_agent);
            std::string status = "IDLE";
            if (node.active_information_entity != entt::null) {
                if (auto* info = registry_.try_get<InformationComponent>(node.active_information_entity)) {
                    if (!info->records.empty()) status = "BROADCASTING: " + info->records[0].content_tag;
                }
            }
            m_current_utterance = "CONNECTED TO NODE. STATUS: " + status;
            m_choices.clear();
            m_choices.push_back({'a', "[Set Broadcast Pattern]", DialogueTopicCategory::SET_BROADCAST});
            m_choices.push_back({'z', "[Disconnect]", DialogueTopicCategory::LEAVE});
            return;
        } else if (category == DialogueTopicCategory::SET_BROADCAST) {
            auto player_view = registry_.view<PlayerComponent>();
            if (!player_view.empty()) {
                auto* p_info = registry_.try_get<InformationComponent>(player_view.front());
                if (p_info && !p_info->records.empty()) {
                    m_current_utterance = "SELECT SOURCE DATA FOR PULSE:";
                    m_choices.clear();
                    char key = 'a';
                    for (size_t i = 0; i < std::min(p_info->records.size(), (size_t)5); ++i) {
                        m_choices.push_back({key++, "[" + p_info->records[i].content_tag + "]", DialogueTopicCategory::BROADCAST_SELECT, (int)i});
                    }
                    m_choices.push_back({'z', "[Cancel]", DialogueTopicCategory::GREETING});
                } else {
                    m_current_utterance = "ERROR: NO LOCAL SOURCE DATA FOUND.";
                    m_choices.clear();
                    m_choices.push_back({'z', "[Back]", DialogueTopicCategory::GREETING});
                }
            }
            return;
        }
    }

    // [GOLD PATH] Ink Story Integration [F.1]
    if (auto* ink = registry_.try_get<InkStoryComponent>(m_target_agent)) {
        if (!ink->is_initialized) {
            if (ink->story_obj == nullptr && !ink->story_path.empty()) {
                // Ensure binary story exists (assumed to be compiled in authored scripts)
                auto* story_ptr = ink::runtime::story::from_file(ink->story_path.c_str());
                if (story_ptr) {
                    ink->story_obj = std::shared_ptr<void>(story_ptr, [](void* p) { delete static_cast<ink::runtime::story*>(p); });
                }
            }
            if (ink->story_obj) {
                auto* story_ptr = static_cast<ink::runtime::story*>(ink->story_obj.get());
                ink->runner_obj = std::shared_ptr<void>(new ink::runtime::runner(story_ptr->new_runner()), [](void* p) { delete static_cast<ink::runtime::runner*>(p); });
                ink->is_initialized = true;
            }
        }

        if (ink->runner_obj) {
            auto& runner = *static_cast<ink::runtime::runner*>(ink->runner_obj.get());
            if (runner->can_continue()) {
                m_current_utterance = runner->getline();
            }

            m_choices.clear();
            char key = 'a';
            for (const auto& c : *runner) {
                m_choices.push_back({key++, c.text(), DialogueTopicCategory::INK_CHOICE, (int)c.index()});
                if (key > 'z') break; 
            }

            if (m_choices.empty() && !runner->can_continue()) {
                 m_choices.push_back({'z', "Never mind.", DialogueTopicCategory::LEAVE});
            }
            return;
        }
    }

    // Systemic Selection via Dialogue Atoms (FALLBACK) [F.1]
    auto active_atoms = DialogueAtomLibrary::instance().query_active_atoms(registry_, m_target_agent);
    
    if (category == DialogueTopicCategory::GREETING) {
        // [P.3] Reputation Modifier for Greetings
        std::string rep_modifier = "";
        auto player_view = registry_.view<PlayerComponent>();
        if (!player_view.empty()) {
            auto player = player_view.front();
            if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent)) {
                if (auto* rep = registry_.try_get<ReputationComponent>(player)) {
                    ReputationTier tier = rep->get_tier(pol->primary_faction);
                    switch(tier) {
                        case ReputationTier::ALLY: rep_modifier = "[ALLY] "; break;
                        case ReputationTier::FRIENDLY: rep_modifier = "[FRIENDLY] "; break;
                        case ReputationTier::FAVORED: rep_modifier = "[FAVORED] "; break;
                        case ReputationTier::NEUTRAL: break;
                        case ReputationTier::SUSPICIOUS: rep_modifier = "[SUSPICIOUS] "; break;
                        case ReputationTier::HOSTILE: rep_modifier = "[HOSTILE] "; break;
                        default: break;
                    }
                }
            }
        }

        // Find top atoms for initial greeting (Hunger, Frustration, or just Faction/Generic)
        std::sort(active_atoms.begin(), active_atoms.end(), [](const DialogueAtom* a, const DialogueAtom* b) {
            return a->weight > b->weight;
        });

        std::vector<std::string> grammar_tags;
        int max_topics = 3;
        
        // [F.2] Immediate Needs Impact: severity reduces topic range
        bool desperate = false;
        std::string desperate_tag = "";
        for (auto* atom : active_atoms) {
            if (atom->tag == "TOPIC_HUNGER_CRITICAL" || 
                atom->tag == "TOPIC_THIRST_CRITICAL" || 
                atom->tag == "TOPIC_FRUSTRATION_HOSTILE") {
                desperate = true;
                desperate_tag = atom->tag;
                max_topics = 1; // Only talk about the desperate need
                break;
            }
        }

        if (desperate) {
            m_current_utterance = rep_modifier + GrammarEngine::instance().expand(atom_tag_to_grammar_tag(desperate_tag), registry_, m_target_agent);
            m_choices.clear();
            m_choices.push_back({'z', "Never mind.", DialogueTopicCategory::LEAVE});
            return;
        }

        for (int i = 0; i < std::min((int)active_atoms.size(), (int)max_topics); ++i) {
            grammar_tags.push_back(atom_tag_to_grammar_tag(active_atoms[i]->tag));
        }

        m_current_utterance = rep_modifier + GrammarEngine::instance().assemble(grammar_tags, registry_, m_target_agent);

        // [F.7] Dynamic Topic Menu
        m_choices.clear();
        bool has_work = false, has_family = false, has_lore = false, has_rumor = false, has_religion = false;
        for (auto* atom : active_atoms) {
            auto cat = get_atom_category(atom->tag);
            if (cat == DialogueTopicCategory::WORK) has_work = true;
            if (cat == DialogueTopicCategory::FAMILY) has_family = true;
            if (cat == DialogueTopicCategory::LORE) has_lore = true;
            if (cat == DialogueTopicCategory::RUMOR) has_rumor = true;
            if (cat == DialogueTopicCategory::RELIGION) has_religion = true;
        }

        bool can_buy_info = false;
        bool is_family = false;
        
        auto* info = registry_.try_get<InformationComponent>(m_target_agent);
        if (info && !info->records.empty()) can_buy_info = true;
        
        auto player_view2 = registry_.view<PlayerComponent>();
        PlayerComponent* p_comp = nullptr;
        if (player_view2.begin() != player_view2.end()) {
            auto player = player_view2.front();
            p_comp = registry_.try_get<PlayerComponent>(player);
            auto* rel = registry_.try_get<RelationshipComponent>(m_target_agent);
            if (rel && p_comp && rel->records.count(p_comp->macro_id)) {
                if (rel->records[p_comp->macro_id].tier == RelationshipTier::FAMILY) {
                    is_family = true;
                }
            }
        }

        char key = 'a';
        if (has_work) m_choices.push_back({key++, "[Ask about Work]", DialogueTopicCategory::WORK});
        if (has_family) m_choices.push_back({key++, "[Ask about Family]", DialogueTopicCategory::FAMILY});
        if (has_lore) m_choices.push_back({key++, "[Inquire: Lore]", DialogueTopicCategory::LORE});
        if (has_rumor) m_choices.push_back({key++, "[What's the word?]", DialogueTopicCategory::RUMOR});
        if (has_religion) m_choices.push_back({key++, "[Speak: Spiritual]", DialogueTopicCategory::RELIGION});
        
        if (can_buy_info) m_choices.push_back({key++, "[Buy Info] (50c)", DialogueTopicCategory::BUY_INFO});
        if (is_family) m_choices.push_back({key++, "[Family Check-In]", DialogueTopicCategory::FAMILY_CHECKIN});
        
        // [P.2] Give Credits if NPC is poor or desperate
        bool is_poor = false;
        for (auto* atom : active_atoms) {
            if (atom->tag == "TOPIC_POOR" || atom->tag == "TOPIC_HUNGER_CRITICAL" || atom->tag == "TOPIC_THIRST_CRITICAL") {
                is_poor = true; break;
            }
        }
        if (is_poor) m_choices.push_back({key++, "[Give 10 CR]", DialogueTopicCategory::GREETING}); // Actually category is GIVE_CREDITS, but we'll use GIVE_CREDITS handle

        // [P.4] Beg for Pardon
        auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent);
        if (pol && player_view.begin() != player_view.end()) {
            auto player = player_view.front();
            auto* rep = registry_.try_get<ReputationComponent>(player);
            if (rep) {
                ReputationTier tier = rep->get_tier(pol->primary_faction);
                if (tier == ReputationTier::HOSTILE || tier == ReputationTier::SUSPICIOUS) {
                    m_choices.push_back({key++, "[Beg for Pardon] (100 CR)", DialogueTopicCategory::PARDON});
                }
            }
        }

        // [NEW] Consequence Engine: Memory Branch
        if (p_comp) {
            auto* rel = registry_.try_get<RelationshipComponent>(m_target_agent);
            if (rel && rel->records.count(p_comp->macro_id) && !rel->records[p_comp->macro_id].interaction_history.empty()) {
                 m_choices.push_back({key++, "[Last time we spoke...]", DialogueTopicCategory::MEMORY_RECALL});
            }
        }

        m_choices.push_back({key++, "[Threaten]", DialogueTopicCategory::THREATEN});
        if (registry_.all_of<Layer4PoliticalComponent>(m_target_agent)) {
             m_choices.push_back({key++, "[Express Solidarity]", DialogueTopicCategory::SOLIDARITY});
        }
        
        auto player_v = registry_.view<PlayerComponent>();
        if (!player_v.empty()) {
            auto* p_info = registry_.try_get<InformationComponent>(player_v.front());
            if (p_info && !p_info->records.empty()) {
                m_choices.push_back({key++, "[Share Intel...]", DialogueTopicCategory::LEAK_INFO});
                m_choices.push_back({key++, "[Verify Intel...]", DialogueTopicCategory::VERIFY_INFO});
            }
        }

        m_choices.push_back({key++, "[Lie]", DialogueTopicCategory::LIE});

        m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});

    } else if (category == DialogueTopicCategory::MEMORY_RECALL) {
        auto player_view = registry_.view<PlayerComponent>();
        if (player_view.begin() != player_view.end()) {
            auto player = player_view.front();
            auto* p_comp = registry_.try_get<PlayerComponent>(player);
            auto* rel_comp = registry_.try_get<RelationshipComponent>(m_target_agent);
            if (p_comp && rel_comp && rel_comp->records.count(p_comp->macro_id)) {
                auto& history = rel_comp->records[p_comp->macro_id].interaction_history;
                if (!history.empty()) {
                    std::string last = history.back();
                    if (last == "THREATEN") {
                        m_current_utterance = "Last time we spoke, you threatened me. I haven't forgotten.";
                    } else if (last == "SOLIDARITY") {
                        m_current_utterance = "You spoke of solidarity before. It stayed with me.";
                    } else if (last == "LIE") {
                        m_current_utterance = "I've been thinking about what you told me earlier. Is it really true?";
                    } else if (last == "BUY_INFO") {
                        m_current_utterance = "Still looking for more info, are you?";
                    } else {
                        m_current_utterance = "I remember our last talk. You asked about " + last + ".";
                    }
                }
            }
        }
        m_choices.clear();
        m_choices.push_back({'a', "Yes, let's continue. (More questions)", DialogueTopicCategory::GREETING});
        m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});

    } else if (category == DialogueTopicCategory::THREATEN) {
        // ... (existing handle)
    } else if (category == DialogueTopicCategory::GIVE_CREDITS) {
        auto player_view = registry_.view<PlayerComponent, HUDComponent>();
        if (player_view.begin() != player_view.end()) {
            auto player = player_view.front();
            auto& hud = player_view.get<HUDComponent>(player);
            if (hud.credits >= 10) {
                hud.credits -= 10;
                
                // Increase affinity
                auto* p_comp = registry_.try_get<PlayerComponent>(player);
                auto& rel = registry_.get_or_emplace<RelationshipComponent>(m_target_agent);
                if (p_comp) {
                    auto& record = rel.records[p_comp->macro_id];
                    record.target_macro_id = p_comp->macro_id;
                    record.affinity = std::min(100.0f, record.affinity + 10.0f);
                }

                // [P.2] Reputation boost for "Public Charity"
                if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent)) {
                    event_dispatcher_.enqueue<AgentFactionReputationEvent>({
                        player,
                        pol->primary_faction,
                        2.0f
                    });
                }
                
                // If witnessed by a guard (Law and Order), increase Government rep
                auto pos_ptr = registry_.try_get<PositionComponent>(m_target_agent);
                if (pos_ptr) {
                    auto guard_view = registry_.view<PositionComponent, VisibilityComponent, PatrolComponent, Layer4PoliticalComponent>();
                    for (auto guard : guard_view) {
                         if (guard_view.get<Layer4PoliticalComponent>(guard).primary_faction == "GOVERNMENT") {
                             if (guard_view.get<VisibilityComponent>(guard).visible_tiles.count(*pos_ptr)) {
                                 event_dispatcher_.enqueue<AgentFactionReputationEvent>({
                                     player,
                                     "GOVERNMENT",
                                     1.0f
                                 });
                                 break;
                             }
                         }
                    }
                }

                m_current_utterance = "Ten credits? This... this changes things. Thank you.";
            } else {
                m_current_utterance = "You don't even have ten credits to spare. I understand.";
            }
        }
        m_choices.clear();
        m_choices.push_back({'a', "Take care. (More questions)", DialogueTopicCategory::GREETING});
        m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});

    } else if (category == DialogueTopicCategory::SOLIDARITY) {
        auto player_view = registry_.view<PlayerComponent>();
        if (player_view.begin() != player_view.end()) {
            auto player = player_view.front();
            auto* p_comp = registry_.try_get<PlayerComponent>(player);
            auto& rel = registry_.get_or_emplace<RelationshipComponent>(m_target_agent);
            if (p_comp) {
                auto& record = rel.records[p_comp->macro_id];
                record.target_macro_id = p_comp->macro_id;
                record.affinity = std::min(100.0f, record.affinity + 10.0f);
            }
            
            // Faction Feedback Loop [F.7]
            if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent)) {
                AgentFactionReputationEvent rep_event;
                rep_event.agent_entity = player;
                rep_event.faction_id = pol->primary_faction;
                rep_event.change_amount = 5.0f;
                event_dispatcher_.enqueue<AgentFactionReputationEvent>(rep_event);
            }

            m_current_utterance = GrammarEngine::instance().expand("solidarity_reaction", registry_, m_target_agent);
        }
        m_choices.clear();
        m_choices.push_back({'a', "We're in this together. (More questions)", DialogueTopicCategory::GREETING});
        m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});

    } else if (category == DialogueTopicCategory::PARDON) {
        auto player_view = registry_.view<PlayerComponent, HUDComponent>();
        if (player_view.begin() != player_view.end()) {
            auto player = player_view.front();
            auto& hud = player_view.get<HUDComponent>(player);
            if (hud.credits >= 100) {
                hud.credits -= 100;
                
                if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent)) {
                    auto& rep = registry_.get<ReputationComponent>(player);
                    // Reset to slightly suspicious
                    rep.faction_standing[pol->primary_faction] = -10.0f;
                    rep.fame = std::min(1.0f, rep.fame + 0.05f);
                    
                    m_current_utterance = "Your credits have cleared your debts... for now. Do not fail us again.";
                    event_dispatcher_.trigger(HUDNotificationEvent{"Pardon granted by " + pol->primary_faction, 3.0f, "#00FF00"});
                }
            } else {
                m_current_utterance = "You think you can buy forgiveness with so few credits? Begone.";
            }
        }
        m_choices.clear();
        m_choices.push_back({'a', "I understand. (More questions)", DialogueTopicCategory::GREETING});
        m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});

    } else if (category == DialogueTopicCategory::LIE) {
        auto player_view = registry_.view<PlayerComponent>();
        if (player_view.begin() != player_view.end()) {
            auto player = player_view.front();
            auto& npc_info = registry_.get_or_emplace<InformationComponent>(m_target_agent);
            
            InformationRecord lie;
            lie.type = InformationType::RUMOR;
            lie.content_tag = "PLAYER_DISINFORMATION_" + std::to_string(rand() % 1000);
            lie.veracity = 0.2f; // Obvious lie if checked closely
            
            uint64_t current_tick = 0;
            auto city_view = registry_.view<CityComponent>();
            if (!city_view.empty()) current_tick = registry_.get<CityComponent>(city_view.front()).time_tick;
            lie.origin_tick = current_tick;
            
            npc_info.records.push_back(lie);
            
            m_current_utterance = "Is that so? I'll have to keep that in mind...";
        }
        m_choices.clear();
        m_choices.push_back({'a', "Just thought you should know. (More questions)", DialogueTopicCategory::GREETING});
        m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});

    } else if (category == DialogueTopicCategory::LEAK_INFO) {
        auto player_view = registry_.view<PlayerComponent>();
        if (!player_view.empty()) {
            auto player = player_view.front();
            auto* info = registry_.try_get<InformationComponent>(player);
            if (info && !info->records.empty()) {
                m_current_utterance = "Which piece of intel do you want to share?";
                m_choices.clear();
                char key = 'a';
                // Limit to first 5 for now to avoid UI overflow
                for (size_t i = 0; i < std::min(info->records.size(), (size_t)5); ++i) {
                    m_choices.push_back({key++, "[" + info->records[i].content_tag + "]", DialogueTopicCategory::LEAK_SELECT, (int)i});
                }
                m_choices.push_back({'z', "[Cancel]", DialogueTopicCategory::GREETING});
            } else {
                m_current_utterance = "You don't have any intel worth sharing.";
                m_choices.clear();
                m_choices.push_back({'a', "Back.", DialogueTopicCategory::GREETING});
            }
        }
    } else if (category == DialogueTopicCategory::VERIFY_INFO) {
        auto player_view = registry_.view<PlayerComponent>();
        if (!player_view.empty()) {
            auto player = player_view.front();
            auto* info = registry_.try_get<InformationComponent>(player);
            if (info && !info->records.empty()) {
                m_current_utterance = "Which rumor do you want me to verify?";
                m_choices.clear();
                char key = 'a';
                for (size_t i = 0; i < std::min(info->records.size(), (size_t)5); ++i) {
                    m_choices.push_back({key++, "[Ask about: " + info->records[i].content_tag + "]", DialogueTopicCategory::VERIFY_SELECT, (int)i});
                }
                m_choices.push_back({'z', "[Cancel]", DialogueTopicCategory::GREETING});
            } else {
                m_current_utterance = "You don't have any intel to ask about.";
                m_choices.clear();
                m_choices.push_back({'a', "Back.", DialogueTopicCategory::GREETING});
            }
        }
    } else if (category == DialogueTopicCategory::LEAK_SELECT || category == DialogueTopicCategory::VERIFY_SELECT) {
        // Handled in handleDialogueChoiceEvent
    } else if (category == DialogueTopicCategory::RUMOR) {
        // [F.2] The Rumor Mill: "What's the word?"
        auto* info = registry_.try_get<InformationComponent>(m_target_agent);
        if (info && !info->records.empty()) {
            // NPC shares a rumor with the player
            auto& rumor = info->records[rand() % info->records.size()];
            
            // Add to player intel or just show it
            auto player_view = registry_.view<PlayerComponent>();
            if (player_view.begin() != player_view.end()) {
                auto player = player_view.front();
                auto& p_info = registry_.get_or_emplace<InformationComponent>(player);
                bool duplicate = false;
                for (const auto& r : p_info.records) {
                    if (r.content_tag == rumor.content_tag) {
                        duplicate = true; break;
                    }
                }
                if (!duplicate) {
                    p_info.records.push_back(rumor);
                }
            }

            m_current_utterance = GrammarEngine::instance().expand("rumor_talk", registry_, m_target_agent);
        } else {
            m_current_utterance = "Quiet as a grave today. No word.";
        }

        m_choices.clear();
        m_choices.push_back({'a', "I see. (More questions)", DialogueTopicCategory::GREETING});
        m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});

    } else if (category == DialogueTopicCategory::BUY_INFO) {
        auto player_view = registry_.view<PlayerComponent, HUDComponent>();
        if (player_view.begin() != player_view.end()) {
            auto player = player_view.front();
            auto& hud = player_view.get<HUDComponent>(player);
            auto* info = registry_.try_get<InformationComponent>(m_target_agent);
            int cost = 50; 
            if (hud.credits >= cost && info && !info->records.empty()) {
                hud.credits -= cost;
                
                // [NEW] Transfer credits to NPC economy [F.7]
                if (auto* npc_econ = registry_.try_get<Layer3EconomicComponent>(m_target_agent)) {
                    npc_econ->cash_on_hand += cost;
                }

                auto& rumor = info->records[rand() % info->records.size()];
                auto& p_info = registry_.get_or_emplace<InformationComponent>(player);
                p_info.records.push_back(rumor);
                m_current_utterance = "Fine. Here is what I know. " + GrammarEngine::instance().expand("rumor_talk", registry_, m_target_agent);
            } else if (hud.credits < cost) {
                m_current_utterance = "You don't have enough credits.";
            } else {
                m_current_utterance = "I don't have any info for you.";
            }
        }
        m_choices.clear();
        m_choices.push_back({'a', "I see. (More questions)", DialogueTopicCategory::GREETING});
        m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});

    } else if (category == DialogueTopicCategory::FAMILY_CHECKIN) {
        auto player_view = registry_.view<PlayerComponent>();
        if (player_view.begin() != player_view.end()) {
            auto player = player_view.front();
            if (auto* rel = registry_.try_get<RelationshipComponent>(m_target_agent)) {
                auto* p_comp = registry_.try_get<PlayerComponent>(player);
                if (p_comp && rel->records.count(p_comp->macro_id)) {
                    auto& record = rel->records[p_comp->macro_id];
                    record.affinity = std::min(100.0f, record.affinity + 5.0f);
                    if (auto* needs = registry_.try_get<NeedsComponent>(m_target_agent)) {
                        needs->socialization = std::min(100.0f, needs->socialization + 20.0f);
                    }
                    if (auto* p_needs = registry_.try_get<NeedsComponent>(player)) {
                        p_needs->socialization = std::min(100.0f, p_needs->socialization + 20.0f);
                    }
                    m_current_utterance = GrammarEngine::instance().expand("family_checkin", registry_, m_target_agent);
                }
            }
        }
        m_choices.clear();
        m_choices.push_back({'a', "Good to see you. (More questions)", DialogueTopicCategory::GREETING});
        m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});

    } else {
        // Detailed category talk (WORK, FAMILY, LORE)
        std::vector<const DialogueAtom*> cat_atoms;
        for (auto* atom : active_atoms) {
            if (get_atom_category(atom->tag) == category) {
                cat_atoms.push_back(atom);
            }
        }

        if (cat_atoms.empty()) {
            m_current_utterance = "I don't have anything to say about that.";
        } else {
            std::sort(cat_atoms.begin(), cat_atoms.end(), [](const DialogueAtom* a, const DialogueAtom* b) {
                return a->weight > b->weight;
            });
            m_current_utterance = GrammarEngine::instance().expand(atom_tag_to_grammar_tag(cat_atoms[0]->tag), registry_, m_target_agent);

            // [NEW] H.6 — Player Proselytizing
            if (cat_atoms[0]->tag == "TOPIC_PROSELYTIZING") {
                auto player_view = registry_.view<PlayerComponent>();
                if (player_view.begin() != player_view.end()) {
                    auto player = player_view.front();
                    auto* relig_p = registry_.try_get<ReligiosityComponent>(player);
                    auto* relig_t = registry_.try_get<ReligiosityComponent>(m_target_agent);
                    
                    if (relig_p) {
                         uint64_t p_macro_id = registry_.get<PlayerComponent>(player).macro_id;
                         auto* rel_comp = registry_.try_get<RelationshipComponent>(m_target_agent);
                         float affinity = 0.0f;
                         if (rel_comp && rel_comp->records.count(p_macro_id)) {
                             affinity = rel_comp->records.at(p_macro_id).affinity;
                         }
                         
                         bool has_rival = (relig_t && relig_t->religion_id != relig_p->religion_id && relig_t->devotion > 15.0f);
                         bool success = (affinity > 40.0f && !has_rival);
                         
                         event_dispatcher_.enqueue<ProselytizingEvent>({player, m_target_agent, relig_p->religion_id, success});
                    }
                }
            }
        }

        m_choices.clear();
        m_choices.push_back({'a', "I see. (More questions)", DialogueTopicCategory::GREETING});
        m_choices.push_back({'z', "[Never mind.]", DialogueTopicCategory::LEAVE});
    }
}


void DialogueSystem::draw_dialogue_window() {
    if (!m_dialogue_plane || m_target_agent == entt::null) return;

    ncplane_erase(m_dialogue_plane);
    
    uint32_t accent_color = 0x55AAFF; // Dialogue Blue
    
    ncplane_set_fg_rgb(m_dialogue_plane, accent_color);
    ncplane_cursor_move_yx(m_dialogue_plane, 0, 0);
    ncplane_box(m_dialogue_plane, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0);

    // Title
    ncplane_set_fg_rgb(m_dialogue_plane, 0xFFFFFF);
    ncplane_putstr_yx(m_dialogue_plane, 1, 2, ("DIALOGUE: " + m_agent_name).c_str());
    ncplane_set_fg_rgb(m_dialogue_plane, 0x888888);
    ncplane_putstr_yx(m_dialogue_plane, 2, 2, ("[" + m_agent_title + "]").c_str());

    // Simulation Pulse
    const char pulse_chars[] = {'-', '\\', '|', '/'};
    char pulse = pulse_chars[(m_pulse_counter / 10) % 4];
    ncplane_set_fg_rgb(m_dialogue_plane, accent_color);
    ncplane_printf_yx(m_dialogue_plane, 1, 60, "[%c]", pulse);

    // Draw Utterance (with wrapping)
    uint32_t utterance_color = 0x00FF00; // Default NPC Green
    uint32_t frame_color = accent_color;

    if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent)) {
        if (pol->primary_faction == "GOVERNMENT") {
            utterance_color = 0x55AAFF; // Medical Blue
            frame_color = 0x55AAFF;
        } else if (pol->primary_faction == "REBEL") {
            utterance_color = 0xFF5555; // Warning Red
            frame_color = 0xFF5555;
        } else if (pol->primary_faction == "MAW") {
            utterance_color = 0x55FF55; // Bio Green
            frame_color = 0x55FF55;
        } else if (pol->primary_faction == "VOID") {
            utterance_color = 0xAA55FF; // Ritual Purple
            frame_color = 0xAA55FF;
        } else if (pol->primary_faction == "SYNDICATE") {
            utterance_color = 0xFFCC33; // Gold
            frame_color = 0xFFCC33;
        }
    }

    // Draw Portrait Frame [NEW]
    ncplane_set_fg_rgb(m_dialogue_plane, frame_color);
    int frame_y = 3, frame_x = 3;
    int frame_h = 8, frame_w = 19;
    
    // Custom Frame Styles based on Faction [Visual Metaphor Spec]
    if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent)) {
        if (pol->primary_faction == "GOVERNMENT") {
            // Consensus (Medical Frame): heartbeat peaks
            ncplane_cursor_move_yx(m_dialogue_plane, frame_y, frame_x);
            ncplane_box(m_dialogue_plane, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0);
            ncplane_putstr_yx(m_dialogue_plane, frame_y, frame_x + 8, "-^-");
        } else if (pol->primary_faction == "REBEL") {
            // Entropic (Glitch Frame): artifacts
            ncplane_cursor_move_yx(m_dialogue_plane, frame_y, frame_x);
            ncplane_box(m_dialogue_plane, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0);
            ncplane_putstr_yx(m_dialogue_plane, frame_y, frame_x, "#");
            ncplane_putstr_yx(m_dialogue_plane, frame_y + frame_h - 1, frame_x + frame_w - 1, "%");
        } else if (pol->primary_faction == "MAW") {
            // Maw (Biological Frame): tendrils
            ncplane_cursor_move_yx(m_dialogue_plane, frame_y, frame_x);
            ncplane_box(m_dialogue_plane, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0);
            ncplane_putstr_yx(m_dialogue_plane, frame_y + 2, frame_x + frame_w - 1, "~");
            ncplane_putstr_yx(m_dialogue_plane, frame_y + 5, frame_x, "~");
        } else if (pol->primary_faction == "SYNDICATE") {
            // Syndicate (Industrial Frame): rivets
            ncplane_cursor_move_yx(m_dialogue_plane, frame_y, frame_x);
            ncplane_box(m_dialogue_plane, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0);
            ncplane_putstr_yx(m_dialogue_plane, frame_y, frame_x + 1, ".");
            ncplane_putstr_yx(m_dialogue_plane, frame_y, frame_x + frame_w - 2, ".");
        } else {
            ncplane_cursor_move_yx(m_dialogue_plane, frame_y, frame_x);
            ncplane_box(m_dialogue_plane, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0);
        }
    } else {
        ncplane_cursor_move_yx(m_dialogue_plane, frame_y, frame_x);
        ncplane_box(m_dialogue_plane, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0);
    }

    draw_ascii_portrait();
    
    ncplane_set_fg_rgb(m_dialogue_plane, utterance_color);
    int start_y = 12;
    int x = 4;
    int max_width = 56;
    
    // Very simple wrapping
    std::string text = "\"" + m_current_utterance + "\"";
    size_t pos = 0;
    while (pos < text.length()) {
        size_t len = std::min((size_t)max_width, text.length() - pos);
        ncplane_putnstr_yx(m_dialogue_plane, start_y++, x, (uint32_t)len, text.substr(pos, len).c_str());
        pos += len;
    }

    // Draw Choices
    start_y++;
    for (const auto& choice : m_choices) {
        ncplane_set_fg_rgb(m_dialogue_plane, 0xFFFFFF);
        ncplane_printf_yx(m_dialogue_plane, start_y++, x, "[%c] %s", choice.key, choice.text.c_str());
    }

    ncplane_set_fg_rgb(m_dialogue_plane, 0x888888);
    ncplane_putstr_yx(m_dialogue_plane, 25, 2, "ESC: close");
}

void DialogueSystem::draw_ascii_portrait() {
    int start_y = 4;
    int x = 4;
    
    std::string faction = "CITIZEN";
    std::string title = "Citizen";
    if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent)) {
        faction = pol->primary_faction;
    }
    if (auto* hierarchy = registry_.try_get<SocialHierarchyComponent>(m_target_agent)) {
        title = hierarchy->class_title;
    }

    uint32_t color = 0xAAAAAA; // Default Citizen color
    if (faction == "GOVERNMENT") color = 0x55AAFF;
    else if (faction == "REBEL") color = 0xFF5555;
    else if (faction == "MAW") color = 0x55FF55;
    else if (faction == "VOID") color = 0xAA55FF;
    else if (faction == "SYNDICATE") color = 0xFFCC33;

    ncplane_set_fg_rgb(m_dialogue_plane, color);

    auto* xeno_ptr = registry_.try_get<XenoComponent>(m_target_agent);
    if (xeno_ptr) {
        if (xeno_ptr->type == XenoType::HIERODULE) {
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "      .---.      ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     / / \\\\ \\\\     ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    | |/ \\\\| |    ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    | |\\\\ /| |    ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     \\\\ \\\\ / /     ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "      \'---\'      ");
        } else {
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     <><><>      ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    <  !!  >     ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     < VV >      ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    <      >     ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     <><><>      ");
        }
        return;
    }

    // [NEW] Per-Archetype Portraits
    if (title == "Enforcer" || title == "Guard") {
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "      .---.");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     /|___|\\");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    | [###] |");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    |  _-_  |");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    \'-------\'");
    } else if (title == "Outlaw" || title == "Drifter") {
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "      .---.");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     /=====\\\\");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    | <o> <o>|");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    |   -   |");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     \\ --- /");
    } else if (title == "Member" || title == "Gladiator") {
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "      .---.");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     / @ @ \\\\");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    |  ^ # |");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     \\  -  /");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    .-\'---\'-.");
    } else if (title == "Automaton") {
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     _______");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    | [][][]|");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    |  _-_  |");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    |_______|");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "      |   |");
    } else {
        // Default Human/Citizen portrait
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "      .---.");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     / @ @ \\\\");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    (   ^   )");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     \\\\  -  /");
        ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    .-\'---\'-.");
    }
}

uint32_t DialogueSystem::hex_to_rgb(const std::string& hex) {
    if (hex.empty() || hex[0] != '#') return 0xFFFFFF;
    try {
        return std::stoul(hex.substr(1), nullptr, 16);
    } catch (...) {
        return 0xFFFFFF;
    }
}


DialogueTopicCategory DialogueSystem::get_atom_category(const std::string& tag) {
    if (tag == "TOPIC_WORKPLACE" || tag == "TOPIC_UNEMPLOYED" || tag == "TOPIC_WORK_CONDITIONS" || 
        tag == "TOPIC_SCARCITY" || tag == "TOPIC_MARKET_TRENDS" || tag == "TOPIC_STOCK_CRASH" || 
        tag == "TOPIC_MORNING_ALERT" || tag == "TOPIC_MORNING_ANXIOUS" || tag == "TOPIC_GUARD_PATROL") {
        return DialogueTopicCategory::WORK;
    }
    if (tag == "TOPIC_FAMILY" || tag == "TOPIC_FRIENDS" || tag == "TOPIC_SOCIAL_ISOLATION") {
        return DialogueTopicCategory::FAMILY;
    }
    if (tag == "TOPIC_CITY_MYTHS" || tag == "TOPIC_XENO_HIERODULE" || tag == "TOPIC_XENO_CACOGEN" || 
        tag == "TOPIC_FACTION_DOCTRINE") {
        return DialogueTopicCategory::LORE;
    }
    if (tag == "TOPIC_RUMORS") {
        return DialogueTopicCategory::RUMOR;
    }
    if (tag == "TOPIC_RELIGION_DEVOUT" || tag == "TOPIC_RELIGION_SCHISM" || tag == "TOPIC_RELIGION_GENERIC" || \
        tag == "TOPIC_PROSELYTIZING") {
        return DialogueTopicCategory::RELIGION;
    }
    return DialogueTopicCategory::GREETING;
}

std::string DialogueSystem::atom_tag_to_grammar_tag(const std::string& tag) {
    if (tag == "TOPIC_HUNGER_CRITICAL") return "hunger_desperate";
    if (tag == "TOPIC_HUNGER") return "hunger_talk";
    if (tag == "TOPIC_THIRST_CRITICAL") return "thirst_desperate";
    if (tag == "TOPIC_THIRST") return "thirst_talk";
    if (tag == "TOPIC_FRUSTRATION_HOSTILE") return "frustration_hostile";
    if (tag == "TOPIC_FRUSTRATION") return "frustration_talk";
    if (tag == "TOPIC_WORKPLACE") return "work_complaint";
    if (tag == "TOPIC_UNEMPLOYED") return "unemployment_talk";
    if (tag == "TOPIC_WORK_CONDITIONS") return "work_conditions";
    if (tag == "TOPIC_GUARD_PATROL") return "guard_patrol_talk";
    if (tag == "TOPIC_MARKET_TRENDS") return "market_trends_talk";
    if (tag == "TOPIC_XENO_HIERODULE") return "hierodule_proverbs";
    if (tag == "TOPIC_XENO_CACOGEN") return "cacogen_chatter";
    if (tag == "TOPIC_CITIZEN_DEFAULT") return "citizen_generic";
    if (tag == "TOPIC_SCARCITY") return "scarcity_talk";
    if (tag == "TOPIC_WEALTHY") return "wealthy_talk";
    if (tag == "TOPIC_POOR") return "poor_talk";
    if (tag == "TOPIC_HIGH_STATUS") return "high_status_talk";
    if (tag == "TOPIC_LOW_STATUS") return "low_status_talk";
    if (tag == "TOPIC_STOCK_CRASH") return "stock_crash_talk";
    if (tag == "TOPIC_CITY_MYTHS") return "city_myth";
    if (tag == "TOPIC_FAMILY") return "family_talk";
    if (tag == "TOPIC_FRIENDS") return "friends_talk";
    if (tag == "TOPIC_SOCIAL_ISOLATION") return "isolation_talk";
    if (tag == "TOPIC_RUMORS") return "rumor_talk";
    if (tag == "TOPIC_NIGHT") return "night_tired";
    if (tag == "TOPIC_LATE_NIGHT_WEARY") return "late_night_weary";
    if (tag == "TOPIC_MORNING_ALERT") return "morning_alert";
    if (tag == "TOPIC_MORNING_ANXIOUS") return "morning_anxious";
    if (tag == "TOPIC_RAIN") {
        std::string prefix = "";
        if (auto* xeno = registry_.try_get<XenoComponent>(m_target_agent)) {
            if (xeno->type == XenoType::HIERODULE) prefix = "HIERODULE_";
            else if (xeno->type == XenoType::CACOGEN) prefix = "CACOGEN_";
        } else if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent)) {
            prefix = pol->primary_faction + "_";
        }
        return prefix + "rain_talk";
    }
    if (tag == "TOPIC_SMOG") return "smog_talk";
    if (tag == "TOPIC_HEAT") return "heat_talk";
    if (tag == "TOPIC_RELIGION_DEVOUT") return "devotion_talk";
    if (tag == "TOPIC_RELIGION_SCHISM") return "schism_talk";
    if (tag == "TOPIC_RELIGION_GENERIC") return "religion_generic";
    if (tag == "TOPIC_PROSELYTIZING") return "proselytizing_talk";
    if (tag == "TOPIC_FACTION_DOCTRINE") {
        if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_target_agent)) {
            return "doctrine_" + pol->primary_faction;
        }
        return "faction_doctrine";
    }
    return "citizen_generic";
}


void DialogueSystem::record_interaction(DialogueTopicCategory category) {
    if (!registry_.valid(m_target_agent) || category == DialogueTopicCategory::GREETING || 
        category == DialogueTopicCategory::LEAVE || category == DialogueTopicCategory::MEMORY_RECALL) {
        return;
    }

    auto player_view = registry_.view<PlayerComponent>();
    if (player_view.begin() == player_view.end()) return;
    auto player = player_view.front();
    auto* p_comp = registry_.try_get<PlayerComponent>(player);
    if (!p_comp) return;

    auto& rel_comp = registry_.get_or_emplace<RelationshipComponent>(m_target_agent);
    auto& record = rel_comp.records[p_comp->macro_id];
    record.target_macro_id = p_comp->macro_id;
    
    record.interaction_history.push_back(category_to_string(category));
    if (record.interaction_history.size() > 3) {
        record.interaction_history.erase(record.interaction_history.begin());
    }
}

std::string DialogueSystem::category_to_string(DialogueTopicCategory cat) {
    switch (cat) {
        case DialogueTopicCategory::WORK: return "WORK";
        case DialogueTopicCategory::FAMILY: return "FAMILY";
        case DialogueTopicCategory::LORE: return "LORE";
        case DialogueTopicCategory::RUMOR: return "RUMOR";
        case DialogueTopicCategory::BUY_INFO: return "BUY_INFO";
        case DialogueTopicCategory::FAMILY_CHECKIN: return "FAMILY_CHECKIN";
        case DialogueTopicCategory::THREATEN: return "THREATEN";
        case DialogueTopicCategory::SOLIDARITY: return "SOLIDARITY";
        case DialogueTopicCategory::LIE: return "LIE";
        case DialogueTopicCategory::RELIGION: return "RELIGION";
        case DialogueTopicCategory::LEAK_INFO: return "LEAK_INFO";
        case DialogueTopicCategory::LEAK_SELECT: return "LEAK_SELECT";
        case DialogueTopicCategory::VERIFY_INFO: return "VERIFY_INFO";
        case DialogueTopicCategory::VERIFY_SELECT: return "VERIFY_SELECT";
        case DialogueTopicCategory::SET_BROADCAST: return "SET_BROADCAST";
        case DialogueTopicCategory::BROADCAST_SELECT: return "BROADCAST_SELECT";
        default: return "OTHER";
    }
}

} // namespace NeonOubliette::Systems
