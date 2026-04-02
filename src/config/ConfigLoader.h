#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <stdexcept>
#include <entt/entt.hpp>
#include "nlohmann/json.hpp"
#include "schema_validator.h"

namespace NeonOubliette::Config {

// Structure to hold parsed world generator parameters
struct WorldGeneratorParams {
    uint32_t city_seed = 0;
    uint32_t size_x = 0;
    uint32_t size_y = 0;
    uint32_t num_districts = 0;

    struct TerrainTypes {
        double ground_noise_frequency = 0.0;
        uint32_t ground_noise_octaves = 0;
        double water_level_threshold = 0.0;
        double elevation_scale = 0.0;
    } terrain_types;

    struct ResourceNodeDistributionEntry {
        std::string resource_type;
        uint32_t min_per_district = 0;
        uint32_t max_per_district = 0;
        double spawn_chance = 0.0;
    };
    std::vector<ResourceNodeDistributionEntry> resource_node_distribution;
};

// JSON conversion helpers for nlohmann/json (ADL-found)
void from_json(const nlohmann::json& j, WorldGeneratorParams::TerrainTypes& tt);
void from_json(const nlohmann::json& j, WorldGeneratorParams::ResourceNodeDistributionEntry& rnde);
void from_json(const nlohmann::json& j, WorldGeneratorParams& wgp);

class ConfigLoader {
public:
    explicit ConfigLoader(const std::string& base_config_path, const std::string& base_schema_path);

    WorldGeneratorParams loadWorldGeneratorParams(const std::string& filename = "world_generator_params.json");

    template<typename T>
    T loadConfig(const std::string& filename, const std::string& schema_name) {
        std::string fullPath = m_baseConfigPath + filename;
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open config file: " + fullPath);
        }

        nlohmann::json j;
        try {
            file >> j;
        } catch (const nlohmann::json::exception& e) {
            throw std::runtime_error("JSON parsing error in " + fullPath + ": " + e.what());
        }

        // Auto-load schema if not already present
        if (!schema_name.empty()) {
            // Check if schema is already loaded by calling validate with empty doc (or just rely on validator state)
            // But better to just try loading it if we have a path
            std::string schemaPath = m_baseSchemaPath + schema_name;
            m_schemaValidator.loadSchema(schema_name, schemaPath); 
            
            if (!m_schemaValidator.validate(schema_name, j)) {
                throw std::runtime_error("Schema validation failed for " + fullPath + ": " + m_schemaValidator.getErrors());
            }
        }

        try {
            return j.get<T>();
        } catch (const nlohmann::json::exception& e) {
            throw std::runtime_error("JSON deserialization error in " + fullPath + " (after validation): " + e.what());
        }
    }

private:
    std::string m_baseConfigPath;
    std::string m_baseSchemaPath;
    SchemaValidator m_schemaValidator;
};

} // namespace NeonOubliette::Config
