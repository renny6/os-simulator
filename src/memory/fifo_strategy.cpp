#include "memory/fifo_strategy.h"

namespace memory {

int FifoStrategy::select_victim(const std::vector<PageFrame>& frames) {
    if (frames.empty()) return -1;
    
    int victim = 0;
    uint64_t oldest = frames[0].load_time;
    
    for (size_t i = 1; i < frames.size(); ++i) {
        if (frames[i].load_time < oldest) {
            oldest = frames[i].load_time;
            victim = i;
        }
    }
    return victim;
}

void FifoStrategy::on_page_accessed(int frame_index, uint64_t access_counter) {
    // FIFO eviction doesn't change on arbitrary access
}

void FifoStrategy::on_page_loaded(int frame_index, uint64_t access_counter) {
    // The load_time is updated by MMU
}

std::string FifoStrategy::algorithm_name() const {
    return "FIFO";
}

} // namespace memory
