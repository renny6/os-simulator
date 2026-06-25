#include "../../include/ui/memory_view.h"

namespace ui {

MemoryView::MemoryView() {}

void MemoryView::update_snapshot(const std::vector<memory::PageFrame>& frames) {
    frames_snapshot_ = frames;
}

ftxui::Component MemoryView::get_component() {
    return ftxui::Renderer([this] {
        ftxui::Elements elements;
        elements.push_back(ftxui::text("Physical Page Frames") | ftxui::bold | ftxui::center);
        elements.push_back(ftxui::separator());
        
        ftxui::Elements rows;
        for (size_t i = 0; i < frames_snapshot_.size(); ++i) {
            const auto& frame = frames_snapshot_[i];
            std::string page_str = (frame.page_number == -1) ? "EMPTY" : std::to_string(frame.page_number);
            std::string ref_str = frame.reference_bit ? "1" : "0";
            
            auto cell = ftxui::hbox({
                ftxui::text(" F" + std::to_string(i) + " ") | ftxui::dim,
                ftxui::text(" P:" + page_str + " "),
                ftxui::text(" R:" + ref_str + " ")
            });

            if (frame.last_event_was_fault) {
                cell = cell | ftxui::bgcolor(ftxui::Color::Red) | ftxui::color(ftxui::Color::White);
            } else if (frame.page_number != -1) {
                cell = cell | ftxui::bgcolor(ftxui::Color::Blue) | ftxui::color(ftxui::Color::White);
            } else {
                cell = cell | ftxui::bgcolor(ftxui::Color::GrayDark);
            }
            
            rows.push_back(cell | ftxui::border);
        }
        
        elements.push_back(ftxui::vbox(std::move(rows)));
        return ftxui::window(ftxui::text(" Virtual Memory Manager "), ftxui::vbox(std::move(elements)));
    });
}

} // namespace ui
