#include "feature_flags.h"
#include <fstream>
#include <iostream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace NeonOubliette {

FeatureFlags g_feature_flags;

bool FeatureFlags::is_system_enabled(const std::string& name) const {
    // ── Input / core (always on) ──────────────────────────────
    if (name == "Input")         return true;
    if (name == "TurnManager")   return true;
    if (name == "Logging")       return true;
    if (name == "Serialization") return true;
    if (name == "GodMode")       return true;
    if (name == "Visibility")    return true;
    if (name == "Sound")         return true;
    if (name == "Milestone")     return true;

    // ── Rendering ────────────────────────────────────────────
    if (name == "SDLRenderer" || name == "SDLRendering" || name == "Rendering") return rendering;

    // ── NPC / agent systems ──────────────────────────────────
    if (name == "AgentSpawn")    return npcs && npc.spawning;
    if (name == "Movement")      return npcs && npc.movement;
    if (name == "Vertical")      return npcs && npc.movement;
    if (name == "MacroNav")      return npcs && npc.pathfinding;
    if (name == "Pathfinding")   return npcs && npc.pathfinding;
    if (name == "AgentDecision") return npcs;
    if (name == "AgentAction")   return npcs;
    if (name == "Activity")      return npcs;
    if (name == "Population")    return npcs;
    if (name == "Dialogue")      return npcs && npc.dialogue;
    if (name == "Interaction")   return npcs;
    if (name == "Inspection")    return npcs;
    if (name == "SocialInteraction") return npcs && npc.social_interaction;
    if (name == "Conversation")  return npcs && npc.social_interaction;
    if (name == "Cognitive")     return npcs && npc.cognitive;
    if (name == "Xeno")          return npcs && npc.cognitive;
    if (name == "Inheritance")   return npcs;

    // NPC crime / law enforcement
    // Note: pol.guard_response is checked here even when politics == false.
    // Guard behaviour is considered NPC-side; the politics gate does NOT disable it.
    if (name == "GuardResponse") return npcs && npc.crime && pol.guard_response;
    if (name == "WantedLevel")   return npcs && npc.crime && pol.guard_response;

    // ── City generation ──────────────────────────────────────
    if (name == "CityGen")       return city_generation && city.buildings;
    if (name == "BuildingGen")   return city_generation && city.buildings;
    if (name == "ZoningSolver")  return city_generation && city.zoning;
    if (name == "InfraNetwork")  return city_generation && city.infrastructure;
    if (name == "Infrastructure") return city_generation && city.infrastructure;
    if (name == "InfraInfluence") return city_generation && city.infrastructure;
    if (name == "PowerGrid")     return city_generation && city.power_grid;
    if (name == "Transit")       return city_generation && city.transit;
    if (name == "ChunkStream")   return city_generation && city.chunk_streaming;
    if (name == "Demolition")    return city_generation && city.demolition;
    if (name == "Rebuilding")    return city_generation && city.demolition;
    if (name == "UrbanDecay")    return city_generation && city.urban_decay;

    // ── Economy ──────────────────────────────────────────────
    if (name == "Economic")      return economy && econ.markets;
    if (name == "EconomicMarket") return economy && econ.markets;
    if (name == "SupplyChain")   return economy && econ.supply_chain;
    if (name == "Production")    return economy && econ.production;
    if (name == "DrugManufacturing") return economy && econ.drug_manufacturing;
    if (name == "StockMarket")   return economy && econ.stock_market;
    if (name == "ResourceDistribution") return economy && econ.resource_distribution;
    if (name == "Barter")        return economy && econ.barter;
    if (name == "Crafting")      return economy;
    if (name == "Consumption")   return economy;
    if (name == "Container")     return player_actions;
    if (name == "ItemUsage")     return player_actions;
    if (name == "Information")   return economy || politics;

    // ── Politics / factions ──────────────────────────────────
    if (name == "Faction")       return politics && pol.factions;
    if (name == "Religion")      return politics && pol.religion;
    if (name == "Political")     return politics;
    if (name == "PoliticalOpinion") return politics && pol.political_opinions;
    if (name == "Crisis")        return politics && pol.crisis;
    if (name == "CrisisDashboard") return politics && pol.crisis;
    if (name == "BroadcastTower") return politics && pol.media;
    if (name == "UndergroundMedia") return politics && pol.media;

    // ── Environment / simulation layers ──────────────────────
    if (name == "Physics")       return environment && env.physics;
    if (name == "Biology")       return environment && env.biology;
    if (name == "Ecosystem")     return environment && env.ecosystem;
    if (name == "Environmental") return environment;
    if (name == "Hazard")        return environment && env.hazards;

    // Unknown system — allow by default so new systems aren't silently dropped
    return true;
}

