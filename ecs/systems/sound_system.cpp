#include "sound_system.h"
#include <queue>
#include <map>
#include <algorithm>

namespace NeonOubliette::Systems {

SoundSystem::SoundSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {}

void SoundSystem::initialize() {
    dispatcher_.sink<SpeechEvent>().connect<&SoundSystem::handleSpeechEvent>(this);
}

void SoundSystem::update(double delta_time) {
    (void)delta_time;
    // Tick down speech components
    auto view = registry_.view<SpeechComponent>();
    std::vector<entt::entity> to_remove;
    for (auto entity : view) {
        auto& speech = view.get<SpeechComponent>(entity);
        if (speech.ticks_remaining > 0) {
            speech.ticks_remaining--;
            // [B.5] Sound attenuation data recalculated lazily when room doors open/close; cached per room pair until topology changes.
            // Simplified: Recalculate each tick for currently active speech.
            propagateSound(entity);
        } else {
            to_remove.push_back(entity);
        }
    }
    for (auto entity : to_remove) {
        registry_.remove<SpeechComponent>(entity);
    }
}

void SoundSystem::handleSpeechEvent(const SpeechEvent& event) {
    auto& speech = registry_.get_or_emplace<SpeechComponent>(event.speaker);
    speech.text = event.text;
    speech.ticks_remaining = event.duration_ticks;
    speech.speaker = event.speaker;
    speech.audibility = AudibilityLevel::CLEAR;

    propagateSound(event.speaker);
}

void SoundSystem::propagateSound(entt::entity speaker) {
    if (!registry_.all_of<PositionComponent>(speaker)) return;
    const auto& s_pos = registry_.get<PositionComponent>(speaker);

    // Get the player to determine relative audibility
    entt::entity player = entt::null;
    auto player_view = registry_.view<PlayerComponent, PositionComponent>();
    if (player_view.begin() != player_view.end()) player = *player_view.begin();
    if (player == entt::null) return;
    const auto& p_pos = registry_.get<PositionComponent>(player);

    auto& speech = registry_.get<SpeechComponent>(speaker);

    // Same layer
    if (s_pos.layer_id == p_pos.layer_id) {
        // Overworld (L0)
        if (s_pos.layer_id == 0) {
            int dist = std::abs(s_pos.x - p_pos.x) + std::abs(s_pos.y - p_pos.y);
            if (dist <= 5) speech.audibility = AudibilityLevel::CLEAR;
            else if (dist <= 10) speech.audibility = AudibilityLevel::MUFFLED;
            else speech.audibility = AudibilityLevel::INAUDIBLE;
            return;
        }

        // Interior (L >= 1000)
        entt::entity building = entt::null;
        int speaker_room = -1;
        if (registry_.all_of<InteriorStateComponent>(speaker)) {
            const auto& speaker_s = registry_.get<InteriorStateComponent>(speaker);
            building = speaker_s.building_entity;
            speaker_room = speaker_s.current_room_index;
        }

        if (building != entt::null && registry_.all_of<BuildingAcousticsComponent>(building)) {
            const auto& acoustics = registry_.get<BuildingAcousticsComponent>(building);
            
            int player_room = -1;
            if (registry_.all_of<InteriorStateComponent>(player)) {
                const auto& player_s = registry_.get<InteriorStateComponent>(player);
                if (player_s.building_entity == building) {
                    player_room = player_s.current_room_index;
                }
            }

            if (speaker_room >= 0 && player_room >= 0) {
                // BFS to find room distance
                std::queue<std::pair<size_t, int>> q;
                q.push({(size_t)speaker_room, 0});
                std::map<size_t, int> visited;
                visited[(size_t)speaker_room] = 0;

                int min_walls = 999;
                while (!q.empty()) {
                    auto [curr, walls] = q.front(); q.pop();
                    if (curr == (size_t)player_room) {
                        min_walls = walls;
                        break;
                    }
                    if (walls >= 3) continue;

                    for (const auto& edge : acoustics.nodes[curr].neighbors) {
                        int edge_weight = 2; // solid wall
                        if (edge.door_entity != entt::null) {
                            if (registry_.all_of<DoorComponent>(edge.door_entity)) {
                                const auto& door = registry_.get<DoorComponent>(edge.door_entity);
                                edge_weight = door.is_open ? 0 : 1;
                            }
                        }
                        
                        int next_walls = walls + edge_weight;
                        if (visited.find(edge.target_room_index) == visited.end() || visited[edge.target_room_index] > next_walls) {
                            visited[edge.target_room_index] = next_walls;
                            q.push({edge.target_room_index, next_walls});
                        }
                    }
                }

                if (min_walls == 0) speech.audibility = AudibilityLevel::CLEAR;
                else if (min_walls <= 2) speech.audibility = AudibilityLevel::MUFFLED;
                else speech.audibility = AudibilityLevel::INAUDIBLE;
            } else {
                speech.audibility = AudibilityLevel::INAUDIBLE;
            }
        }
    } else {
        // Different layers - Check window rule (Inside to Outside)
        if (p_pos.layer_id == 0 && s_pos.layer_id >= 1000) {
             entt::entity building = entt::null;
             int speaker_room = -1;
             if (registry_.all_of<InteriorStateComponent>(speaker)) {
                 const auto& speaker_s = registry_.get<InteriorStateComponent>(speaker);
                 building = speaker_s.building_entity;
                 speaker_room = speaker_s.current_room_index;
             }
             if (building != entt::null && registry_.all_of<BuildingAcousticsComponent>(building)) {
                 const auto& acoustics = registry_.get<BuildingAcousticsComponent>(building);
                 if (speaker_room >= 0 && acoustics.nodes[speaker_room].has_window) {
                      const auto& b_pos = registry_.get<PositionComponent>(building);
                      const auto& b_size = registry_.get<SizeComponent>(building);
                      int dist_x = std::max(0, std::max(b_pos.x - p_pos.x, p_pos.x - (b_pos.x + b_size.width - 1)));
                      int dist_y = std::max(0, std::max(b_pos.y - p_pos.y, p_pos.y - (b_pos.y + b_size.height - 1)));
                      if (dist_x + dist_y <= 2) speech.audibility = AudibilityLevel::MUFFLED;
                      else speech.audibility = AudibilityLevel::INAUDIBLE;
                 } else {
                      speech.audibility = AudibilityLevel::INAUDIBLE;
                 }
             }
        } else {
            speech.audibility = AudibilityLevel::INAUDIBLE;
        }
    }
}

} // namespace NeonOubliette::Systems
