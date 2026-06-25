#include "utils/config_parser.h"
#include <yaml-cpp/yaml.h>
#include <stdexcept>

namespace utils {

ConfigData ConfigParser::parse(const std::string& filepath) {
    try {
        YAML::Node root = YAML::LoadFile(filepath);
        ConfigData config;

        // Parse system
        if (!root["system"]) {
            throw std::runtime_error("Missing 'system' section in config");
        }
        auto system = root["system"];
        config.system.num_frames = system["num_frames"].as<int>();
        config.system.page_size = system["page_size"].as<int>();
        config.system.num_cores = system["num_cores"].as<int>();
        config.system.time_slice_quantum = system["time_slice_quantum"].as<int>();
        config.system.tick_interval_ms = system["tick_interval_ms"].as<int>();
        config.system.scheduling_algorithm = system["scheduling_algorithm"].as<std::string>();
        config.system.page_replacement_algorithm = system["page_replacement_algorithm"].as<std::string>();
        config.system.thrashing_fault_threshold = system["thrashing_fault_threshold"].as<float>();
        config.system.warmup_cycles = system["warmup_cycles"].as<int>();
        config.system.telemetry_ring_buffer_size = system["telemetry_ring_buffer_size"].as<int>();
        config.system.output_csv_path = system["output_csv_path"].as<std::string>();

        // Parse resources
        if (!root["resources"]) {
            throw std::runtime_error("Missing 'resources' section in config");
        }
        auto resources = root["resources"];
        config.resources.total_resources = resources["total_resources"].as<std::vector<int>>();

        // Parse processes
        if (!root["processes"]) {
            throw std::runtime_error("Missing 'processes' section in config");
        }
        for (auto p : root["processes"]) {
            ProcessConfig pconfig;
            pconfig.pid = p["pid"].as<int>();
            pconfig.name = p["name"].as<std::string>();
            pconfig.burst_time = p["burst_time"].as<int>();
            pconfig.priority = p["priority"].as<int>();
            pconfig.arrival_cycle = p["arrival_cycle"].as<int>();
            pconfig.max_resources = p["max_resources"].as<std::vector<int>>();
            pconfig.initial_resources = p["initial_resources"].as<std::vector<int>>();
            pconfig.page_references = p["page_references"].as<std::vector<int>>();
            config.processes.push_back(pconfig);
        }

        // Parse validation (Optional based on schema, but included in our config.yaml)
        if (root["validation"]) {
            auto validation = root["validation"];
            config.validation.validation_trace = validation["validation_trace"].as<std::vector<int>>();
        }

        return config;

    } catch (const YAML::BadFile& e) {
        throw std::runtime_error("Failed to load config file: " + filepath);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("YAML parsing error in " + filepath + ": " + e.what());
    }
}

} // namespace utils
