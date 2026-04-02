#ifndef NEON_OUBLIETTE_SCHEMA_VALIDATOR_H
#define NEON_OUBLIETTE_SCHEMA_VALIDATOR_H

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include <string>
#include <fstream>
#include <iostream>
#include <map>

namespace NeonOubliette::Config {

class SchemaValidator {
public:
    // Loads a JSON schema from a file and registers it.
    bool loadSchema(const std::string& schema_id, const std::string& schema_filepath);

    // Validates a JSON document against a registered schema.
    bool validate(const std::string& schema_id, const nlohmann::json& document) const;

    // Get validation errors as a string.
    std::string getErrors() const;

private:
    std::map<std::string, nlohmann::json_schema::json_validator> schemas;
    mutable std::string last_errors; // mutable to allow modification in const methods
};

} // namespace NeonOubliette::Config

#endif // NEON_OUBLIETTE_SCHEMA_VALIDATOR_H
