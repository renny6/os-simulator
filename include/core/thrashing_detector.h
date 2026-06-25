#pragma once

#include "memory/mmu.h"
#include "core/cpu_scheduler.h"

namespace core {

class ThrashingDetector {
public:
    ThrashingDetector(memory::MemoryManagementUnit& mmu, CpuScheduler& scheduler);

    // Callback invoked by MMU upon high fault rate
    void on_memory_pressure(double fault_rate);

private:
    memory::MemoryManagementUnit& mmu_;
    CpuScheduler& scheduler_;
    double thrashing_threshold_;
};

} // namespace core
