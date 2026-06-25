#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace memory {

// Forward declaration of PageFrame located in mmu.h
struct PageFrame;

class PageReplacer {
public:
    virtual ~PageReplacer() = default;

    virtual int select_victim(const std::vector<PageFrame>& frames) = 0;
    virtual void on_page_accessed(int frame_index, uint64_t access_counter) = 0;
    virtual void on_page_loaded(int frame_index, uint64_t access_counter) = 0;
    virtual std::string algorithm_name() const = 0;
};

} // namespace memory
