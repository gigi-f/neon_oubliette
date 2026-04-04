#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_SIMULATION_LAYERS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_SIMULATION_LAYERS_H

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <cereal/types/map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

namespace NeonOubliette {

enum class SimulationLayer : uint8_t {
    L0_Physics = 0,
    L1_Biology = 1,
    L2_Cognitive = 2,
    L3_Economic = 3,
    L4_Political = 4,
    Count
};

enum class ConcealmentLevel : uint8_t {
    NONE,
    CASUAL,
    ACTIVE,
    MILITARY
};

// =====================================================================
// Privacy and Inspection
// =====================================================================

struct PrivacyMask {
    ConcealmentLevel concealment_level = ConcealmentLevel::NONE;
    int encryption_strength = 0;
    std::map<std::string, std::string> falsified_data;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("concealment_level", concealment_level),
           cereal::make_nvp("encryption_strength", encryption_strength),
           cereal::make_nvp("falsified_data", falsified_data));
    }
};

struct EntityPrivacyComponent {
    std::map<SimulationLayer, PrivacyMask> masks;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("masks", masks));
    }
};

// =====================================================================
// Layer 0: Physics & Topology
// =====================================================================

enum class MaterialType : uint8_t {
    CONCRETE,
    FLESH,
    STEEL,
    PLASTIC,
    GLASS,
    ELECTRONICS,
    WATER
};

struct Layer0PhysicsComponent {
    MaterialType material = MaterialType::CONCRETE;
    float structural_integrity = 1.0f; // 0.0 to 1.0
    float temperature_celsius = 20.0f;
    bool is_liquid = false;
    bool is_gas = false;
    bool is_plasma = false;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("material", material),
           cereal::make_nvp("structural_integrity", structural_integrity),
           cereal::make_nvp("temperature_celsius", temperature_celsius),
           cereal::make_nvp("is_liquid", is_liquid),
           cereal::make_nvp("is_gas", is_gas),
           cereal::make_nvp("is_plasma", is_plasma));
    }
};

// =====================================================================
// Layer 1: Biological Systems
// =====================================================================

enum class SpeciesType : uint8_t {
    HUMAN,
    RAT,
    SYNTHETIC,
    CANINE,
    FELINE,
    CACOGEN,    // [NEW] Off-worlder/Monster
    HIERODULE   // [NEW] Servant of the "New Sun"
};

enum class OrganType : uint8_t {
    HEART,
    LUNGS,
    LIVER,
    KIDNEYS,
    BRAIN,
    EYES,
    LIMBS,
    NEURAL_LINK
};

struct OrganState {
    float health = 1.0f;
    bool is_cybernetic = false;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("health", health),
           cereal::make_nvp("is_cybernetic", is_cybernetic));
    }
};

struct VitalSigns {
    float heart_rate = 70.0f;       // BPM
    float blood_pressure_sys = 120.0f;
    float blood_pressure_dia = 80.0f;
    float oxygen_saturation = 98.0f; // Percentage
    float core_temperature = 37.0f;  // Celsius

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("heart_rate", heart_rate),
           cereal::make_nvp("blood_pressure_sys", blood_pressure_sys),
           cereal::make_nvp("blood_pressure_dia", blood_pressure_dia),
           cereal::make_nvp("oxygen_saturation", oxygen_saturation),
           cereal::make_nvp("core_temperature", core_temperature));
    }
};

struct Layer1BiologyComponent {
    SpeciesType species = SpeciesType::HUMAN;
    float consciousness_level = 1.0f; // 0.0 to 1.0
    int pain_level = 0; // 0 to 10
    float metabolic_rate = 1.0f;
    
    VitalSigns vitals;
    std::map<OrganType, OrganState> organs;
    std::map<std::string, float> blood_chemistry; // Compound ID -> Concentration

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("species", species),
           cereal::make_nvp("consciousness_level", consciousness_level),
           cereal::make_nvp("pain_level", pain_level),
           cereal::make_nvp("metabolic_rate", metabolic_rate),
           cereal::make_nvp("vitals", vitals),
           cereal::make_nvp("organs", organs),
           cereal::make_nvp("blood_chemistry", blood_chemistry));
    }
};

// =====================================================================
// Layer 2: Cognitive/Social
// =====================================================================

struct Layer2CognitiveComponent {
    float pleasure = 0.0f; // -1.0 to 1.0
    float arousal = 0.0f;  // -1.0 to 1.0
    float dominance = 0.0f; // -1.0 to 1.0
    std::map<entt::entity, float> relationship_web; // Entity -> Affinity
    std::map<std::string, float> reputation_scores; // FactionID -> Score

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("pleasure", pleasure),
           cereal::make_nvp("arousal", arousal),
           cereal::make_nvp("dominance", dominance),
           cereal::make_nvp("relationship_web", relationship_web),
           cereal::make_nvp("reputation_scores", reputation_scores));
    }
};

/**
 * @brief [NEW CLASS] Defines an entity's standing in the social hierarchy.
 */
struct SocialHierarchyComponent {
    float status = 0.5f; // 0.0 to 1.0
    std::string class_title = "Citizen";
    bool is_autonomous = true;
    
    // Deference logic
    entt::entity currently_yielding_to = entt::null;
    int yield_ticks_remaining = 0;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("status", status),
           cereal::make_nvp("class_title", class_title),
           cereal::make_nvp("is_autonomous", is_autonomous),
           cereal::make_nvp("currently_yielding_to", currently_yielding_to),
           cereal::make_nvp("yield_ticks_remaining", yield_ticks_remaining));
    }
};

