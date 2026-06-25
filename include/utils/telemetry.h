#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <string>
#include <fstream>
#include <atomic>

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
 * @brief Thread-safe logger that stores telemetry in a ring buffer and writes incrementally to a CSV.
 */
class TelemetryLogger {
public:
    /**
     * @brief Constructs the telemetry logger, opens the output file, and starts the writer thread.
     * 
     * @param output_csv_path Path to the CSV file where metrics will be recorded.
     * @param max_buffer_size Maximum number of items in the ring buffer before oldest are discarded.
     */
    TelemetryLogger(const std::string& output_csv_path, size_t max_buffer_size);

    /**
     * @brief Destructor gracefully stops the writer loop and flushes remaining buffer contents.
     */
    ~TelemetryLogger();

    // Disable copy semantics to manage thread and file resources safely
    TelemetryLogger(const TelemetryLogger&) = delete;
    TelemetryLogger& operator=(const TelemetryLogger&) = delete;

    /**
     * @brief Safely adds a metrics record to the ring buffer and notifies the writer thread.
     * 
     * @param metrics The metrics to record for the cycle.
     */
    void push(const CycleMetrics& metrics);

private:
    /**
     * @brief Background writer loop that drains the buffer and writes to the CSV incrementally.
     */
    void writer_loop();

    std::deque<CycleMetrics> buffer_;
    size_t max_buffer_size_;
    std::ofstream out_stream_;

    std::mutex telemetry_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_flag_;
    std::thread writer_thread_;
};

} // namespace utils
