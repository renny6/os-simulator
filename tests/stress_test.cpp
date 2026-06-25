#include "utils/config_parser.h"
#include "core/concurrency_manager.h"
#include "core/cpu_scheduler.h"
#include "core/process.h"
#include <iostream>
#include <cassert>
#include <vector>

namespace core { ConcurrencyManager* g_cm = nullptr; }

int main() {
    std::cout << "=========================================================\n";
    std::cout << "         Starting OS Simulator Stress & Verification Test\n";
    std::cout << "=========================================================\n";

    auto config = utils::ConfigParser::parse("config.yaml");
    
    core::ConcurrencyManager concurrency_manager;
    core::g_cm = &concurrency_manager;
    core::CpuScheduler scheduler;

    std::vector<core::ProcessControlBlock*> pcbs;
    for (const auto& pconf : config.processes) {
        auto pcb = std::make_unique<core::ProcessControlBlock>(pconf);
        pcbs.push_back(pcb.get());
        concurrency_manager.register_process(pcb.get());
        scheduler.add_process(std::move(pcb));
    }

    const int test_cycles = 500;
    std::cout << "Simulating " << test_cycles << " cycles under heavy stress load...\n";

    for (int cycle = 1; cycle <= test_cycles; ++cycle) {
        scheduler.tick();

        // Verification a) Resource Conservation Invariant
        core::BankersMatrix bm = concurrency_manager.get_bankers_matrix();
        std::vector<int> sum_allocated(bm.available.size(), 0);

        for (auto pcb : pcbs) {
            auto alloc = pcb->get_allocated_resources();
            for (size_t i = 0; i < alloc.size() && i < sum_allocated.size(); ++i) {
                sum_allocated[i] += alloc[i];
            }
        }

        auto total_resources = config.resources.total_resources;
        for (size_t i = 0; i < total_resources.size(); ++i) {
            assert((sum_allocated[i] + bm.available[i] == total_resources[i]) && 
                   "FATAL INVARIANT VIOLATION: Sum of allocated + available != total resources!");
        }

        // Verification b) Deadlock Liveness Check
        bool any_runnable = false;
        bool all_terminated = true;
        for (auto pcb : pcbs) {
            auto st = pcb->get_current_state();
            if (st != core::ProcessState::TERMINATED) {
                all_terminated = false;
            }
            if (st == core::ProcessState::READY || st == core::ProcessState::RUNNING) {
                any_runnable = true;
            }
        }

        if (!all_terminated && !any_runnable) {
            assert(bm.is_safe_state() && 
                   "DEADLOCK DETECTED: System in unsafe state with no runnable processes!");
        }
    }

    std::cout << "---------------------------------------------------------\n";
    std::cout << " [PASSED] Resource Conservation Invariant verified\n";
    std::cout << " [PASSED] Deadlock Liveness Invariant verified\n";
    std::cout << "=========================================================\n";
    std::cout << " ALL 500 STRESS TEST CYCLES COMPLETED SUCCESSFULLY!\n";
    std::cout << "=========================================================\n";

    return 0;
}
