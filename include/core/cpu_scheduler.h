#pragma once

#include <vector>
#include <queue>
#include <memory>
#include "process.h"

namespace core {

class CpuScheduler {
public:
    CpuScheduler();

    // Disable copy semantics
    CpuScheduler(const CpuScheduler&) = delete;
    CpuScheduler& operator=(const CpuScheduler&) = delete;

    void add_process(std::unique_ptr<ProcessControlBlock> pcb);
    
    // Core scheduling loop logic
    void run_loop();
    void tick();
    void context_switch();

    // Thrashing mitigation helper
    void suspend_highest_burst_process();

private:
    ProcessControlBlock* select_next_process();

    // CpuScheduler owns PCBs via std::vector<std::unique_ptr<ProcessControlBlock>>
    std::vector<std::unique_ptr<ProcessControlBlock>> processes_;
    
    // Internal queues/structures to support RR and SRTF
    std::queue<ProcessControlBlock*> ready_queue_rr_;
    
    ProcessControlBlock* active_process_ = nullptr;
};

} // namespace core
