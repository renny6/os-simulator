#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include "../utils/telemetry.h"

namespace ui {

class TelemetryView {
public:
    TelemetryView();

    ftxui::Component get_component();

    // Reads from a snapshot of the CycleMetrics generated in Step 4
    void update_snapshot(const utils::CycleMetrics& metrics);

private:
    utils::CycleMetrics current_metrics_;
};

} // namespace ui
