#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <string>
#include <fstream>
#include <atomic>
#include <algorithm>

namespace utils {

/**
 * @brief Telemetry record capturing the state of the system at the end of a simulated cycle.
 */
struct CycleMetrics {
    uint64_t cycle_id;
    int active_pid; // PID of process that ran this cycle (-1 if idle)
    float cpu_utilization_percent;
    int total_page_faults; // cumulative
    int total_tlb_hits;    // cumulative
    int total_tlb_misses;  // cumulative
    int deadlocks_prevented; // cumulative
    int frames_in_use; // at end of this cycle
    bool belady_anomaly_detected; // true if FIFO faults increased with more frames
    bool thrashing_detected; // true if fault rate exceeded threshold this cycle
};

/**
 * @brief Standalone helper function to calculate the moving average of page faults over a given window.
 * 
 * @param history Collection of historical cycle metrics.
 * @param window_size Number of recent cycles to average over.
 * @return double Moving average of page faults.
 */
inline double calculate_moving_average(const std::deque<CycleMetrics>& history, size_t window_size) {
    if (history.empty() || window_size == 0) return 0.0;
    size_t count = std::min(window_size, history.size());
    double sum = 0.0;
    for (size_t i = history.size() - count; i < history.size(); ++i) {
        sum += history[i].total_page_faults;
    }
    return sum / count;
}

/**
 * @brief Thread-safe logger that stores telemetry in a ring buffer and writes incrementally to a CSV.
 */
class TelemetryLogger {
public:
    TelemetryLogger(const std::string& output_csv_path, size_t max_buffer_size);
    ~TelemetryLogger();

    TelemetryLogger(const TelemetryLogger&) = delete;
    TelemetryLogger& operator=(const TelemetryLogger&) = delete;

    void push(const CycleMetrics& metrics);

    /**
     * @brief Calculates moving average of total_page_faults across retained history.
     */
    double calculate_moving_average(size_t window_size) const;

    /**
     * @brief Returns a snapshot of retained history records.
     */
    std::deque<CycleMetrics> get_history() const;

private:
    void writer_loop();

    std::deque<CycleMetrics> history_buffer_;
    std::deque<CycleMetrics> write_queue_;
    size_t max_buffer_size_;
    std::ofstream out_stream_;

    mutable std::mutex telemetry_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_flag_;
    std::thread writer_thread_;
};

} // namespace utils
