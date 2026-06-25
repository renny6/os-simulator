#include "../../include/core/cpu_scheduler.h"
#include "../../include/utils/config_parser.h"
#include <iostream>

namespace core {

// Static configuration cache since the schema prevents us from adding 
// these fields to the header. We parse once upon first use.
static std::string g_scheduling_algorithm = "RR";
static int g_time_slice_quantum = 3;
static bool g_config_loaded = false;

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
    
    ProcessControlBlock* raw_ptr = pcb.get();
    processes_.push_back(std::move(pcb));
    
    if (g_scheduling_algorithm == "RR") {
        ready_queue_rr_.push(raw_ptr);
    }
}

ProcessControlBlock* CpuScheduler::select_next_process() {
    load_config_if_needed();

    if (g_scheduling_algorithm == "RR") {
        if (ready_queue_rr_.empty()) {
            return nullptr;
        }
        ProcessControlBlock* next = ready_queue_rr_.front();
        ready_queue_rr_.pop();
        return next;
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
    // Pre-empt the active process if it's still running
    if (active_process_) {
        if (active_process_->get_current_state() == ProcessState::RUNNING) {
            active_process_->set_state(ProcessState::READY);
            
            // Re-queue under Round Robin
            if (g_scheduling_algorithm == "RR") {
                ready_queue_rr_.push(active_process_);
            }
        }
    }
    
    active_process_ = select_next_process();
    
    if (active_process_) {
        active_process_->set_state(ProcessState::RUNNING);
    }
}

void CpuScheduler::tick() {
    // Persist slice ticks across consecutive ticks
    static int current_slice_ticks = 0;
    
    load_config_if_needed();

    // If there is no active process, attempt to schedule one
    if (!active_process_) {
        context_switch();
        current_slice_ticks = 0;
    }

    // If still no process, the system is idle
    if (!active_process_) {
        return; 
    }

    // Execute the process for 1 simulated tick
    active_process_->decrement_remaining_burst_time();
    current_slice_ticks++;

    // Terminate if burst time reaches zero
    if (active_process_->get_remaining_burst_time() <= 0) {
        active_process_->set_state(ProcessState::TERMINATED);
        active_process_ = nullptr;
        current_slice_ticks = 0;
        
        // Immediately fetch the next process
        context_switch();
        return;
    }

    // Handle Pre-emption
    if (g_scheduling_algorithm == "RR") {
        if (current_slice_ticks >= g_time_slice_quantum) {
            current_slice_ticks = 0;
            context_switch();
        }
    } 
    else if (g_scheduling_algorithm == "SRTF") {
        // Under SRTF, peek at the queue to see if a shorter process arrived or became ready
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
