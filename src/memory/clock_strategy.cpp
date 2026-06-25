#include "memory/clock_strategy.h"

namespace memory {

int ClockStrategy::select_victim(const std::vector<PageFrame>& frames) {
    if (frames.empty()) return -1;
    
    if (clock_hand_ >= static_cast<int>(frames.size())) {
        clock_hand_ = 0;
    }

    // Cast away constness to modify reference_bit during search
    auto& mutable_frames = const_cast<std::vector<PageFrame>&>(frames);

    while (true) {
        if (!mutable_frames[clock_hand_].reference_bit) {
            int victim = clock_hand_;
            clock_hand_ = (clock_hand_ + 1) % frames.size();
            return victim;
        } else {
            // Second chance!
            mutable_frames[clock_hand_].reference_bit = false;
            clock_hand_ = (clock_hand_ + 1) % frames.size();
        }
    }
}

void ClockStrategy::on_page_accessed(int frame_index, uint64_t access_counter) {
    // MMU sets reference_bit to true directly
}

void ClockStrategy::on_page_loaded(int frame_index, uint64_t access_counter) {
    // MMU sets reference_bit to true directly
}

std::string ClockStrategy::algorithm_name() const {
    return "CLOCK";
}

} // namespace memory
