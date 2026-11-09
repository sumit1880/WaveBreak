#include "spectrum_sentinel/config_loader.hpp"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <set>

namespace spectrum_sentinel {

static inline void skip_whitespace(const std::string& str, size_t& pos) {
    while (pos < str.size() && (std::isspace(static_cast<unsigned char>(str[pos])))) {
        ++pos;
    }
}

static std::string parse_string_token(const std::string& str, size_t& pos) {
    if (pos >= str.size() || str[pos] != '"') {
        throw std::runtime_error("Expected '\"' at position " + std::to_string(pos));
    }
    ++pos; // skip opening quote
    std::string result;
    bool escaped = false;
    while (pos < str.size()) {
        char c = str[pos++];
        if (escaped) {
            if (c == '"' || c == '\\' || c == '/') {
                result.push_back(c);
            } else if (c == 'b') {
                result.push_back('\b');
            } else if (c == 'f') {
                result.push_back('\f');
            } else if (c == 'n') {
                result.push_back('\n');
            } else if (c == 'r') {
                result.push_back('\r');
            } else if (c == 't') {
                result.push_back('\t');
            } else {
                result.push_back(c);
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return result;
        } else {
            result.push_back(c);
        }
    }
    throw std::runtime_error("Unterminated string in JSON");
}

std::unordered_map<std::string, std::string> ConfigLoader::parse_flat_json(const std::string& json_text) {
    std::unordered_map<std::string, std::string> result;
    size_t pos = 0;
    skip_whitespace(json_text, pos);

    if (pos >= json_text.size() || json_text[pos] != '{') {
        throw std::runtime_error("Malformed JSON: must start with '{'");
    }
    ++pos; // skip '{'

    while (pos < json_text.size()) {
        skip_whitespace(json_text, pos);
        if (pos >= json_text.size()) {
            throw std::runtime_error("Malformed JSON: missing closing '}'");
        }
        if (json_text[pos] == '}') {
            ++pos;
            skip_whitespace(json_text, pos);
            if (pos != json_text.size()) {
                throw std::runtime_error("Malformed JSON: unexpected extra characters after closing '}'");
            }
            return result;
        }

        // Parse key
        std::string key = parse_string_token(json_text, pos);

        skip_whitespace(json_text, pos);
        if (pos >= json_text.size() || json_text[pos] != ':') {
            throw std::runtime_error("Malformed JSON: expected ':' after key \"" + key + "\"");
        }
        ++pos; // skip ':'
        skip_whitespace(json_text, pos);

        if (pos >= json_text.size()) {
            throw std::runtime_error("Malformed JSON: unexpected end of string expecting value for key \"" + key + "\"");
        }

        std::string val;
        if (json_text[pos] == '"') {
            val = parse_string_token(json_text, pos);
        } else {
            // Numeric token or boolean
            size_t val_start = pos;
            while (pos < json_text.size() &&
                   json_text[pos] != ',' &&
                   json_text[pos] != '}' &&
                   !std::isspace(static_cast<unsigned char>(json_text[pos]))) {
                ++pos;
            }
            val = json_text.substr(val_start, pos - val_start);
            if (val.empty()) {
                throw std::runtime_error("Malformed JSON: empty value for key \"" + key + "\"");
            }
        }

        result[key] = val;

        skip_whitespace(json_text, pos);
        if (pos < json_text.size() && json_text[pos] == ',') {
            ++pos; // skip comma
            skip_whitespace(json_text, pos);
            if (pos < json_text.size() && json_text[pos] == '}') {
                throw std::runtime_error("Malformed JSON: trailing comma before '}'");
            }
        } else if (pos < json_text.size() && json_text[pos] == '}') {
            // closing brace handled on next loop iteration
        } else {
            throw std::runtime_error("Malformed JSON: expected ',' or '}'");
        }
    }

    throw std::runtime_error("Malformed JSON: missing closing '}'");
}

Config ConfigLoader::parse_config(const std::string& json_text) {
    auto kv = parse_flat_json(json_text);
    Config cfg;

    if (kv.find("fs") != kv.end()) {
        cfg.fs = std::stod(kv["fs"]);
    }
    if (kv.find("num_samples") != kv.end()) {
        cfg.num_samples = std::stoull(kv["num_samples"]);
    }
    if (kv.find("fft_size") != kv.end()) {
        cfg.fft_size = std::stoull(kv["fft_size"]);
    }
    if (kv.find("hop_size") != kv.end()) {
        cfg.hop_size = std::stoull(kv["hop_size"]);
    }
    if (kv.find("desired_freq") != kv.end()) {
        cfg.desired_freq = std::stod(kv["desired_freq"]);
    }
    if (kv.find("desired_amp") != kv.end()) {
        cfg.desired_amp = std::stod(kv["desired_amp"]);
    }
    if (kv.find("scenario") != kv.end()) {
        cfg.scenario = kv["scenario"];
    }
    if (kv.find("seed") != kv.end()) {
        cfg.seed = std::stoull(kv["seed"]);
    }
    if (kv.find("output_dir") != kv.end()) {
        cfg.output_dir = kv["output_dir"];
    }
    if (kv.find("threshold_db") != kv.end()) {
        cfg.threshold_db = std::stod(kv["threshold_db"]);
    }
    if (kv.find("noise_floor_floor_db") != kv.end()) {
        cfg.noise_floor_floor_db = std::stod(kv["noise_floor_floor_db"]);
    }

    validate(cfg);
    return cfg;
}

Config ConfigLoader::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open configuration file: " + filepath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_config(buffer.str());
}

void ConfigLoader::validate(const Config& cfg) {
    validate_parameters(cfg.fs, cfg.fft_size, cfg.hop_size, cfg.desired_freq);

    if (cfg.num_samples == 0) {
        throw std::invalid_argument("num_samples must be greater than 0.");
    }
    if (cfg.num_samples < cfg.fft_size) {
        throw std::invalid_argument("num_samples must be >= fft_size.");
    }
    if (cfg.desired_amp < 0.0) {
        throw std::invalid_argument("desired_amp must be non-negative.");
    }

    static const std::set<std::string> valid_scenarios = {
        "clean", "low_noise", "high_noise",
        "narrowband", "broadband", "burst",
        "multi", "drift"
    };

    if (valid_scenarios.find(cfg.scenario) == valid_scenarios.end()) {
        throw std::invalid_argument("Unknown scenario: \"" + cfg.scenario + "\"");
    }
}

} // namespace spectrum_sentinel
