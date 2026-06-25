#pragma once

#include <string>
#include <vector>
#include <shared_mutex>
#include "../utils/config_parser.h"

namespace core {

enum class ProcessState {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
};

struct PCBSnapshot {
    int pid;
    ProcessState state;
    int base_priority;
    int current_priority;
    int remaining_burst_time;
    int starvation_counter;
    bool priority_boosted;
};

class ProcessControlBlock {
public:
    explicit ProcessControlBlock(const utils::ProcessConfig& config);

    // Disable copy semantics due to shared_mutex
    ProcessControlBlock(const ProcessControlBlock&) = delete;
    ProcessControlBlock& operator=(const ProcessControlBlock&) = delete;

    // Returns a lock-free snapshot for the UI thread
    PCBSnapshot get_snapshot() const;

    // Getters
    int get_pid() const;
    std::string get_name() const;
    ProcessState get_current_state() const;
    int get_base_priority() const;
    int get_current_priority() const;
    int get_burst_time() const;
    int get_remaining_burst_time() const;
    int get_arrival_cycle() const;
    int get_starvation_counter() const;
    int get_flash_countdown() const;
    
    std::vector<int> get_page_references() const;
    int get_page_ref_index() const;
    
    std::vector<int> get_max_resources() const;
    std::vector<int> get_allocated_resources() const;

    // Setters & Modifiers
    void set_state(ProcessState state);
    void set_current_priority(int priority);
    void decrement_remaining_burst_time();
    void increment_starvation_counter();
    void reset_starvation_counter();
    void set_flash_countdown(int countdown);
    void decrement_flash_countdown();

    // Memory Tracking
    bool has_more_page_references() const;
    int get_next_page_reference();

    // Resource Tracking
    void allocate_resources(const std::vector<int>& resources);
    void release_resources(const std::vector<int>& resources);

private:
    int pid;
    std::string name;
    ProcessState current_state;
    int base_priority;
    int current_priority;
    int burst_time;
    int remaining_burst_time;
    int arrival_cycle;
    int starvation_counter;
    int flash_countdown;
    std::vector<int> page_references;
    int page_ref_index;
    std::vector<int> max_resources;
    std::vector<int> allocated_resources;

    mutable std::shared_mutex rw_mutex;
};

} // namespace core
