#include "crisis_dashboard_system.h"
#include <algorithm>

namespace NeonOubliette::Systems {

CrisisDashboardSystem::CrisisDashboardSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {}

void CrisisDashboardSystem::initialize() {
    m_dispatcher.sink<ToggleCrisisDashboardEvent>().connect<&CrisisDashboardSystem::handleToggleCrisisDashboard>(this);
    
    // Ensure CrisisDashboardComponent singleton exists
    auto view = m_registry.view<CrisisDashboardComponent>();
    if (view.empty()) {
        auto entity = m_registry.create();
        m_registry.emplace<CrisisDashboardComponent>(entity);
    }
}

void CrisisDashboardSystem::handleToggleCrisisDashboard(const ToggleCrisisDashboardEvent& event) {
    (void)event;
    auto view = m_registry.view<SimulationStateComponent>();
    if (view.empty()) return;
    auto& state = view.get<SimulationStateComponent>(view.front());

    // Only allow dashboard in God Mode
    if (state.mode == SimulationMode::GOD_MODE) {
        auto dash_view = m_registry.view<CrisisDashboardComponent>();
        if (!dash_view.empty()) {
            auto& dash = dash_view.get<CrisisDashboardComponent>(dash_view.front());
            dash.visible = !dash.visible;
        }
    }
}

void CrisisDashboardSystem::update(double delta_time) {
    (void)delta_time;

    auto dash_view = m_registry.view<CrisisDashboardComponent>();
    auto crisis_view = m_registry.view<CrisisComponent>();
    
    if (dash_view.empty() || crisis_view.empty()) return;
    
    auto& dash = dash_view.get<CrisisDashboardComponent>(dash_view.front());
    const auto& crisis = crisis_view.get<CrisisComponent>(crisis_view.front());

    // Always update history to have a rolling graph even if hidden
    update_history(dash, crisis);
    update_propagation_vectors(dash, crisis);
}

void CrisisDashboardSystem::update_history(CrisisDashboardComponent& dash, const CrisisComponent& crisis) {
    auto push_clamped = [](std::vector<float>& vec, float val, size_t limit) {
        vec.push_back(std::clamp(val, 0.0f, 1.0f));
        if (vec.size() > limit) {
            vec.erase(vec.begin());
        }
    };

    float econ_s = 0.0f, pol_s = 0.0f, bio_s = 0.0f, env_s = 0.0f;
    for (const auto& c : crisis.active_crises) {
        if (c.type == CrisisType::ECONOMIC_COLLAPSE) econ_s += c.severity;
        if (c.type == CrisisType::POLITICAL_UNREST || c.type == CrisisType::FACTION_WAR) pol_s += c.severity;
        if (c.type == CrisisType::BIOLOGICAL_OUTBREAK) bio_s += c.severity;
        if (c.type == CrisisType::ENVIRONMENTAL_HAZARD) env_s += c.severity;
    }

    // Baseline minimums so graphs aren't totally empty
    econ_s = std::max(econ_s, 0.05f); 
    pol_s = std::max(pol_s, 0.05f);
    bio_s = std::max(bio_s, evaluate_biological_stress());
    env_s = std::max(env_s, evaluate_environmental_stress());

    push_clamped(dash.economic_stress_history, econ_s, dash.history_limit);
    push_clamped(dash.political_stress_history, pol_s, dash.history_limit);
    push_clamped(dash.biological_stress_history, bio_s, dash.history_limit);
    push_clamped(dash.environmental_stress_history, env_s, dash.history_limit);
}

float CrisisDashboardSystem::evaluate_biological_stress() {
    auto view = m_registry.view<InfectionComponent>();
    if (view.empty()) return 0.0f;
    
    float total_progress = 0.0f;
    int count = 0;
    for (auto entity : view) {
        total_progress += view.get<InfectionComponent>(entity).progress;
        count++;
    }
    return std::min(1.0f, (float)count / 50.0f + (total_progress / 50.0f));
}

float CrisisDashboardSystem::evaluate_environmental_stress() {
    auto view = m_registry.view<EnvironmentalHazardComponent>();
    if (view.empty()) return 0.0f;
    
    float max_tox = 0.0f;
    for (auto entity : view) {
        auto& env = view.get<EnvironmentalHazardComponent>(entity);
        if (env.is_active) {
            max_tox = std::max(max_tox, env.toxicity_level);
        }
    }
    return max_tox;
}

void CrisisDashboardSystem::update_propagation_vectors(CrisisDashboardComponent& dash, const CrisisComponent& crisis) {
    dash.propagation_vectors.clear();
    
    if (crisis.active_crises.empty()) {
        dash.propagation_vectors.push_back("SYSTEM STATUS: NOMINAL");
        return;
    }

    for (const auto& c : crisis.active_crises) {
        switch(c.type) {
            case CrisisType::ECONOMIC_COLLAPSE:
                dash.propagation_vectors.push_back("* Scarcity -> Increased Crime Risk");
                dash.propagation_vectors.push_back("* Low GDP -> Political Instability");
                break;
            case CrisisType::BIOLOGICAL_OUTBREAK:
                dash.propagation_vectors.push_back("* Agent Sickness -> Reduced Labor Output");
                dash.propagation_vectors.push_back("* Public Fear -> Faction Influence Drift");
                break;
            case CrisisType::POLITICAL_UNREST:
                dash.propagation_vectors.push_back("* Civil Unrest -> Logistics Friction");
                dash.propagation_vectors.push_back("* Faction Clash -> Physical Infrastructure Damage");
                break;
            case CrisisType::INFRASTRUCTURE_FAILURE:
                dash.propagation_vectors.push_back("* Power Grid Loss -> Reduced Economic Activity");
                dash.propagation_vectors.push_back("* Darkness -> Increased Criminal Boldness");
                break;
            case CrisisType::ENVIRONMENTAL_HAZARD:
                dash.propagation_vectors.push_back("* Toxic Levels -> Biological Systemic Stress");
                dash.propagation_vectors.push_back("* Corrosive Atmosphere -> Infrastructure Decay");
                break;
            default:
                dash.propagation_vectors.push_back("* Systemic Anomaly -> Unpredictable Side Effects");
                break;
        }
    }
}

} // namespace NeonOubliette::Systems
