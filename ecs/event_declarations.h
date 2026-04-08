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
    HISTORY,
    FORENSIC,
    SURVEILLANCE
};

enum class BarterState : uint32_t {
    REQUEST,
    ACCEPT,
    REJECT,
    COUNTER_OFFER,
    PRESSURE
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
    entt::entity door_entity = entt::null;
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

struct DropItemEvent {
    entt::entity dropper_entity;
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

struct SpeechEvent {
    entt::entity speaker;
    std::string text;
    uint32_t duration_ticks = 20;
    AudibilityLevel audibility = AudibilityLevel::CLEAR;
    bool has_record = false;
    InformationRecord record;
};

// --- System Events ---

struct TurnEvent {
    uint64_t turn_number;
};

struct TimeOfDayChangeEvent {
    TimeOfDay new_time;
};

struct AdvanceTurnRequestEvent {};

struct ToggleGodModeEvent {};
struct TogglePauseEvent {};
struct ToggleCrisisDashboardEvent {};
struct AdjustGodModeSpeedEvent { float delta = 0.5f; };
struct GodModeFocusBuildingEvent { entt::entity building_entity; };
struct GodModeExitFocusEvent {};

struct GodModeFollowAgentEvent { entt::entity target; };
struct GodModeTeleportCursorEvent { int x; int y; int layer; };
struct GodModeTagEntityEvent { entt::entity target; std::string tag_label; };
struct OpenContextMenuEvent { int x; int y; int layer_id; entt::entity target_entity = entt::null; };
struct CloseContextMenuEvent {};
struct ContextMenuSelectEvent { int selection_index; };

struct LogEvent {
    std::string message;
    enum class LogSeverity { DEBUG, INFO, WARNING, ERROR, CRITICAL } severity;
    std::string source_system;
};
using LogSeverity = LogEvent::LogSeverity;

struct ShutdownEvent {};
struct SaveGameEvent {};
struct LoadGameEvent {};

// --- Chunk/World Change Events ---
struct ChunkChangedEvent {};  // Fired when chunks load/unload, invalidates spatial caches

// --- Interaction & UI Events ---

struct InventoryToggleEvent {
    entt::entity entity;
};

struct CloseInspectionWindowEvent {};

struct CloseDialogueWindowEvent {};

struct OpenBarterEvent {
    entt::entity initiator;
    entt::entity target;
};

struct CloseBarterEvent {};

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

struct DialogueEvent {
    entt::entity player_entity;
    entt::entity target_agent;
};

struct DialogueChoiceEvent {
    int choice_index;
};

struct InteractEvent {
    entt::entity entity;
    int layer_id = 0;
    int x = 0;
    int y = 0;
};

struct UseItemEvent {
    entt::entity user_entity;
    entt::entity item_in_inventory_entity;
};

struct AttackEvent {
    entt::entity attacker_entity;
    entt::entity target_entity;
};

struct StealAttemptEvent {
    entt::entity thief_entity;
    entt::entity target_entity;
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
    std::vector<PositionComponent> macro_path; // NEW: The sequence of high-level nodes
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
    float standing_change;
};

struct AgentFactionReputationEvent {
    entt::entity agent_entity;
    std::string faction_id;
    float change_amount;
};

struct AgentDeathEvent {
    entt::entity entity = entt::null;
    uint64_t macro_id = 0;
    
