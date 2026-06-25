#include "core/concurrency_manager.h"
#include "core/process.h"
#include "utils/config_parser.h"
#include <iostream>
#include <cassert>
#include <memory>
#include <vector>

int main() {
    std::cout << "=========================================================\n";
    std::cout << "    Starting Transitive Priority Inheritance Verif. Suite\n";
    std::cout << "=========================================================\n";

    core::ConcurrencyManager cm;

    // 1. Setup ProcessConfigs (field order: pid, name, burst_time, priority, arrival_cycle, ...)
    utils::ProcessConfig c1{1, "P1_High", 10, 5, 0, {}, {1, 1}, {0, 0}};
    utils::ProcessConfig c2{2, "P2_Med",  10, 3, 0, {}, {1, 1}, {0, 0}};
    utils::ProcessConfig c3{3, "P3_Low",  10, 1, 0, {}, {1, 1}, {0, 0}};

    auto p1 = std::make_unique<core::ProcessControlBlock>(c1);
    auto p2 = std::make_unique<core::ProcessControlBlock>(c2);
    auto p3 = std::make_unique<core::ProcessControlBlock>(c3);

    cm.register_process(p1.get());
    cm.register_process(p2.get());
    cm.register_process(p3.get());

    std::cout << "[Setup] Initialized P1 (High: 5), P2 (Med: 3), P3 (Low: 1)\n";

    // --- Step 1: Setup Multi-Level Dependency Chain ---
    // P2 (Med) acquires Mutex 2
    bool acq2 = cm.try_acquire_mutex(2, 2);
    assert(acq2 && "P2 failed to acquire Mutex 2");
    std::cout << "[Step 1] P2 (Med) acquired Mutex 2.\n";

    // P3 (Low) acquires Mutex 1
    bool acq1 = cm.try_acquire_mutex(1, 3);
    assert(acq1 && "P3 failed to acquire Mutex 1");
    std::cout << "[Step 2] P3 (Low) acquired Mutex 1.\n";

    // P3 (Low) attempts to acquire Mutex 2 -> Blocks on P2
    bool acq3_on_2 = cm.try_acquire_mutex(2, 3);
    assert(!acq3_on_2 && "P3 should have blocked waiting for Mutex 2");
    assert(p3->get_blocked_on_mutex_id() == 2 && "P3 blocked_on_mutex_id tracking mismatch");
    std::cout << "[Step 3] P3 attempted Mutex 2 -> Blocked waiting for P2 (Med).\n";

    // P1 (High) attempts to acquire Mutex 1 -> Blocks on P3
    std::cout << "[Step 4] P1 (High: 5) attempting Mutex 1 (owned by P3)...\n";
    bool acq1_on_1 = cm.try_acquire_mutex(1, 1);
    assert(!acq1_on_1 && "P1 should have blocked waiting for Mutex 1");

    // --- Step 2: Assertions (Direct & Transitive Propagation) ---
    std::cout << "\n[Verifying Invariants] Inspecting boosted priorities...\n";
    std::cout << "  -> P3 Current Priority: " << p3->get_current_priority() << " (Expected: 5)\n";
    assert(p3->get_current_priority() == 5 && 
           "DIRECT INHERITANCE FAILED: P3 priority was not boosted to 5!");

    std::cout << "  -> P2 Current Priority: " << p2->get_current_priority() << " (Expected: 5)\n";
    assert(p2->get_current_priority() == 5 && 
           "TRANSITIVE INHERITANCE FAILED: Boost did not propagate across dependency chain to P2!");

    std::cout << " [PASSED] Direct Boost (P1->P3) and Transitive Boost (P3->P2) verified!\n\n";

    // --- Step 3: Cleanup & Restoration ---
    std::cout << "[Step 5] P2 releases Mutex 2 -> P3 unblocks & inherits M2.\n";
    cm.release_mutex(2, 2);
    assert(p2->get_current_priority() == 3 && "P2 did not revert back to base priority 3!");

    std::cout << "[Step 6] P3 releases Mutex 1 -> P3 reverts to base priority 1.\n";
    cm.release_mutex(1, 3);
    std::cout << "  -> P3 Current Priority: " << p3->get_current_priority() << " (Expected: 1)\n";
    assert(p3->get_current_priority() == 1 && "REVERSION FAILED: P3 priority did not revert to 1!");

    std::cout << "=========================================================\n";
    std::cout << " ALL PRIORITY INHERITANCE PROTOCOL ASSERTIONS PASSED!\n";
    std::cout << "=========================================================\n";

    return 0;
}
