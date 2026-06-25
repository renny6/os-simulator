#include "../../include/memory/lru_strategy.h"

namespace memory {

int LruStrategy::select_victim(const std::vector<PageFrame>& frames) {
    if (frames.empty()) return -1;
    
    int victim = 0;
    uint64_t oldest = frames[0].last_used;
    
    for (size_t i = 1; i < frames.size(); ++i) {
        if (frames[i].last_used < oldest) {
            oldest = frames[i].last_used;
            victim = i;
        }
    }
    return victim;
}

void LruStrategy::on_page_accessed(int frame_index, uint64_t access_counter) {
    // MMU directly updates last_used timestamp
}

void LruStrategy::on_page_loaded(int frame_index, uint64_t access_counter) {
    // MMU directly updates last_used timestamp
}

std::string LruStrategy::algorithm_name() const {
    return "LRU";
}

} // namespace memory