    // Macro data (if macro_id > 0)
    int credits = 0;
    std::string faction_id = "CITIZEN";
    std::map<std::string, uint64_t> portfolio;
    std::unordered_map<uint64_t, RelationshipRecord> relationships;
    std::vector<entt::entity> items;
};

/**
 * @brief [J.2] Fired when a new agent is born into the simulation.
 */
struct BirthEvent {
    entt::entity child;
    entt::entity parent_a;
    entt::entity parent_b = entt::null;
    int x, y, layer;
};

// --- Crime & Security Events ---
struct CrimeReportEvent {
    entt::entity perpetrator;
    entt::entity victim;
    int x = 0;
    int y = 0;
    int layer = 0;
    std::string crime_type = "UNKNOWN";
    uint64_t tick = 0;
};

struct WantedAlertEvent {
    entt::entity target;
    std::string reporting_faction_id;
    int wanted_level;
};

// --- Barter Events ---

struct BarterEvent {
    entt::entity initiator_entity;
    entt::entity target_entity;
    std::vector<entt::entity> offered_items;
    std::vector<entt::entity> requested_items;
    std::vector<InformationRecord> offered_info;
    std::vector<InformationRecord> requested_info;
    int credit_offered = 0;
    int credit_requested = 0;
    BarterState state;
};

// --- Infrastructure & Environmental Events ---

/**
 * @brief [K.3] Fired when a building is slated for removal due to decay or renewal.
 */
struct DemolitionEvent {
    entt::entity building_entity;
};

/**
 * @brief [K.3] Fired when a vacant lot is ready for new construction.
 */
struct RebuildEvent {
    entt::entity lot_entity;
};

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

struct StockPurchaseEvent {
    entt::entity agent;
    std::string ticker;
    uint64_t shares;
    double price_per_share;
};

struct StockSaleEvent {
    entt::entity agent;
    std::string ticker;
    uint64_t shares;
    double price_per_share;
};

struct CommerceEvent {
    std::string type;
    uint64_t amount;
    entt::entity building_id;
    uint32_t resource;
};

struct InformationPropagationEvent {
    entt::entity source;
    entt::entity target;
    InformationRecord record;
};

struct InformationCreatedEvent {
    InformationRecord record;
};

struct RawMaterialDeliveryEvent {
    entt::entity supplier;
    entt::entity receiver;
    uint32_t resource_type;
    uint64_t amount;
};

/**
 * @brief [O.1] Fired when an agent successfully extracts resources from a node.
 */
struct ResourceExtractedEvent {
    entt::entity actor;
    entt::entity node;
    RawMaterialType material_type;
    float amount;
    int x, y, layer;
};

/**
 * @brief [O.2] Fired when a factory completes a production cycle.
 */
struct ProductionCompletedEvent {
    entt::entity factory_entity = entt::null;
    uint32_t output_item_type_id = 0;
    std::string item_name;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(factory_entity), CEREAL_NVP(output_item_type_id), CEREAL_NVP(item_name));
    }
};

/**
 * @brief [O.3] Fired when a factory's supply chain is significantly disrupted.
 */
struct SupplyChainDisruptedEvent {
    entt::entity factory_entity;
    std::string reason; // "LOGISTICS", "STRIKE", "SABOTAGE"
    float intensity;    // 0.0 to 1.0
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

struct MilestoneEvent {
    std::string type;
    std::string description;
    std::string faction_a_id = "";
    std::string faction_b_id = "";
    entt::entity actor = entt::null;
    float importance = 1.0f;
};

// --- Religion Events ---

struct WorshipEvent {
    entt::entity worshipper;
    std::string religion_id;
    entt::entity place_of_worship;
    uint64_t tick;
};

struct HolyDayEvent {
    std::string religion_id;
    uint32_t holy_day_id;
    std::string description;
};

struct ReligiousTensionEvent {
    std::string religion_a_id;
    std::string religion_b_id;
    entt::entity location;
    float tension_increase;
};

struct RaidEvent {
    entt::entity instigator_faction;
    std::string target_religion_id;
    entt::entity target_building;
    int x = 0;
    int y = 0;
};

struct ProselytizingEvent {
    entt::entity initiator;
    entt::entity target;
    std::string religion_id;
    bool success;
};

// --- Crisis Events [L.1] ---

struct CrisisStartedEvent {
    CrisisType type;
    float severity;
    std::string description;
};

struct CrisisResolvedEvent {
    CrisisType type;
};

struct CrisisEffectEvent {
    CrisisType type;
    float intensity; // current pulse strength
};

/**
 * @brief [NEW CLASS] Event fired when a broadcast pulse occurs.
 */
struct BroadcastPulseEvent {
    entt::entity tower_entity;
    int x, y, layer;
    int radius;
    entt::entity faction_entity;
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
