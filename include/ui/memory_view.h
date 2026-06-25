#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <vector>
#include "../memory/mmu.h"

namespace ui {

class MemoryView {
public:
    MemoryView();

    ftxui::Component get_component();

    // Method to receive and render the snapshot from the MMU
    void update_snapshot(const std::vector<memory::PageFrame>& frames);

private:
    std::vector<memory::PageFrame> frames_snapshot_;
};

} // namespace ui
