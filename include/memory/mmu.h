#pragma once

#include <vector>
#include <memory>
#include <shared_mutex>
#include <cstdint>
#include <string>
#include "page_replacer.h"

namespace memory {

struct PageFrame {
    int page_number;
    uint64_t load_time;
    uint64_t last_used;
    bool reference_bit;
    int flash_countdown;
    bool last_event_was_fault;
};

class MemoryManagementUnit {
public:
    explicit MemoryManagementUnit(int num_frames, const std::string& replacement_algorithm);

    // Disable copy semantics
    MemoryManagementUnit(const MemoryManagementUnit&) = delete;
    MemoryManagementUnit& operator=(const MemoryManagementUnit&) = delete;

    // Provide the current active process's page reference string to the MMU
    void set_active_page_references(const std::vector<int>* page_refs, size_t current_index);

    // Core memory access logic
    bool handle_page_access(int page_id, uint64_t access_counter);

    // Snapshots for the UI thread
    std::vector<PageFrame> get_frames_snapshot() const;

private:
    std::vector<PageFrame> frames;
    std::unique_ptr<PageReplacer> replacer;
    const std::vector<int>* current_page_references;
    size_t current_reference_index;

    mutable std::shared_mutex vmm_mutex;
};

} // namespace memory
