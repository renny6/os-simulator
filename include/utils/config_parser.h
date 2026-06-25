#pragma once

#include <string>
#include <vector>

namespace utils {

struct SystemConfig {
    int num_frames;
    int page_size;
    int num_cores;
    int time_slice_quantum;
    int tick_interval_ms;
    std::string scheduling_algorithm;
    std::string page_replacement_algorithm;
    float thrashing_fault_threshold;
    int warmup_cycles;
    int telemetry_ring_buffer_size;
    std::string output_csv_path;
};

struct ResourceConfig {
    std::vector<int> total_resources;
};

struct ProcessConfig {
    int pid;
    std::string name;
    int burst_time;
    int priority;
    int arrival_cycle;
    std::vector<int> max_resources;
    std::vector<int> initial_resources;
    std::vector<int> page_references;
};

struct ValidationConfig {
    std::vector<int> validation_trace;
};

struct ConfigData {
    SystemConfig system;
    ResourceConfig resources;
    std::vector<ProcessConfig> processes;
    ValidationConfig validation;
};

class ConfigParser {
public:
    static ConfigData parse(const std::string& filepath);
};

} // namespace utils
