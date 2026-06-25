#include "core/concurrency_manager.h"
#include "utils/config_parser.h"
#include <iostream>
#include <mutex>
#include <shared_mutex>

namespace core {

// ==========================================
// BankersMatrix Implementation
// ==========================================

bool BankersMatrix::is_safe_state() const {
    int num_processes = maximum.size();
    if (num_processes == 0) return true;
    int num_resources = available.size();

    std::vector<int> work = available;
    std::vector<bool> finish(num_processes, false);

    bool found;
    do {
        found = false;
        for (int p = 0; p < num_processes; ++p) {
            if (!finish[p]) {
                // Skip empty process slots caused by direct PID indexing
                if (maximum[p].empty()) {
                    finish[p] = true;
                    found = true;
                    continue;
                }

                bool can_allocate = true;
                for (int r = 0; r < num_resources; ++r) {
                    if (need[p][r] > work[r]) {
                        can_allocate = false;
                        break;
                    }
                }

                if (can_allocate) {
                    for (int r = 0; r < num_resources; ++r) {
                        work[r] += allocation[p][r];
                    }
                    finish[p] = true;
                    found = true;
                }
            }
        }
    } while (found);

    // If any process cannot finish, we are in an unsafe state
    for (bool f : finish) {
        if (!f) return false;
    }
    return true;
}

bool BankersMatrix::request_resources(int pid, const std::vector<int>& request) {
    if (pid >= maximum.size() || maximum[pid].empty() || request.size() > available.size()) {
        return false;
    }

    // Validate against Need and Available
    for (size_t i = 0; i < request.size(); ++i) {
        if (request[i] > need[pid][i]) return false; 
        if (request[i] > available[i]) return false; 
    }

    // Tentatively allocate
    for (size_t i = 0; i < request.size(); ++i) {
        available[i] -= request[i];
        allocation[pid][i] += request[i];
        need[pid][i] -= request[i];
    }

    // Run Safety Check
    if (is_safe_state()) {
        return true;
    } else {
        // Unsafe -> Revert matrices back to previous state
        for (size_t i = 0; i < request.size(); ++i) {
            available[i] += request[i];
            allocation[pid][i] -= request[i];
            need[pid][i] += request[i];
        }
        return false;
    }
}

void BankersMatrix::release_resources(int pid, const std::vector<int>& release) {
    if (pid >= allocation.size() || allocation[pid].empty() || release.size() > available.size()) {
        return;
    }

    for (size_t i = 0; i < release.size(); ++i) {
        if (release[i] > allocation[pid][i]) continue; // Prevent negative allocation anomaly

        available[i] += release[i];
        allocation[pid][i] -= release[i];
        need[pid][i] += release[i]; // Need conceptually increases as we release
    }
}


// ==========================================
// ConcurrencyManager Implementation
// ==========================================

// Pre-allocate a fixed size pool of KernelMutex objects to bypass std::mutex non-copyable vector limitations

ConcurrencyManager::ConcurrencyManager() : mutexes_(64) {
    try {
        auto config = utils::ConfigParser::parse("config.yaml");
        bankers_matrix_.available = config.resources.total_resources;
    } catch (const std::exception& e) {
        std::cerr << "ConcurrencyManager warning: " << e.what() << "\n";
    }
}

void ConcurrencyManager::register_process(ProcessControlBlock* pcb) {
    std::lock_guard<std::mutex> lock(cm_mutex_);
    pcbs_.push_back(pcb);

    int pid = pcb->get_pid();
    if (pid >= bankers_matrix_.maximum.size()) {
        bankers_matrix_.maximum.resize(pid + 1);
        bankers_matrix_.allocation.resize(pid + 1);
        bankers_matrix_.need.resize(pid + 1);
    }

    bankers_matrix_.maximum[pid] = pcb->get_max_resources();
    bankers_matrix_.allocation[pid] = pcb->get_allocated_resources();
    
    std::vector<int> need_vec(bankers_matrix_.maximum[pid].size(), 0);
    for (size_t i = 0; i < need_vec.size(); ++i) {
        need_vec[i] = bankers_matrix_.maximum[pid][i] - bankers_matrix_.allocation[pid][i];
    }
    bankers_matrix_.need[pid] = need_vec;

    // Deduct initial allocations from the available pool across the entire system
    for (size_t i = 0; i < bankers_matrix_.allocation[pid].size() && i < bankers_matrix_.available.size(); ++i) {
        bankers_matrix_.available[i] -= bankers_matrix_.allocation[pid][i];
    }
}

void ConcurrencyManager::apply_priority_boost(int target_pid, int new_priority) {
    ProcessControlBlock* target_pcb = nullptr;
    for (auto pcb : pcbs_) {
        if (pcb->get_pid() == target_pid) {
            target_pcb = pcb;
            break;
        }
    }
    if (!target_pcb) return;

    int boosted_prio = std::max(target_pcb->get_current_priority(), new_priority);
    if (boosted_prio > target_pcb->get_current_priority()) {
        target_pcb->set_current_priority(boosted_prio);
        int blocked_mid = target_pcb->get_blocked_on_mutex_id();
        if (blocked_mid != -1 && blocked_mid < static_cast<int>(mutexes_.size())) {
            int next_owner = mutexes_[blocked_mid].owner_pid.load();
            if (next_owner != -1 && next_owner != target_pid) {
                apply_priority_boost(next_owner, new_priority);
            }
        }
    }
}

bool ConcurrencyManager::try_acquire_mutex(int mutex_id, int pid) {
    std::lock_guard<std::mutex> lock(cm_mutex_);
    if (mutex_id < 0 || mutex_id >= static_cast<int>(mutexes_.size())) return false;

    KernelMutex& m = mutexes_[mutex_id];
    
    if (m.owner_pid == -1) {
        // Grant the lock directly
        m.owner_pid = pid;
        ProcessControlBlock* caller = nullptr;
        for (auto pcb : pcbs_) {
            if (pcb->get_pid() == pid) { caller = pcb; break; }
        }
        if (caller) {
            m.owner_base_priority = caller->get_base_priority();
            caller->add_held_mutex(mutex_id);
            caller->set_blocked_on_mutex_id(-1);
        }
        return true;
    } else {
        // Push caller to wait queue
        m.wait_queue.push(pid);
        
        ProcessControlBlock* caller = nullptr;
        for (auto pcb : pcbs_) {
            if (pcb->get_pid() == pid) { caller = pcb; break; }
        }
        
        if (caller) {
            caller->set_blocked_on_mutex_id(mutex_id);
            int owner_pid = m.owner_pid.load();
            if (owner_pid != -1 && owner_pid != pid) {
                apply_priority_boost(owner_pid, caller->get_current_priority());
            }
        }
        return false;
    }
}

void ConcurrencyManager::release_mutex(int mutex_id, int pid) {
    std::lock_guard<std::mutex> lock(cm_mutex_);
    if (mutex_id < 0 || mutex_id >= static_cast<int>(mutexes_.size())) return;

    KernelMutex& m = mutexes_[mutex_id];
    if (m.owner_pid != pid) return; // Only owner can release

    // Revert priority inheritance logic
    ProcessControlBlock* owner = nullptr;
    for (auto pcb : pcbs_) {
        if (pcb->get_pid() == pid) { owner = pcb; break; }
    }
    if (owner) {
        owner->set_current_priority(m.owner_base_priority);
        owner->remove_held_mutex(mutex_id);
    }

    if (!m.wait_queue.empty()) {
        // Pass lock explicitly to next in queue
        int next_pid = m.wait_queue.front();
        m.wait_queue.pop();
        
        m.owner_pid = next_pid;
        ProcessControlBlock* next_owner = nullptr;
        for (auto pcb : pcbs_) {
            if (pcb->get_pid() == next_pid) { next_owner = pcb; break; }
        }
        if (next_owner) {
            m.owner_base_priority = next_owner->get_base_priority();
            next_owner->set_state(ProcessState::READY);
            next_owner->add_held_mutex(mutex_id);
            next_owner->set_blocked_on_mutex_id(-1);
        }
    } else {
        // Mutex is now completely free
        m.owner_pid = -1;
    }
}

int ConcurrencyManager::get_mutex_owner(int mutex_id) const {
    std::lock_guard<std::mutex> lock(cm_mutex_);
    if (mutex_id < 0 || mutex_id >= static_cast<int>(mutexes_.size())) return -1;
    return mutexes_[mutex_id].owner_pid.load();
}

std::vector<int> ConcurrencyManager::get_mutex_waiters(int mutex_id) const {
    std::lock_guard<std::mutex> lock(cm_mutex_);
    if (mutex_id < 0 || mutex_id >= static_cast<int>(mutexes_.size())) return {};
    std::vector<int> res;
    std::queue<int> q_copy = mutexes_[mutex_id].wait_queue;
    while (!q_copy.empty()) {
        res.push_back(q_copy.front());
        q_copy.pop();
    }
    return res;
}

bool ConcurrencyManager::request_resources(int pid, const std::vector<int>& request) {
    std::lock_guard<std::mutex> lock(cm_mutex_);
    
    // Attempt bankers allocation
    bool granted = bankers_matrix_.request_resources(pid, request);
    if (granted) {
        // Mirror the state directly into the PCB so the UI can snapshot it
        for (auto pcb : pcbs_) {
            if (pcb->get_pid() == pid) {
                pcb->allocate_resources(request);
                break;
            }
        }
    }
    return granted;
}

void ConcurrencyManager::release_resources(int pid, const std::vector<int>& release) {
    std::lock_guard<std::mutex> lock(cm_mutex_);
    bankers_matrix_.release_resources(pid, release);
    
    for (auto pcb : pcbs_) {
        if (pcb->get_pid() == pid) {
            pcb->release_resources(release);
            break;
        }
    }
}

BankersMatrix ConcurrencyManager::get_bankers_matrix() const {
    std::lock_guard<std::mutex> lock(cm_mutex_);
    return bankers_matrix_;
}

} // namespace core
