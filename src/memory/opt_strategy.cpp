#include "memory/opt_strategy.h"
#include <string>

namespace memory {

int OptStrategy::select_victim(const std::vector<PageFrame>& frames) {
    if (frames.empty()) return -1;
    if (!active_page_references_) return 0; // Fallback if no references exist

    int victim = 0;
    size_t farthest_use = 0;

    for (size_t i = 0; i < frames.size(); ++i) {
        int page_num = frames[i].page_number;
        if (page_num == -1) {
            return i; // Always pick an empty frame first
        }

        size_t next_use = std::string::npos;
        // Search forward in the active process's remaining page references
        for (size_t j = current_reference_index_; j < active_page_references_->size(); ++j) {
            if ((*active_page_references_)[j] == page_num) {
                next_use = j;
                break;
            }
        }

        // If the page is never used again, it's the perfect victim
        if (next_use == std::string::npos) {
            return i; 
        }

        // Otherwise find the one used furthest in the future
        if (next_use > farthest_use) {
            farthest_use = next_use;
            victim = i;
        }
    }

    return victim;
}

void OptStrategy::on_page_accessed(int frame_index, uint64_t access_counter) {}

void OptStrategy::on_page_loaded(int frame_index, uint64_t access_counter) {}

std::string OptStrategy::algorithm_name() const {
    return "OPT";
}

void OptStrategy::set_active_page_references(const std::vector<int>* refs, size_t current_index) {
    active_page_references_ = refs;
    current_reference_index_ = current_index;
}

} // namespace memory
