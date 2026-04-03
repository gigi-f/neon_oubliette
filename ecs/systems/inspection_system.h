#ifndef NEON_OUBLIETTE_INSPECTION_SYSTEM_H
#define NEON_OUBLIETTE_INSPECTION_SYSTEM_H

#include <notcurses/notcurses.h>
#include <entt/entt.hpp>
#include "../../ecs/component_declarations.h"
#include "../../ecs/events.h"
#include "../../ecs/components/simulation_layers.h"
#include "system_scheduler.h"
#include <string>
#include <vector>

namespace NeonOubliette {
namespace Systems {

/**
 * @brief Represents a cross-layer simulation insight.
 */
struct LayerInsight {
    std::string message;
    std::string hex_color;
};

/**
 * @brief Manages the inspection interface, providing detailed simulation data 
 * through five distinct visual lenses (Physical, Bio, Cognitive, Economic, Political).
 */
class InspectionSystem : public ISystem {
public:
    InspectionSystem(entt::registry& registry, struct notcurses* nc_context, entt::dispatcher& event_dispatcher);
    ~InspectionSystem();

    void initialize() override;
    void update(double delta_time) override;

private:
    entt::registry& registry_;
    struct notcurses* nc_context_;
    entt::dispatcher& event_dispatcher_;
    
    struct ncplane* m_inspection_plane = nullptr;
    bool m_window_visible = false;
    entt::entity m_current_target = entt::null;
    InspectionMode m_current_mode = InspectionMode::GLANCE;
    
    uint64_t m_pulse_counter = 0;

    void handleInspectEvent(const InspectEvent& event);
    void handleCloseInspectionWindowEvent(const CloseInspectionWindowEvent& event);
    void draw_inspection_window();
    void create_inspection_plane();
    void close_inspection_window();
    
    // UI Helpers
    void draw_tabs();
    void draw_content();
    void draw_ascii_portrait();
    
    // Cross-Layer Logic
    std::vector<LayerInsight> calculate_insights(entt::entity target, SimulationLayer current_view);
    
    // String Converters
    std::string get_material_name(MaterialType type);
    std::string get_species_name(SpeciesType type);
    uint32_t hex_to_rgb(const std::string& hex);
};

} // namespace Systems
} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_INSPECTION_SYSTEM_H
