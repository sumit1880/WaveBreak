#include "wavebreak/io_writers.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace wavebreak {

bool IoWriters::ensure_directory(const std::string& dirpath) {
    if (dirpath.empty()) return true;
    try {
        if (!fs::exists(dirpath)) {
            return fs::create_directories(dirpath);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool IoWriters::write_samples_csv(
    const std::string& filepath,
    const SampleVector& samples)
{
    fs::path p(filepath);
    if (p.has_parent_path()) {
        ensure_directory(p.parent_path().string());
    }

    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "i,q\n";
    out << std::setprecision(12);
    for (const auto& s : samples) {
        out << s.real() << "," << s.imag() << "\n";
    }
    return true;
}

bool IoWriters::read_samples_csv(
    const std::string& filepath,
    SampleVector& out_samples)
{
    std::ifstream in(filepath);
    if (!in.is_open()) return false;

    out_samples.clear();
    std::string line;
    bool is_first = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (is_first) {
            is_first = false;
            if (line.find("i,q") != std::string::npos || line.find("i") != std::string::npos) {
                continue;
            }
        }
        std::stringstream ss(line);
        std::string i_str, q_str;
        if (std::getline(ss, i_str, ',') && std::getline(ss, q_str, ',')) {
            try {
                double i_val = std::stod(i_str);
                double q_val = std::stod(q_str);
                out_samples.emplace_back(i_val, q_val);
            } catch (...) {
                // skip corrupted lines
            }
        }
    }
    return true;
}

bool IoWriters::write_samples_meta_json(
    const std::string& filepath,
    const Config& config)
{
    fs::path p(filepath);
    if (p.has_parent_path()) {
        ensure_directory(p.parent_path().string());
    }

    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "{\n";
    out << "  \"fs\": " << std::fixed << std::setprecision(1) << config.fs << ",\n";
    out << "  \"num_samples\": " << config.num_samples << ",\n";
    out << "  \"scenario\": \"" << config.scenario << "\",\n";
    out << "  \"seed\": " << config.seed << ",\n";
    out << "  \"desired_freq\": " << std::fixed << std::setprecision(1) << config.desired_freq << ",\n";
    out << "  \"desired_amp\": " << std::fixed << std::setprecision(2) << config.desired_amp << "\n";
    out << "}\n";
    return true;
}

bool IoWriters::read_samples_meta_json(
    const std::string& filepath,
    double& out_fs,
    size_t& out_num_samples,
    std::string& out_scenario,
    uint64_t& out_seed)
{
    std::ifstream in(filepath);
    if (!in.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());

    auto find_number = [&](const std::string& key) -> double {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0.0;
        pos = content.find(':', pos);
        if (pos == std::string::npos) return 0.0;
        size_t end_pos = content.find_first_of(",}\n", pos);
        std::string num_str = content.substr(pos + 1, end_pos - pos - 1);
        try { return std::stod(num_str); } catch (...) { return 0.0; }
    };

    auto find_string = [&](const std::string& key) -> std::string {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = content.find(':', pos);
        if (pos == std::string::npos) return "";
        size_t q1 = content.find('"', pos);
        if (q1 == std::string::npos) return "";
        size_t q2 = content.find('"', q1 + 1);
        if (q2 == std::string::npos) return "";
        return content.substr(q1 + 1, q2 - q1 - 1);
    };

    out_fs = find_number("fs");
    out_num_samples = static_cast<size_t>(find_number("num_samples"));
    out_seed = static_cast<uint64_t>(find_number("seed"));
    out_scenario = find_string("scenario");
    return true;
}

bool IoWriters::write_ground_truth_json(
    const std::string& filepath,
    const std::vector<GroundTruthEvent>& events)
{
    fs::path p(filepath);
    if (p.has_parent_path()) {
        ensure_directory(p.parent_path().string());
    }

    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "[\n";
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& ev = events[i];
        out << "  {\n";
        out << "    \"id\": " << ev.id << ",\n";
        out << "    \"type\": \"" << ev.type << "\",\n";
        out << "    \"start_time\": " << std::fixed << std::setprecision(6) << ev.start_time << ",\n";
        out << "    \"end_time\": " << std::fixed << std::setprecision(6) << ev.end_time << ",\n";
        out << "    \"frequency\": " << std::fixed << std::setprecision(2) << ev.frequency << ",\n";
        out << "    \"freq_min\": " << std::fixed << std::setprecision(2) << ev.freq_min << ",\n";
        out << "    \"freq_max\": " << std::fixed << std::setprecision(2) << ev.freq_max << ",\n";
        out << "    \"amplitude\": " << std::fixed << std::setprecision(4) << ev.amplitude << ",\n";
        out << "    \"power_db\": " << std::fixed << std::setprecision(2) << ev.power_db << "\n";
        out << "  }" << (i + 1 < events.size() ? ",\n" : "\n");
    }
    out << "]\n";
    return true;
}

bool IoWriters::read_ground_truth_json(
    const std::string& filepath,
    std::vector<GroundTruthEvent>& out_events)
{
    out_events.clear();
    std::ifstream in(filepath);
    if (!in.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());

    size_t pos = 0;
    while ((pos = content.find('{', pos)) != std::string::npos) {
        size_t end_pos = content.find('}', pos);
        if (end_pos == std::string::npos) break;
        std::string block = content.substr(pos, end_pos - pos + 1);

        auto get_num = [&](const std::string& key) -> double {
            size_t kp = block.find("\"" + key + "\"");
            if (kp == std::string::npos) return 0.0;
            size_t c = block.find(':', kp);
            if (c == std::string::npos) return 0.0;
            size_t ep = block.find_first_of(",}\n", c);
            std::string s = block.substr(c + 1, ep - c - 1);
            try { return std::stod(s); } catch (...) { return 0.0; }
        };

        auto get_str = [&](const std::string& key) -> std::string {
            size_t kp = block.find("\"" + key + "\"");
            if (kp == std::string::npos) return "";
            size_t c = block.find(':', kp);
            if (c == std::string::npos) return "";
            size_t q1 = block.find('"', c);
            if (q1 == std::string::npos) return "";
            size_t q2 = block.find('"', q1 + 1);
            if (q2 == std::string::npos) return "";
            return block.substr(q1 + 1, q2 - q1 - 1);
        };

        GroundTruthEvent ev;
        ev.id = static_cast<int>(get_num("id"));
        ev.type = get_str("type");
        ev.start_time = get_num("start_time");
        ev.end_time = get_num("end_time");
        ev.frequency = get_num("frequency");
        ev.freq_min = get_num("freq_min");
        ev.freq_max = get_num("freq_max");
        ev.amplitude = get_num("amplitude");
        ev.power_db = get_num("power_db");

        out_events.push_back(ev);
        pos = end_pos + 1;
    }
    return true;
}

bool IoWriters::write_spectrogram_csv(
    const std::string& filepath,
    const std::vector<std::vector<double>>& spectrogram_db)
{
    fs::path p(filepath);
    if (p.has_parent_path()) {
        ensure_directory(p.parent_path().string());
    }

    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << std::setprecision(4) << std::fixed;
    for (size_t w = 0; w < spectrogram_db.size(); ++w) {
        const auto& row = spectrogram_db[w];
        for (size_t b = 0; b < row.size(); ++b) {
            out << row[b] << (b + 1 < row.size() ? "," : "\n");
        }
    }
    return true;
}

} // namespace wavebreak
