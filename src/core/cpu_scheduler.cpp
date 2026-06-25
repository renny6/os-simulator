#include "core/cpu_scheduler.h"
#include "core/concurrency_manager.h"
#include "utils/config_parser.h"
#include <iostream>
#include <random>
#include <unordered_map>
#include <vector>

namespace core {

extern ConcurrencyManager* g_cm;

static std::string g_scheduling_algorithm = "RR";
static int g_time_slice_quantum = 3;
static bool g_config_loaded = false;

// Map PID -> vector of pair<mutex_id, hold_ticks_remaining>
static std::unordered_map<int, std::vector<std::pair<int, int>>> g_held_mutexes;

static void load_config_if_needed() {
    if (!g_config_loaded) {
        try {
            auto config = utils::ConfigParser::parse("config.yaml");
            g_scheduling_algorithm = config.system.scheduling_algorithm;
            g_time_slice_quantum = config.system.time_slice_quantum;
        } catch (const std::exception& e) {
            std::cerr << "CpuScheduler warning: " << e.what() << "\n";
        }
        g_config_loaded = true;
    }
}

CpuScheduler::CpuScheduler() {
    load_config_if_needed();
}

void CpuScheduler::add_process(std::unique_ptr<ProcessControlBlock> pcb) {
    load_config_if_needed();
    processes_.push_back(std::move(pcb));
}

ProcessControlBlock* CpuScheduler::select_next_process() {
    load_config_if_needed();

    if (g_scheduling_algorithm == "RR") {
        static size_t rr_index = 0;
        if (processes_.empty()) return nullptr;
        
        for (size_t i = 0; i < processes_.size(); ++i) {
            size_t idx = (rr_index + i) % processes_.size();
            if (processes_[idx]->get_current_state() == ProcessState::READY) {
                rr_index = (idx + 1) % processes_.size();
                return processes_[idx].get();
            }
        }
        return nullptr;
    } 
    else if (g_scheduling_algorithm == "SRTF") {
        ProcessControlBlock* shortest = nullptr;
        int min_burst = 999999999;
        
        for (const auto& pcb : processes_) {
            if (pcb->get_current_state() == ProcessState::READY) {
                int burst = pcb->get_remaining_burst_time();
                if (burst < min_burst) {
                    min_burst = burst;
                    shortest = pcb.get();
                }
            }
        }
        return shortest;
    }
    
    return nullptr;
}

void CpuScheduler::context_switch() {
    if (active_process_) {
        if (active_process_->get_current_state() == ProcessState::RUNNING) {
            active_process_->set_state(ProcessState::READY);
        }
    }
    
    active_process_ = select_next_process();
    
    if (active_process_) {
        active_process_->set_state(ProcessState::RUNNING);
    }
}

void CpuScheduler::tick() {
    static int current_slice_ticks = 0;
    static uint64_t total_ticks = 0;
    static std::mt19937 rng(1337);
    std::uniform_int_distribution<int> dist(1, 100);
    
    total_ticks++;
    load_config_if_needed();

    if (!active_process_) {
        context_switch();
        current_slice_ticks = 0;
    }

    if (!active_process_) {
        return; 
    }

    int pid = active_process_->get_pid();

    // --- 1. Kernel Mutex Stress Test & Ownership Tracking ---
    if (g_cm) {
        auto& held_list = g_held_mutexes[pid];

        // Pick up newly inherited mutexes from unblocking events
        for (int m = 0; m < 6; ++m) {
            if (g_cm->get_mutex_owner(m) == pid) {
                bool tracked = false;
                for (const auto& p : held_list) {
                    if (p.first == m) { tracked = true; break; }
                }
                if (!tracked) {
                    std::uniform_int_distribution<int> hold_dist(3, 7);
                    int htime = hold_dist(rng);
                    held_list.push_back({m, htime});
                    std::cout << "[Sim Cycle " << total_ticks << "] PID " << pid << ": Woke up holding Mutex " << m << "\n";
                }
            }
        }

        // Decrement hold duration and release expired mutexes
        for (auto it = held_list.begin(); it != held_list.end(); ) {
            it->second--;
            if (it->second <= 0) {
                int mid = it->first;
                g_cm->release_mutex(mid, pid);
                std::cout << "[Sim Cycle " << total_ticks << "] PID " << pid << ": Released Mutex " << mid << "\n";
                it = held_list.erase(it);
            } else {
                ++it;
            }
        }

        // 20% chance to acquire random mutex 0-5
        if (dist(rng) <= 20) {
            std::uniform_int_distribution<int> mid_dist(0, 5);
            std::uniform_int_distribution<int> hold_dist(3, 7);
            int mid = mid_dist(rng);

            bool already_held = false;
            for (const auto& p : held_list) {
                if (p.first == mid) { already_held = true; break; }
            }

            if (!already_held) {
                bool acquired = g_cm->try_acquire_mutex(mid, pid);
                if (acquired) {
                    int htime = hold_dist(rng);
                    held_list.push_back({mid, htime});
                    std::cout << "[Sim Cycle " << total_ticks << "] PID " << pid << ": Acquired Mutex " << mid << " (holding for " << htime << " ticks)\n";
                } else {
                    std::cout << "[Sim Cycle " << total_ticks << "] PID " << pid << ": Blocked on Mutex " << mid << "\n";
                    active_process_->set_state(ProcessState::BLOCKED);
                    active_process_ = nullptr;
                    current_slice_ticks = 0;
                    context_switch();
                    return;
                }
            }
        }
    }
    // --------------------------------------------------------

    // --- 2. Automated Resource Request / Release Simulation ---
    if (g_cm && dist(rng) <= 45) {
        auto max_res = active_process_->get_max_resources();
        auto alloc_res = active_process_->get_allocated_resources();

        std::vector<int> need(max_res.size(), 0);
        bool has_need = false;
        for (size_t i = 0; i < max_res.size(); ++i) {
            need[i] = max_res[i] - alloc_res[i];
            if (need[i] > 0) has_need = true;
        }

        bool holds_resources = false;
        for (int r : alloc_res) {
            if (r > 0) { holds_resources = true; break; }
        }

        if (has_need && (!holds_resources || dist(rng) <= 65)) {
            std::vector<int> req(max_res.size(), 0);
            for (size_t i = 0; i < need.size(); ++i) {
                if (need[i] > 0) { req[i] = 1; break; }
            }
            bool granted = g_cm->request_resources(pid, req);
            if (granted) {
                std::cout << "[Sim Cycle " << total_ticks << "] PID " << pid << ": Banker request granted.\n";
            }
        } else if (holds_resources) {
            std::vector<int> rel(alloc_res.size(), 0);
            for (size_t i = 0; i < alloc_res.size(); ++i) {
                if (alloc_res[i] > 0) { rel[i] = 1; break; }
            }
            g_cm->release_resources(pid, rel);
            std::cout << "[Sim Cycle " << total_ticks << "] PID " << pid << ": Released Banker resources.\n";
        }
    }
    // --------------------------------------------------------

    active_process_->decrement_remaining_burst_time();
    current_slice_ticks++;

    if (active_process_->get_remaining_burst_time() <= 0) {
        // Release any held mutexes upon termination
        if (g_cm) {
            for (const auto& p : g_held_mutexes[pid]) {
                g_cm->release_mutex(p.first, pid);
            }
            g_held_mutexes[pid].clear();
        }

        active_process_->set_state(ProcessState::TERMINATED);
        active_process_ = nullptr;
        current_slice_ticks = 0;
        
        context_switch();
        return;
    }

    if (g_scheduling_algorithm == "RR") {
        if (current_slice_ticks >= g_time_slice_quantum) {
            current_slice_ticks = 0;
            context_switch();
        }
    } 
    else if (g_scheduling_algorithm == "SRTF") {
        ProcessControlBlock* shortest_ready = select_next_process();
        
        if (shortest_ready && shortest_ready->get_remaining_burst_time() < active_process_->get_remaining_burst_time()) {
            context_switch();
        }
    }
}

void CpuScheduler::run_loop() {
    tick();
}

} // namespace core
