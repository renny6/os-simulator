#include "utils/telemetry.h"
#include <fstream>
#include <iostream>

namespace utils {

TelemetryLogger::TelemetryLogger(const std::string& output_csv_path, size_t max_buffer_size)
    : max_buffer_size_(max_buffer_size), stop_flag_(false) {
    
    out_stream_.open(output_csv_path, std::ios::out | std::ios::trunc);
    if (!out_stream_.is_open()) {
        std::cerr << "Failed to open telemetry output file: " << output_csv_path << "\n";
    } else {
        // Write CSV header based on the CycleMetrics fields. 
        // Note: although the implementation plan briefly mentioned a "3-column CSV header", 
        // we are accurately serializing the full 10 fields defined in the schema for CycleMetrics.
        out_stream_ << "cycle_id,active_pid,cpu_utilization_percent,total_page_faults,"
                    << "total_tlb_hits,total_tlb_misses,deadlocks_prevented,frames_in_use,"
                    << "belady_anomaly_detected,thrashing_detected\n";
    }

    writer_thread_ = std::thread(&TelemetryLogger::writer_loop, this);
}

TelemetryLogger::~TelemetryLogger() {
    stop_flag_ = true;
    cv_.notify_one();
    
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
    
    // Flush any remaining items left in the buffer before shutting down
    std::deque<CycleMetrics> remaining_buffer;
    {
        std::lock_guard<std::mutex> lock(telemetry_mutex_);
        remaining_buffer.swap(buffer_);
    }

    if (out_stream_.is_open() && !remaining_buffer.empty()) {
        for (const auto& metrics : remaining_buffer) {
            out_stream_ << metrics.cycle_id << ","
                        << metrics.active_pid << ","
                        << metrics.cpu_utilization_percent << ","
                        << metrics.total_page_faults << ","
                        << metrics.total_tlb_hits << ","
                        << metrics.total_tlb_misses << ","
                        << metrics.deadlocks_prevented << ","
                        << metrics.frames_in_use << ","
                        << (metrics.belady_anomaly_detected ? 1 : 0) << ","
                        << (metrics.thrashing_detected ? 1 : 0) << "\n";
        }
        out_stream_.flush();
    }
    
    if (out_stream_.is_open()) {
        out_stream_.close();
    }
}

void TelemetryLogger::push(const CycleMetrics& metrics) {
    {
        std::lock_guard<std::mutex> lock(telemetry_mutex_);
        if (buffer_.size() >= max_buffer_size_) {
            buffer_.pop_front();
        }
        buffer_.push_back(metrics);
    }
    cv_.notify_one();
}

void TelemetryLogger::writer_loop() {
    while (!stop_flag_) {
        std::deque<CycleMetrics> local_buffer;
        {
            std::unique_lock<std::mutex> lock(telemetry_mutex_);
            cv_.wait(lock, [this]() { return !buffer_.empty() || stop_flag_; });
            
            local_buffer.swap(buffer_);
        }

        if (out_stream_.is_open() && !local_buffer.empty()) {
            for (const auto& metrics : local_buffer) {
                out_stream_ << metrics.cycle_id << ","
                            << metrics.active_pid << ","
                            << metrics.cpu_utilization_percent << ","
                            << metrics.total_page_faults << ","
                            << metrics.total_tlb_hits << ","
                            << metrics.total_tlb_misses << ","
                            << metrics.deadlocks_prevented << ","
                            << metrics.frames_in_use << ","
                            << (metrics.belady_anomaly_detected ? 1 : 0) << ","
                            << (metrics.thrashing_detected ? 1 : 0) << "\n";
            }
            out_stream_.flush();
        }
    }
}

} // namespace utils
