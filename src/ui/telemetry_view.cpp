#include "../../include/ui/telemetry_view.h"

namespace ui {

TelemetryView::TelemetryView() {}

void TelemetryView::update_snapshot(const utils::CycleMetrics& metrics) {
    current_metrics_ = metrics;
}

ftxui::Component TelemetryView::get_component() {
    return ftxui::Renderer([this] {
        ftxui::Elements elements;
        
        elements.push_back(ftxui::text("System Health Telemetry") | ftxui::bold | ftxui::center);
        elements.push_back(ftxui::separator());
        
        elements.push_back(ftxui::text("Cycle ID: " + std::to_string(current_metrics_.cycle_id)));
        elements.push_back(ftxui::text("Active PID: " + std::to_string(current_metrics_.active_pid)));
        elements.push_back(ftxui::text("CPU Utilization: " + std::to_string((int)current_metrics_.cpu_utilization_percent) + "%"));
        elements.push_back(ftxui::text("Page Faults: " + std::to_string(current_metrics_.total_page_faults)));
        elements.push_back(ftxui::text("Frames in Use: " + std::to_string(current_metrics_.frames_in_use)));
        elements.push_back(ftxui::text("Deadlocks Prevented: " + std::to_string(current_metrics_.deadlocks_prevented)));
        
        if (current_metrics_.thrashing_detected) {
            elements.push_back(ftxui::text("ALERT: THRASHING DETECTED") | ftxui::color(ftxui::Color::Red) | ftxui::bold | ftxui::blink);
        }
        if (current_metrics_.belady_anomaly_detected) {
            elements.push_back(ftxui::text("ALERT: BELADY ANOMALY DETECTED") | ftxui::color(ftxui::Color::Yellow) | ftxui::bold);
        }

        return ftxui::window(ftxui::text(" Telemetry & Metrics "), ftxui::vbox(std::move(elements)));
    });
}

} // namespace ui
