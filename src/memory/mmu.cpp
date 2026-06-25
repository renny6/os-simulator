#include "memory/mmu.h"
#include "memory/fifo_strategy.h"
#include "memory/lru_strategy.h"
#include "memory/clock_strategy.h"
#include "memory/opt_strategy.h"
#include <mutex>
#include <shared_mutex>

namespace memory {

MemoryManagementUnit::MemoryManagementUnit(int num_frames, const std::string& replacement_algorithm) 
    : current_page_references(nullptr), current_reference_index(0) {
    
    frames.resize(num_frames);
    for (int i = 0; i < num_frames; ++i) {
        frames[i].page_number = -1;
        frames[i].load_time = 0;
        frames[i].last_used = 0;
        frames[i].reference_bit = false;
        frames[i].flash_countdown = 0;
        frames[i].last_event_was_fault = false;
    }

    if (replacement_algorithm == "FIFO") {
        replacer = std::make_unique<FifoStrategy>();
    } else if (replacement_algorithm == "LRU") {
        replacer = std::make_unique<LruStrategy>();
    } else if (replacement_algorithm == "CLOCK") {
        replacer = std::make_unique<ClockStrategy>();
    } else if (replacement_algorithm == "OPT") {
        replacer = std::make_unique<OptStrategy>();
    } else {
        replacer = std::make_unique<FifoStrategy>(); // Fallback
    }
}

void MemoryManagementUnit::set_active_page_references(const std::vector<int>* page_refs, size_t current_index) {
    std::unique_lock<std::shared_mutex> lock(vmm_mutex);
    current_page_references = page_refs;
    current_reference_index = current_index;
    
    if (replacer->algorithm_name() == "OPT") {
        auto opt = static_cast<OptStrategy*>(replacer.get());
        opt->set_active_page_references(page_refs, current_index);
    }
}

bool MemoryManagementUnit::handle_page_access(int page_id, uint64_t access_counter) {
    std::unique_lock<std::shared_mutex> lock(vmm_mutex);
    
    // Scan for hit
    for (size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].page_number == page_id) {
            frames[i].last_used = access_counter;
            frames[i].reference_bit = true;
            frames[i].flash_countdown = 5;
            frames[i].last_event_was_fault = false;
            
            replacer->on_page_accessed(i, access_counter);
            return true; // Page Hit
        }
    }
    
    // Scan for an empty frame before evicting
    int target_frame = -1;
    for (size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].page_number == -1) {
            target_frame = i;
            break;
        }
    }

    // Replace if full
    if (target_frame == -1) {
        target_frame = replacer->select_victim(frames);
    }
    
    // Perform load
    if (target_frame >= 0 && target_frame < static_cast<int>(frames.size())) {
        frames[target_frame].page_number = page_id;
        frames[target_frame].load_time = access_counter;
        frames[target_frame].last_used = access_counter;
        frames[target_frame].reference_bit = true;
        frames[target_frame].flash_countdown = 5;
        frames[target_frame].last_event_was_fault = true;
        
        replacer->on_page_loaded(target_frame, access_counter);
    }
    
    return false; // Page Fault
}

std::vector<PageFrame> MemoryManagementUnit::get_frames_snapshot() const {
    std::shared_lock<std::shared_mutex> lock(vmm_mutex);
    return frames; // Return copy
}

} // namespace memory
