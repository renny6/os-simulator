#include "include/utils/config_parser.h"
#include "include/utils/telemetry.h"
#include "include/memory/mmu.h"
#include "include/core/concurrency_manager.h"
#include "include/core/cpu_scheduler.h"
#include "include/core/process.h"
#include "include/ui/memory_view.h"
#include "include/ui/lock_monitor.h"
#include "include/ui/telemetry_view.h"
#include "include/ui/dashboard.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <iostream>

std::atomic<bool> simulation_running{true};

void backend_loop(
    core::CpuScheduler& scheduler,
    memory::MemoryManagementUnit& mmu,
    core::ConcurrencyManager& concurrency,
    utils::TelemetryLogger& telemetry,
    int tick_interval_ms,
    ui::MemoryView& mem_view,
    ui::LockMonitor& lock_monitor,
    ui::TelemetryView& tel_view,
    ftxui::ScreenInteractive& screen
) {
    uint64_t cycle_id = 0;
    
    while (simulation_running) {
        scheduler.tick();
        
        utils::CycleMetrics metrics;
        metrics.cycle_id = cycle_id++;
        metrics.active_pid = -1; // Placeholder as header didn't expose active process
        metrics.cpu_utilization_percent = 100.0f; 
        metrics.total_page_faults = 0;
        metrics.total_tlb_hits = 0;
        metrics.total_tlb_misses = 0;
        metrics.deadlocks_prevented = 0;
        metrics.frames_in_use = 0;
        metrics.belady_anomaly_detected = false;
        metrics.thrashing_detected = false;
        
        telemetry.push(metrics);

        // Update UI Snapshots via thread-safe read locks
        mem_view.update_snapshot(mmu.get_frames_snapshot());
        tel_view.update_snapshot(metrics);
        
        ui::BankersSnapshot b_snap;
        lock_monitor.update_bankers_snapshot(b_snap);
        lock_monitor.update_mutex_snapshot({});

        // Safely trigger a re-render on the main thread
        screen.PostEvent(ftxui::Event::Custom);

        std::this_thread::sleep_for(std::chrono::milliseconds(tick_interval_ms));
    }
}

int main() {
    try {
        auto config = utils::ConfigParser::parse("config.yaml");

        utils::TelemetryLogger telemetry(config.system.output_csv_path, config.system.telemetry_ring_buffer_size);
        memory::MemoryManagementUnit mmu(config.system.num_frames, config.system.page_replacement_algorithm);
        core::ConcurrencyManager concurrency_manager;
        core::CpuScheduler scheduler;

        for (const auto& pconf : config.processes) {
            auto pcb = std::make_unique<core::ProcessControlBlock>(pconf);
            concurrency_manager.register_process(pcb.get());
            scheduler.add_process(std::move(pcb));
        }

        ui::MemoryView memory_view;
        ui::LockMonitor lock_monitor;
        ui::TelemetryView telemetry_view;
        
        // Unified UI Composition
        auto main_layout = ftxui::Container::Horizontal({
            memory_view.get_component(),
            lock_monitor.get_component(),
            telemetry_view.get_component()
        });
        
        auto renderer = ftxui::Renderer(main_layout, [&] {
            return ftxui::hbox({
                memory_view.get_component()->Render() | ftxui::flex,
                ftxui::separator(),
                lock_monitor.get_component()->Render() | ftxui::flex,
                ftxui::separator(),
                telemetry_view.get_component()->Render() | ftxui::flex
            }) | ftxui::border;
        });

        auto screen = ftxui::ScreenInteractive::Fullscreen();

        // Launch backend simulation
        std::thread backend_thread(backend_loop, 
            std::ref(scheduler), std::ref(mmu), std::ref(concurrency_manager), std::ref(telemetry), 
            config.system.tick_interval_ms,
            std::ref(memory_view), std::ref(lock_monitor), std::ref(telemetry_view), std::ref(screen)
        );

        // Blocks until user exits UI (e.g. via 'q' or escape)
        screen.Loop(renderer);

        // Graceful teardown
        simulation_running = false;
        backend_thread.join();

    } catch (const std::exception& e) {
        std::cerr << "Simulator Fatal Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
