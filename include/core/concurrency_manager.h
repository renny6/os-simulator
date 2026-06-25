#pragma once

#include <vector>
#include <mutex>
#include <atomic>
#include <queue>
#include "process.h"

namespace core {

// Synchronisation primitive wrapper
struct KernelMutex {
    std::mutex internal_mutex;
    std::atomic<int> owner_pid{-1}; // PID of holding thread (-1 if unlocked)
    int owner_base_priority;        // stored at lock time for inheritance revert
    std::queue<int> wait_queue;     // PIDs blocked waiting for this mutex
};

// Deadlock avoidance state
class BankersMatrix {
public:
    // O(n²) safety check, call only on resource request
    bool is_safe_state() const;
    
    bool request_resources(int pid, const std::vector<int>& request);
    void release_resources(int pid, const std::vector<int>& release);

    std::vector<int> available; // [R0_avail, R1_avail, R2_avail]
    std::vector<std::vector<int>> maximum; // max[pid][resource_type]
    std::vector<std::vector<int>> allocation; // allocation[pid][resource_type]
    std::vector<std::vector<int>> need; // need = maximum - allocation
};

class ConcurrencyManager {
public:
    ConcurrencyManager();

    // ConcurrencyManager holds non-owning raw pointers to PCBs (for priority manipulation)
    void register_process(ProcessControlBlock* pcb);

    // Thread-safe mechanisms to manage process states and avoid deadlocks
    bool try_acquire_mutex(int mutex_id, int pid);
    void release_mutex(int mutex_id, int pid);

    int get_mutex_owner(int mutex_id) const;
    std::vector<int> get_mutex_waiters(int mutex_id) const;

    bool request_resources(int pid, const std::vector<int>& request);
    void release_resources(int pid, const std::vector<int>& release);

    BankersMatrix get_bankers_matrix() const;

private:
    void apply_priority_boost(int target_pid, int new_priority);

    std::vector<ProcessControlBlock*> pcbs_;
    std::vector<KernelMutex> mutexes_;
    BankersMatrix bankers_matrix_;
    
    // KernelMutex wait queues (protected by ConcurrencyManager mutex)
    mutable std::mutex cm_mutex_;
};

} // namespace core
