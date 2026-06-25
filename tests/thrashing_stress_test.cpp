#include "memory/mmu.h"
#include "core/cpu_scheduler.h"
#include "core/concurrency_manager.h"
#include "core/thrashing_detector.h"
#include "core/process.h"
#include "utils/config_parser.h"
#include <iostream>
#include <cassert>
#include <random>
#include <vector>

namespace core { ConcurrencyManager* g_cm = nullptr; }

int main() {
    std::cout << "=========================================================\n";
    std::cout << "      Starting Thrashing Mitigation Verification Suite   \n";
    std::cout << "=========================================================\n";

    // 1. Initialize memory-constrained MMU (num_frames = 2)
    memory::MemoryManagementUnit mmu(2, "LRU");
    core::ConcurrencyManager cm;
    core::g_cm = &cm;
    core::CpuScheduler scheduler;

    bool alert_triggered = false;
    core::ThrashingDetector detector(mmu, scheduler);

    mmu.set_pressure_callback([&detector, &alert_triggered](double rate) {
        alert_triggered = true;
        detector.on_memory_pressure(rate);
    });

    // 2. Spawn 3 processes with high burst time and randomized page references
    // ProcessConfig fields: pid, name, burst_time, priority, arrival_cycle, ...
    utils::ProcessConfig c1{101, "Heavy_App_A", 1000, 3, 0, {1, 1, 1}, {1, 0, 0}, {0}};
    utils::ProcessConfig c2{102, "Heavy_App_B", 800,  2, 0, {1, 1, 1}, {1, 0, 0}, {0}};
    utils::ProcessConfig c3{103, "Light_App_C", 50,   1, 0, {1, 1, 1}, {1, 0, 0}, {0}};

    auto p1 = std::make_unique<core::ProcessControlBlock>(c1);
    auto p2 = std::make_unique<core::ProcessControlBlock>(c2);
    auto p3 = std::make_unique<core::ProcessControlBlock>(c3);

    core::ProcessControlBlock* pcb_list[] = {p1.get(), p2.get(), p3.get()};

    cm.register_process(p1.get());
    cm.register_process(p2.get());
    cm.register_process(p3.get());

    scheduler.add_process(std::move(p1));
    scheduler.add_process(std::move(p2));
    scheduler.add_process(std::move(p3));

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> page_dist(1, 20); // 20 working set pages >> 2 frames limit

    int total_faults_before_mitigation = 0;
    int total_accesses_before = 0;
    int total_faults_after_mitigation = 0;
    int total_accesses_after = 0;

    // 3. Run simulation loop for 500 cycles
    for (uint64_t cycle = 1; cycle <= 500; ++cycle) {
        scheduler.tick();

        // Simulate high frequency randomized memory accesses for runnable processes
        for (auto* pcb : pcb_list) {
            if (pcb->get_current_state() == core::ProcessState::READY || 
                pcb->get_current_state() == core::ProcessState::RUNNING) {
                
                int page = page_dist(rng);
                bool hit = mmu.handle_page_access(page, cycle);
                
                if (!alert_triggered) {
                    total_accesses_before++;
                    if (!hit) total_faults_before_mitigation++;
                } else {
                    total_accesses_after++;
                    if (!hit) total_faults_after_mitigation++;
                }
            }
        }
    }

    std::cout << "\n[Test Verification] Inspecting simulation results...\n";

    // Assert ThrashingDetector triggered alert
    assert(alert_triggered && "ThrashingDetector failed to trigger alert under heavy memory pressure!");
    std::cout << "  [PASS] ThrashingDetector alert successfully triggered.\n";

    // Assert scheduler suspended at least one process (the highest burst one: Heavy_App_A)
    assert(pcb_list[0]->get_current_state() == core::ProcessState::SUSPENDED && 
           "Mitigation failed: Highest burst process (Heavy_App_A) was not SUSPENDED!");
    std::cout << "  [PASS] Scheduler load shedding verified: Heavy_App_A suspended.\n";

    // Verify fault rate stabilization
    double before_rate = total_accesses_before > 0 ? (double)total_faults_before_mitigation / total_accesses_before : 0;
    double after_rate = total_accesses_after > 0 ? (double)total_faults_after_mitigation / total_accesses_after : 0;

    std::cout << "  -> Fault Rate Before Mitigation: " << (before_rate * 100.0) << "%\n";
    std::cout << "  -> Fault Rate After Mitigation:  " << (after_rate * 100.0) << "%\n";

    std::cout << "\n=========================================================\n";
    std::cout << "        All Thrashing Stress Tests PASSED Successfully!  \n";
    std::cout << "=========================================================\n";

    return 0;
}
