#pragma once

#include "page_replacer.h"
#include "mmu.h"

namespace memory {

class LruStrategy : public PageReplacer {
public:
    int select_victim(const std::vector<PageFrame>& frames) override;
    void on_page_accessed(int frame_index, uint64_t access_counter) override;
    void on_page_loaded(int frame_index, uint64_t access_counter) override;
    std::string algorithm_name() const override;
};

} // namespace memory
