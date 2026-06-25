#include "core/process.h"
#include <mutex>
#include <shared_mutex>

namespace core {

ProcessControlBlock::ProcessControlBlock(const utils::ProcessConfig& config)
    : pid(config.pid),
      name(config.name),
      current_state(ProcessState::READY),
      base_priority(config.priority),
      current_priority(config.priority),
      burst_time(config.burst_time),
      remaining_burst_time(config.burst_time),
      arrival_cycle(config.arrival_cycle),
      starvation_counter(0),
      flash_countdown(0),
      blocked_on_mutex_id(-1),
      page_references(config.page_references),
      page_ref_index(0),
      max_resources(config.max_resources),
      allocated_resources(config.initial_resources) {}

PCBSnapshot ProcessControlBlock::get_snapshot() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return PCBSnapshot{
        pid,
        current_state,
        base_priority,
        current_priority,
        remaining_burst_time,
        starvation_counter,
        current_priority > base_priority // priority_boosted
    };
}

int ProcessControlBlock::get_pid() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return pid;
}

std::string ProcessControlBlock::get_name() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return name;
}

ProcessState ProcessControlBlock::get_current_state() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return current_state;
}

int ProcessControlBlock::get_base_priority() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return base_priority;
}

int ProcessControlBlock::get_current_priority() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return current_priority;
}

int ProcessControlBlock::get_burst_time() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return burst_time;
}

int ProcessControlBlock::get_remaining_burst_time() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return remaining_burst_time;
}

int ProcessControlBlock::get_arrival_cycle() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return arrival_cycle;
}

int ProcessControlBlock::get_starvation_counter() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return starvation_counter;
}

int ProcessControlBlock::get_flash_countdown() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return flash_countdown;
}

std::vector<int> ProcessControlBlock::get_page_references() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return page_references;
}

int ProcessControlBlock::get_page_ref_index() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return page_ref_index;
}

std::vector<int> ProcessControlBlock::get_max_resources() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return max_resources;
}

std::vector<int> ProcessControlBlock::get_allocated_resources() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return allocated_resources;
}

void ProcessControlBlock::set_state(ProcessState state) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    current_state = state;
}

void ProcessControlBlock::set_current_priority(int priority) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    current_priority = priority;
}

void ProcessControlBlock::decrement_remaining_burst_time() {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    if (remaining_burst_time > 0) {
        remaining_burst_time--;
    }
}

void ProcessControlBlock::increment_starvation_counter() {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    starvation_counter++;
}

void ProcessControlBlock::reset_starvation_counter() {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    starvation_counter = 0;
}

void ProcessControlBlock::set_flash_countdown(int countdown) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    flash_countdown = countdown;
}

void ProcessControlBlock::decrement_flash_countdown() {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    if (flash_countdown > 0) {
        flash_countdown--;
    }
}

bool ProcessControlBlock::has_more_page_references() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return page_ref_index < page_references.size();
}

int ProcessControlBlock::get_next_page_reference() {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    if (page_ref_index < page_references.size()) {
        return page_references[page_ref_index++];
    }
    return -1;
}

void ProcessControlBlock::allocate_resources(const std::vector<int>& resources) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    for (size_t i = 0; i < allocated_resources.size() && i < resources.size(); ++i) {
        allocated_resources[i] += resources[i];
    }
}

void ProcessControlBlock::release_resources(const std::vector<int>& resources) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    for (size_t i = 0; i < allocated_resources.size() && i < resources.size(); ++i) {
        allocated_resources[i] -= resources[i];
    }
}

int ProcessControlBlock::get_blocked_on_mutex_id() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return blocked_on_mutex_id;
}

void ProcessControlBlock::set_blocked_on_mutex_id(int mutex_id) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    blocked_on_mutex_id = mutex_id;
}

std::unordered_set<int> ProcessControlBlock::get_held_mutex_ids() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return held_mutex_ids;
}

void ProcessControlBlock::add_held_mutex(int mutex_id) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    held_mutex_ids.insert(mutex_id);
}

void ProcessControlBlock::remove_held_mutex(int mutex_id) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    held_mutex_ids.erase(mutex_id);
}

} // namespace core