void FeatureFlags::load_from_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    try {
        json j;
        f >> j;

        // Top-level toggles
        if (j.contains("npcs")) npcs = j["npcs"];
        if (j.contains("city_generation")) city_generation = j["city_generation"];
        if (j.contains("economy")) economy = j["economy"];
        if (j.contains("politics")) politics = j["politics"];
        if (j.contains("environment")) environment = j["environment"];
        if (j.contains("rendering")) rendering = j["rendering"];
        if (j.contains("player_actions")) player_actions = j["player_actions"];

        // NPC sub-flags
        if (j.contains("npc")) {
            auto& n = j["npc"];
            if (n.contains("spawning")) npc.spawning = n["spawning"];
            if (n.contains("movement")) npc.movement = n["movement"];
            if (n.contains("pathfinding")) npc.pathfinding = n["pathfinding"];
            if (n.contains("dialogue")) npc.dialogue = n["dialogue"];
            if (n.contains("social_interaction")) npc.social_interaction = n["social_interaction"];
            if (n.contains("faction_behavior")) npc.faction_behavior = n["faction_behavior"];
            if (n.contains("crime")) npc.crime = n["crime"];
            if (n.contains("cognitive")) npc.cognitive = n["cognitive"];
        }

        // City sub-flags
        if (j.contains("city")) {
            auto& c = j["city"];
            if (c.contains("buildings")) city.buildings = c["buildings"];
            if (c.contains("infrastructure")) city.infrastructure = c["infrastructure"];
            if (c.contains("transit")) city.transit = c["transit"];
            if (c.contains("zoning")) city.zoning = c["zoning"];
            if (c.contains("chunk_streaming")) city.chunk_streaming = c["chunk_streaming"];
            if (c.contains("demolition")) city.demolition = c["demolition"];
            if (c.contains("power_grid")) city.power_grid = c["power_grid"];
            if (c.contains("urban_decay")) city.urban_decay = c["urban_decay"];
        }

        // Economy sub-flags
        if (j.contains("econ")) {
            auto& e = j["econ"];
            if (e.contains("markets")) econ.markets = e["markets"];
            if (e.contains("supply_chain")) econ.supply_chain = e["supply_chain"];
            if (e.contains("drug_manufacturing")) econ.drug_manufacturing = e["drug_manufacturing"];
            if (e.contains("stock_market")) econ.stock_market = e["stock_market"];
            if (e.contains("barter")) econ.barter = e["barter"];
            if (e.contains("production")) econ.production = e["production"];
            if (e.contains("resource_distribution")) econ.resource_distribution = e["resource_distribution"];
        }

        // Politics sub-flags
        if (j.contains("pol")) {
            auto& p = j["pol"];
            if (p.contains("factions")) pol.factions = p["factions"];
            if (p.contains("religion")) pol.religion = p["religion"];
            if (p.contains("political_opinions")) pol.political_opinions = p["political_opinions"];
            if (p.contains("guard_response")) pol.guard_response = p["guard_response"];
            if (p.contains("crisis")) pol.crisis = p["crisis"];
            if (p.contains("media")) pol.media = p["media"];
        }

        // Environment sub-flags
        if (j.contains("env")) {
            auto& ev = j["env"];
            if (ev.contains("physics")) env.physics = ev["physics"];
            if (ev.contains("biology")) env.biology = ev["biology"];
            if (ev.contains("ecosystem")) env.ecosystem = ev["ecosystem"];
            if (ev.contains("hazards")) env.hazards = ev["hazards"];
        }

    } catch (const std::exception& e) {
        std::cerr << "Error loading feature flags from " << path << ": " << e.what() << std::endl;
    }
}

void FeatureFlags::save_to_file(const std::string& path) {
    json j;
    
    j["npcs"] = npcs;
    j["city_generation"] = city_generation;
    j["economy"] = economy;
    j["politics"] = politics;
    j["environment"] = environment;
    j["rendering"] = rendering;
    j["player_actions"] = player_actions;

    j["npc"] = {
        {"spawning", npc.spawning},
        {"movement", npc.movement},
        {"pathfinding", npc.pathfinding},
        {"dialogue", npc.dialogue},
        {"social_interaction", npc.social_interaction},
        {"faction_behavior", npc.faction_behavior},
        {"crime", npc.crime},
        {"cognitive", npc.cognitive}
    };

    j["city"] = {
        {"buildings", city.buildings},
        {"infrastructure", city.infrastructure},
        {"transit", city.transit},
        {"zoning", city.zoning},
        {"chunk_streaming", city.chunk_streaming},
        {"demolition", city.demolition},
        {"power_grid", city.power_grid},
        {"urban_decay", city.urban_decay}
    };

    j["econ"] = {
        {"markets", econ.markets},
        {"supply_chain", econ.supply_chain},
        {"drug_manufacturing", econ.drug_manufacturing},
        {"stock_market", econ.stock_market},
        {"barter", econ.barter},
        {"production", econ.production},
        {"resource_distribution", econ.resource_distribution}
    };

    j["pol"] = {
        {"factions", pol.factions},
        {"religion", pol.religion},
        {"political_opinions", pol.political_opinions},
        {"guard_response", pol.guard_response},
        {"crisis", pol.crisis},
        {"media", pol.media}
    };

    j["env"] = {
        {"physics", env.physics},
        {"biology", env.biology},
        {"ecosystem", env.ecosystem},
        {"hazards", env.hazards}
    };

    std::ofstream f(path);
    if (f.is_open()) {
        f << j.dump(4);
    }
}

} // namespace NeonOubliette
