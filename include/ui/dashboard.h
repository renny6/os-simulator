#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace ui {

class Dashboard {
public:
    Dashboard();

    // The main FTXUI component that composes the other views
    ftxui::Component get_component();

    // Method to trigger UI updates when the backend simulation ticks
    void trigger_update();

private:
    ftxui::Component main_container_;
};

} // namespace ui
