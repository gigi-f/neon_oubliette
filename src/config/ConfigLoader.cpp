#include "ConfigLoader.h"
#include <fstream>
#include <stdexcept>
#include <iostream>

// Assuming nlohmann/json is available for parsing
#include "nlohmann/json.hpp"

namespace NeonOubliette::Config {

// Helper for nlohmann/json to populate WorldGeneratorParams
void from_json(const nlohmann::json& j, WorldGeneratorParams::TerrainTypes& tt) {
    j.at("ground_noise_frequency").get_to(tt.ground_noise_frequency);
    j.at("ground_noise_octaves").get_to(tt.ground_noise_octaves);
    j.at("water_level_threshold").get_to(tt.water_level_threshold);
    j.at("elevation_scale").get_to(tt.elevation_scale);
}

void from_json(const nlohmann::json& j, WorldGeneratorParams::ResourceNodeDistributionEntry& rnde) {
    j.at("resource_type").get_to(rnde.resource_type);
    j.at("min_per_district").get_to(rnde.min_per_district);
    j.at("max_per_district").get_to(rnde.max_per_district);
    j.at("spawn_chance").get_to(rnde.spawn_chance);
}

void from_json(const nlohmann::json& j, WorldGeneratorParams& wgp) {
    j.at("city_seed").get_to(wgp.city_seed);
    j.at("size_x").get_to(wgp.size_x);
    j.at("size_y").get_to(wgp.size_y);
    j.at("num_districts").get_to(wgp.num_districts);
    j.at("terrain_types").get_to(wgp.terrain_types);
    j.at("resource_node_distribution").get_to(wgp.resource_node_distribution);
}

ConfigLoader::ConfigLoader(const std::string& base_config_path, const std::string& base_schema_path)
    : m_baseConfigPath(base_config_path),
      m_baseSchemaPath(base_schema_path),
      m_schemaValidator() // Initialize SchemaValidator
{
    if (!m_baseConfigPath.empty() && m_baseConfigPath.back() != '/') {
        m_baseConfigPath += '/';
    }
    if (!m_baseSchemaPath.empty() && m_baseSchemaPath.back() != '/') {
        m_baseSchemaPath += '/';
    }
}

WorldGeneratorParams ConfigLoader::loadWorldGeneratorParams(const std::string& filename) {
    return loadConfig<WorldGeneratorParams>(filename, "world_generator_params_schema.json"); 
}

} // namespace NeonOubliette::Config
