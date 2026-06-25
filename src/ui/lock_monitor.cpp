#include "../../include/ui/lock_monitor.h"

namespace ui {

LockMonitor::LockMonitor() {}

void LockMonitor::update_bankers_snapshot(const BankersSnapshot& snapshot) {
    bankers_snapshot_ = snapshot;
}

void LockMonitor::update_mutex_snapshot(const std::vector<MutexSnapshot>& snapshot) {
    mutex_snapshots_ = snapshot;
}

ftxui::Component LockMonitor::get_component() {
    return ftxui::Renderer([this] {
        // Bankers Matrix Section
        ftxui::Elements bankers_elements;
        bankers_elements.push_back(ftxui::text("Banker's Algorithm State") | ftxui::bold);
        
        std::string avail_str = "Available: [";
        for (int v : bankers_snapshot_.available) avail_str += std::to_string(v) + " ";
        avail_str += "]";
        bankers_elements.push_back(ftxui::text(avail_str) | ftxui::color(ftxui::Color::Green));

        ftxui::Elements pid_rows;
        for (size_t p = 0; p < bankers_snapshot_.maximum.size(); ++p) {
            if (bankers_snapshot_.maximum[p].empty()) continue;
            
            std::string row_str = "PID " + std::to_string(p) + " | Alloc: [";
            for (int v : bankers_snapshot_.allocation[p]) row_str += std::to_string(v) + " ";
            row_str += "] Max: [";
            for (int v : bankers_snapshot_.maximum[p]) row_str += std::to_string(v) + " ";
            row_str += "] Need: [";
            for (int v : bankers_snapshot_.need[p]) row_str += std::to_string(v) + " ";
            row_str += "]";
            pid_rows.push_back(ftxui::text(row_str));
        }
        if (pid_rows.empty()) pid_rows.push_back(ftxui::text("No active claims") | ftxui::dim);
        
        bankers_elements.push_back(ftxui::vbox(std::move(pid_rows)) | ftxui::border);

        // Mutexes Section
        ftxui::Elements mutex_elements;
        mutex_elements.push_back(ftxui::text("Kernel Mutexes") | ftxui::bold);
        ftxui::Elements m_rows;
        for (size_t i = 0; i < mutex_snapshots_.size(); ++i) {
            const auto& m = mutex_snapshots_[i];
            if (m.owner_pid == -1 && m.wait_queue.empty()) continue; // Hide unused
            
            std::string state = (m.owner_pid == -1) ? "UNLOCKED" : "LOCKED by PID " + std::to_string(m.owner_pid);
            std::string wait_q = " WaitQ: [";
            for (int w : m.wait_queue) wait_q += std::to_string(w) + " ";
            wait_q += "]";
            
            m_rows.push_back(ftxui::text("M" + std::to_string(i) + ": " + state + wait_q));
        }
        if (m_rows.empty()) m_rows.push_back(ftxui::text("No active mutexes") | ftxui::dim);
        
        mutex_elements.push_back(ftxui::vbox(std::move(m_rows)) | ftxui::border);

        return ftxui::window(ftxui::text(" Concurrency Manager "), 
            ftxui::vbox({
                ftxui::vbox(std::move(bankers_elements)),
                ftxui::separator(),
                ftxui::vbox(std::move(mutex_elements))
            })
        );
    });
}

} // namespace ui
