#ifndef NEON_OUBLIETTE_ECS_EVENT_DECLARATIONS_H
#define NEON_OUBLIETTE_ECS_EVENT_DECLARATIONS_H

#include <chrono>
#include <entt/entt.hpp>
#include <string>
#include <vector>

#include "components/components.h"

namespace NeonOubliette {

using MicroEventBus = entt::dispatcher;
using MacroEventBus = entt::dispatcher;

// --- Enums for Events ---
enum class InspectionMode {
    GLANCE,
    SURFACE_SCAN,
    BIOLOGICAL_AUDIT,
    COGNITIVE_PROFILE,
    FINANCIAL_FORENSICS,
    STRUCTURAL_ANALYSIS,
    FORENSIC,
    SURVEILLANCE
};

enum class BarterState : uint32_t {
    REQUEST,
    ACCEPT,
    REJECT
};

enum class ContainerInteractionType {
    OPEN,
    CLOSE,
    TOGGLE
};

// --- Core Simulation Events ---

struct MoveEvent {
    entt::entity entity;
    int dx = 0;
    int dy = 0;
    int layer_id = 0;
};

struct PlayerMoveEvent {
    int dx = 0;
    int dy = 0;
};

struct BuildingEntranceEvent {
    entt::entity visitor;
    entt::entity building;
    int entry_x = 0;
    int entry_y = 0;
    int entry_layer = 0;
};

struct InteriorActivatedEvent {
    entt::entity building_entity;
    int base_layer_id;
};

struct PickupItemEvent {
    entt::entity picker_entity;
    entt::entity item_entity;
    int x = 0;
    int y = 0;
    int layer_id = 0;
};

struct HUDNotificationEvent {
    std::string message;
    float duration = 2.0f;
    std::string color_hex = "#FFFFFF";
};

// --- System Events ---

struct TurnEvent {
    uint64_t turn_number;
};

struct AdvanceTurnRequestEvent {};

struct LogEvent {
    std::string message;
    enum class LogSeverity { DEBUG, INFO, WARNING, ERROR, CRITICAL } severity;
    std::string source_system;
};
using LogSeverity = LogEvent::LogSeverity;

struct ShutdownEvent {};
struct SaveGameEvent {};
struct LoadGameEvent {};

// --- Interaction & UI Events ---

struct InventoryToggleEvent {
    entt::entity entity;
};

struct CloseInspectionWindowEvent {};

struct ToggleControlsHelpEvent {
    entt::entity entity;
};

struct InspectEvent {
    entt::entity player_entity;
    int layer_id;
    int x;
    int y;
    InspectionMode mode;
};

struct InteractEvent {
    entt::entity entity;
};

struct UseItemEvent {
    entt::entity user_entity;
    entt::entity item_in_inventory_entity;
};

struct ConsumeItemEvent {
    entt::entity consumer_entity;
    entt::entity item_to_consume_entity;
};

struct CraftItemEvent {
    entt::entity crafter;
    std::string recipe_id;
};

struct ContainerInteractionEvent {
    entt::entity interaction_entity;
    entt::entity container_entity;
    ContainerInteractionType interaction_type = ContainerInteractionType::TOGGLE;
};

// --- Pathfinding Events ---

struct PathfindingRequestEvent {
    entt::entity entity;
    PositionComponent start;
    PositionComponent goal;
    uint32_t request_id;
};

struct PathfindingResponseEvent {
    entt::entity entity;
    std::vector<PositionComponent> path;
    uint32_t request_id;
    bool success;
};

// --- Activity Events ---

struct StartActivityEvent {
    entt::entity actor_entity;
    ActivityType type;
    int total_turns_required;
    entt::entity target_entity = entt::null;
    entt::entity secondary_entity = entt::null;
    std::string description;
};

struct ActivityProgressEvent {
    entt::entity actor_entity;
    ActivityType type;
    int turns_remaining;
    int total_turns_required;
};

struct ActivityCompletedEvent {
    entt::entity actor_entity;
    ActivityType type;
    entt::entity target_entity = entt::null;
    entt::entity secondary_entity = entt::null;
};

struct ActivityInterruptedEvent {
    entt::entity actor_entity;
    ActivityType type;
};

// --- Faction Events ---

struct ChangeFactionStandingEvent {
    std::string acting_faction_id;
    std::string target_faction_id;
    int standing_change;
};

// --- Barter Events ---

struct BarterEvent {
    entt::entity initiator_entity;
    entt::entity target_entity;
    std::vector<entt::entity> offered_items;
    std::vector<entt::entity> requested_items;
    BarterState state;
};

// --- Infrastructure & Environmental Events ---

struct InfrastructureDegradationEvent {
    entt::entity infrastructure_entity_id;
    float current_condition_percentage;
    float degradation_amount;
    uint64_t timestamp;
};

struct InfrastructureBreakdownEvent {
    entt::entity infrastructure_entity_id;
    std::string problem_description;
    uint64_t timestamp;
};

struct InfrastructureRepairedEvent {
    entt::entity infrastructure_entity_id;
};

struct DegradationEvent {
    entt::entity entity;
    float amount;
};

struct WaterMainBreakEvent {
    entt::entity location;
};

struct PowerOutageEvent {
    entt::entity location;
};

struct WorkOrderEvent {
    entt::entity target;
    std::string description;
};

// --- Economic & Labor Events ---

struct PurchaseEvent {
    entt::entity buyer_npc_id;
    entt::entity shop_entity_id;
    std::string item_name;
    uint32_t quantity;
    uint64_t price_paid;
};

struct CommerceEvent {
    std::string type;
    uint64_t amount;
    entt::entity building_id;
    uint32_t resource;
};

struct RawMaterialDeliveryEvent {
    entt::entity supplier;
    entt::entity receiver;
    uint32_t resource_type;
    uint64_t amount;
};

struct JobOpeningEvent {
    entt::entity employer;
    std::string role;
};

struct LayoffEvent {
    entt::entity employer;
    entt::entity employee;
};

// --- Traffic Events ---

struct CongestionEvent {
    entt::entity road_segment;
    float density;
    uint64_t timestamp = 0;
};

// --- Political & Social Events ---

struct CampaignEvent {
    entt::entity candidate_id;
    std::string type;
    entt::entity target_faction_id;
    std::string message;
};

struct PolicyChangeEvent {
    std::string policy_name;
    std::string old_value;
    std::string new_value;
    entt::entity instigator_faction_id;
    uint64_t timestamp;
};

struct BackroomDealEvent {
    entt::entity faction_a;
    entt::entity faction_b;
};

struct ScandalEvent {
    entt::entity target;
    std::string description;
};

struct ElectionEvent {
    std::string jurisdiction;
};

struct LegislationEvent {
    std::string bill_name;
};

struct CorruptionEvent {
    entt::entity perpetrator;
};

struct DiscoveryEvent {
    entt::entity finder;
    std::string subject;
};

struct DiseaseEvent {
    std::string disease_name;
    entt::entity affected_entity;
    entt::entity source_location;
};

struct PlayerLayerChangeEvent {
    int dz = 0;
};

// =====================================================================
// Legacy Namespace Redirection
// =====================================================================
namespace ECS {
    using MicroEventBus = NeonOubliette::MicroEventBus;
    using MacroEventBus = NeonOubliette::MacroEventBus;
    using HUDNotificationEvent = NeonOubliette::HUDNotificationEvent;
    using ShutdownEvent = NeonOubliette::ShutdownEvent;
    using SaveGameEvent = NeonOubliette::SaveGameEvent;
    using LoadGameEvent = NeonOubliette::LoadGameEvent;
    using InventoryToggleEvent = NeonOubliette::InventoryToggleEvent;
    using CloseInspectionWindowEvent = NeonOubliette::CloseInspectionWindowEvent;
    using RawMaterialDeliveryEvent = NeonOubliette::RawMaterialDeliveryEvent;
    using CongestionEvent = NeonOubliette::CongestionEvent;
    using PlayerMoveEvent = NeonOubliette::PlayerMoveEvent;
    using PlayerLayerChangeEvent = NeonOubliette::PlayerLayerChangeEvent;
} // namespace ECS

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_EVENT_DECLARATIONS_H
