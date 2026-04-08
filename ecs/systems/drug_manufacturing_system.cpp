#include "drug_manufacturing_system.h"
#include <iostream>

namespace NeonOubliette {

DrugManufacturingSystem::DrugManufacturingSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    dispatcher.sink<RawMaterialDeliveryEvent>().connect<&DrugManufacturingSystem::handleRawMaterialDelivery>(*this);
    dispatcher.sink<RaidEvent>().connect<&DrugManufacturingSystem::handleRaidEvent>(*this);
    dispatcher.sink<TurnEvent>().connect<&DrugManufacturingSystem::handleTurnEvent>(*this);
}

void DrugManufacturingSystem::initialize() {}

void DrugManufacturingSystem::update(double delta_time) {
    (void)delta_time;
    // Ticking is handled by handleTurnEvent
}

void DrugManufacturingSystem::handleRawMaterialDelivery(const RawMaterialDeliveryEvent& event) {
    // Check if the receiver is a building with a lab
    if (m_registry.all_of<ClandestineLabComponent>(event.receiver)) {
        auto& lab = m_registry.get<ClandestineLabComponent>(event.receiver);
        // Only accept if resource_type 101 (example for chemicals)
        if (event.resource_type == 101) {
            lab.raw_chemicals_stored = std::min(lab.max_raw_chemicals, lab.raw_chemicals_stored + static_cast<int>(event.amount));
            m_dispatcher.enqueue<HUDNotificationEvent>({"Raw chemicals delivered to lab.", 3.0f, "#00FF00"});
        }
    }
}

void DrugManufacturingSystem::handleRaidEvent(const RaidEvent& event) {
    // Check if the target building has a lab
    if (m_registry.all_of<ClandestineLabComponent>(event.target_building)) {
        auto& lab = m_registry.get<ClandestineLabComponent>(event.target_building);
        lab.is_raided = true;
        lab.raw_chemicals_stored = 0;
        lab.drugs_produced_stored = 0;
        lab.production_progress = 0.0f;
        
        m_dispatcher.enqueue<HUDNotificationEvent>({"A clandestine lab was raided!", 5.0f, "#FF0000"});
        m_dispatcher.enqueue<MilestoneEvent>({"CRIME_LAB_RAIDED", "A clandestine lab was shut down in a raid.", "", "", event.instigator_faction, 2.0f});
    }
}

void DrugManufacturingSystem::handleTurnEvent(const TurnEvent& event) {
    // Process every turn or every N turns
    if (event.turn_number % 5 == 0) {
        processLabTick();
    }
}

void DrugManufacturingSystem::processLabTick() {
    auto lab_view = m_registry.view<ClandestineLabComponent, BuildingComponent>();

    for (auto entity : lab_view) {
        auto& lab = lab_view.get<ClandestineLabComponent>(entity);
        
        if (lab.is_raided) continue;

        // If we have inputs and room for output, proceed
        if (lab.raw_chemicals_stored > 0 && lab.drugs_produced_stored < lab.max_drugs_produced) {
            lab.production_progress += lab.production_rate * 5.0f; // Scale by 5 turns since we tick every 5

            if (lab.production_progress >= 1.0f) {
                lab.raw_chemicals_stored--;
                lab.drugs_produced_stored++;
                lab.production_progress = 0.0f;

                // Create information that a lab is active
                InformationRecord rumor;
                rumor.type = InformationType::RUMOR;
                rumor.content_tag = "LAB_ACTIVE_" + std::to_string(static_cast<uint32_t>(entity));
                rumor.veracity = 0.8f;
                rumor.origin_tick = 0; // Current tick from simulation context would be better

                m_dispatcher.enqueue<InformationCreatedEvent>({rumor});
            }
        }
    }
}

} // namespace NeonOubliette