// =====================================================================
// Layer 3: Economic
// =====================================================================

struct ProvenanceStep {
    std::string owner_name;
    uint64_t timestamp;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("owner_name", owner_name),
           cereal::make_nvp("timestamp", timestamp));
    }
};

struct Layer3EconomicComponent {
    int cash_on_hand = 0;
    std::map<std::string, int> bank_accounts; // BankID -> Balance
    std::vector<entt::entity> inventory;
    int credit_score = 600;
    std::map<std::string, uint64_t> portfolio; // Ticker -> Shares

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("cash_on_hand", cash_on_hand),
           cereal::make_nvp("bank_accounts", bank_accounts),
           cereal::make_nvp("inventory", inventory),
           cereal::make_nvp("credit_score", credit_score),
           cereal::make_nvp("portfolio", portfolio));
    }
};

struct StockComponent {
    std::string ticker;
    std::string company_name;
    double current_price = 100.0;
    double previous_price = 100.0;
    double opening_price = 100.0;
    uint64_t total_shares = 1000000;
    uint64_t outstanding_shares = 0;
    float volatility = 0.05f;
    float momentum = 0.0f;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("ticker", ticker),
           cereal::make_nvp("company_name", company_name),
           cereal::make_nvp("current_price", current_price),
           cereal::make_nvp("previous_price", previous_price),
           cereal::make_nvp("opening_price", opening_price),
           cereal::make_nvp("total_shares", total_shares),
           cereal::make_nvp("outstanding_shares", outstanding_shares),
           cereal::make_nvp("volatility", volatility),
           cereal::make_nvp("momentum", momentum));
    }
};

// =====================================================================
// Layer 4: Political/Power
// =====================================================================

enum class FactionArchetype : uint8_t {
    CONSENSUS,    // Aura-9: Order, Routine, Efficiency
    ENTROPIC_DRIFT, // Malware-Alpha: Chaos, Incentives, Stolen Data
    SILICON_MAW,    // The Architect: Bio-Digital Hybrid, Energy over Hunger
    VOID_WALKERS,  // The Signal: Synchronicity, Geometric Ritual
    SYNDICATE,     // The Arbiter: Combat, Gambling, Entertainment
    UNALIGNED
};

enum class DirectiveType : uint8_t {
    NONE,
    ROUTINE_ENFORCEMENT, // Increases speed if on schedule, hurts frustration if not
    UTILITY_BURST,       // Massive weight to specific actions (e.g. hack, graffiti)
    BIOLOGICAL_OVERRIDE, // Modifies need decay rates (hunger -> charge)
    SYNCHRONICITY,       // Moves agents to specific coordinates at specific times
    CRACKDOWN            // Guards aggressive towards non-faction members
};

struct FactionLeaderComponent {
    std::string leader_name;
    FactionArchetype archetype = FactionArchetype::UNALIGNED;
    float influence_strength = 1.0f; // Multiplier for directives

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("leader_name", leader_name),
           cereal::make_nvp("archetype", archetype),
           cereal::make_nvp("influence_strength", influence_strength));
    }
};

struct FactionDirectiveComponent {
    DirectiveType active_directive = DirectiveType::NONE;
    int duration_ticks = 0;
    float magnitude = 1.0f;
    std::string target_faction_id = ""; // If applicable
    
    // For Synchronicity: Target coordinates
    int target_x = -1;
    int target_y = -1;
    int target_layer = 0;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("active_directive", active_directive),
           cereal::make_nvp("duration_ticks", duration_ticks),
           cereal::make_nvp("magnitude", magnitude),
           cereal::make_nvp("target_faction_id", target_faction_id),
           cereal::make_nvp("target_x", target_x),
           cereal::make_nvp("target_y", target_y),
           cereal::make_nvp("target_layer", target_layer));
    }
};

struct Layer4PoliticalComponent {
    std::string primary_faction;
    float faction_loyalty = 1.0f;
    std::string rank;
    float soft_power = 0.0f;
    float hard_power = 0.0f;
    float directive_compliance = 1.0f; // 0.0 to 1.0: how much they obey the leader

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("primary_faction", primary_faction),
           cereal::make_nvp("faction_loyalty", faction_loyalty),
           cereal::make_nvp("rank", rank),
           cereal::make_nvp("soft_power", soft_power),
           cereal::make_nvp("hard_power", hard_power),
           cereal::make_nvp("directive_compliance", directive_compliance));
    }
};

// =====================================================================
// Layer 3 (Chunk-Level Economics)
// =====================================================================

struct MarketDemandComponent {
    // item_type_id -> demand_score (aggregate needs of agents)
    std::map<uint32_t, float> item_type_demand;
    // item_type_id -> scarcity_multiplier (demand/supply)
    std::map<uint32_t, float> item_type_scarcity;
    
    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("item_type_demand", item_type_demand),
           cereal::make_nvp("item_type_scarcity", item_type_scarcity));
    }
};

// =====================================================================
// Layer 4 (Chunk-Level Political/Influence)
// =====================================================================

struct FactionInfluenceFieldComponent {
    // faction_id -> influence value (0.0 to 100.0)
    std::map<std::string, float> influence;
    
    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("influence", influence));
    }
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_SIMULATION_LAYERS_H
