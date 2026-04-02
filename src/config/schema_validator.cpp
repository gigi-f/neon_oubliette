#include "schema_validator.h"

namespace NeonOubliette::Config {

bool SchemaValidator::loadSchema(const std::string& schema_id, const std::string& schema_filepath) {
    std::ifstream file(schema_filepath);
    if (!file.is_open()) {
        last_errors = "Failed to open schema file: " + schema_filepath;
        return false;
    }

    nlohmann::json schema_json;
    try {
        file >> schema_json;
    } catch (const nlohmann::json::parse_error& e) {
        last_errors = "Failed to parse schema JSON from " + schema_filepath + ": " + e.what();
        return false;
    }

    try {
        // Create and store the validator. nlohmann::json_schema::json_validator is not copy-assignable
        // so we need to store it by value or unique_ptr, map stores by value.
        schemas.emplace(schema_id, nlohmann::json_schema::json_validator(schema_json));
        return true;
    } catch (const std::exception& e) {
        last_errors = "Failed to create schema validator for " + schema_id + ": " + e.what();
        return false;
    }
}

bool SchemaValidator::validate(const std::string& schema_id, const nlohmann::json& document) const {
    auto it = schemas.find(schema_id);
    if (it == schemas.end()) {
        last_errors = "Schema '" + schema_id + "' not loaded.";
        return false;
    }

    last_errors.clear();
    try {
        it->second.validate(document);
        return true;
    } catch (const std::exception& e) {
        last_errors = e.what();
        return false;
    }
}

std::string SchemaValidator::getErrors() const {
    return last_errors;
}

} // namespace NeonOubliette::Config
