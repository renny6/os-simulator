#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <vector>

namespace ui {

// Minimal snapshot structures for UI rendering to avoid touching backend state
struct BankersSnapshot {
    std::vector<int> available;
    std::vector<std::vector<int>> maximum;
    std::vector<std::vector<int>> allocation;
    std::vector<std::vector<int>> need;
};

struct MutexSnapshot {
    int owner_pid;
    std::vector<int> wait_queue;
};

class LockMonitor {
public:
    LockMonitor();

    ftxui::Component get_component();

    // Receives backend snapshots for rendering
    void update_bankers_snapshot(const BankersSnapshot& snapshot);
    void update_mutex_snapshot(const std::vector<MutexSnapshot>& snapshot);

private:
    BankersSnapshot bankers_snapshot_;
    std::vector<MutexSnapshot> mutex_snapshots_;
};

} // namespace ui
