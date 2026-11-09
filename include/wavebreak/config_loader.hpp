#pragma once

#include "wavebreak/common.hpp"
#include <string>
#include <unordered_map>

namespace wavebreak {

class ConfigLoader {
public:
    // Parses a flat JSON string with string or numeric values
    static std::unordered_map<std::string, std::string> parse_flat_json(const std::string& json_text);

    // Parses JSON and returns a validated Config
    static Config parse_config(const std::string& json_text);

    // Loads JSON from file path and returns a validated Config
    static Config load_from_file(const std::string& filepath);

    // Validates config fields; throws std::invalid_argument on error
    static void validate(const Config& cfg);
};

} // namespace wavebreak
