#include "core/thrashing_detector.h"
#include "utils/config_parser.h"
#include <iostream>

namespace core {

ThrashingDetector::ThrashingDetector(memory::MemoryManagementUnit& mmu, CpuScheduler& scheduler)
    : mmu_(mmu), scheduler_(scheduler), thrashing_threshold_(0.8) {
    try {
        auto config = utils::ConfigParser::parse("config.yaml");
        thrashing_threshold_ = config.system.thrashing_fault_threshold;
    } catch (...) {
        thrashing_threshold_ = 0.8;
    }
}

void ThrashingDetector::on_memory_pressure(double fault_rate) {
    if (fault_rate >= thrashing_threshold_) {
        std::cout << "[Thrashing Detector ALERT] Page fault rate reached " 
                  << (fault_rate * 100.0) << "% (>= threshold " << (thrashing_threshold_ * 100.0) 
                  << "%). Engaging mitigation...\n";
        scheduler_.suspend_highest_burst_process();
    }
}

} // namespace core
