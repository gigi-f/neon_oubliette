#ifndef NEON_OUBLIETTE_INSPECTION_SYSTEM_H
#define NEON_OUBLIETTE_INSPECTION_SYSTEM_H

#include <notcurses/notcurses.h>
#include <entt/entt.hpp>
#include "../../ecs/component_declarations.h"
#include "../../ecs/events.h"
#include "system_scheduler.h"
#include <string>

namespace NeonOubliette {
namespace Systems {

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

    void handleInspectEvent(const InspectEvent& event);
    void draw_inspection_window();
    void create_inspection_plane();
    void close_inspection_window();
    
    // UI Helpers
    void draw_tabs();
    void draw_content();
    void draw_ascii_portrait();
};

} // namespace Systems
} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_INSPECTION_SYSTEM_H
