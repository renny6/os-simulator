#pragma once

#include "page_replacer.h"
#include "mmu.h"
#include <vector>

namespace memory {

class OptStrategy : public PageReplacer {
public:
    int select_victim(const std::vector<PageFrame>& frames) override;
    void on_page_accessed(int frame_index, uint64_t access_counter) override;
    void on_page_loaded(int frame_index, uint64_t access_counter) override;
    std::string algorithm_name() const override;

    // For the look-ahead mechanism
    void set_active_page_references(const std::vector<int>* refs, size_t current_index);

private:
    const std::vector<int>* active_page_references_ = nullptr;
    size_t current_reference_index_ = 0;
};

} // namespace memory
