#include "logging_system.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace NeonOubliette {

LoggingSystem::LoggingSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry(registry), dispatcher(dispatcher) {
    dispatcher.sink<LogEvent>().connect<&LoggingSystem::handleLogEvent>(*this);
}

void LoggingSystem::update(double delta_time) {
    flush_accumulator_seconds_ += std::max(0.0, delta_time);
    if (flush_accumulator_seconds_ >= kFlushIntervalSeconds || pending_lines_.size() >= kFlushBatchSize) {
        flush_accumulator_seconds_ = 0.0;
        flushPendingLogs(kFlushBatchSize);
    }
}

std::string LogSeverityToString(LogSeverity severity) {
    switch (severity) {
        case LogSeverity::DEBUG: return "DEBUG";
        case LogSeverity::INFO: return "INFO";
        case LogSeverity::WARNING: return "WARNING";
        case LogSeverity::ERROR: return "ERROR";
        case LogSeverity::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void LoggingSystem::handleLogEvent(const LogEvent& event) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt{};
#ifdef _WIN32
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif

    std::stringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S") << " [" << LogSeverityToString(event.severity) << "] "
       << "[" << event.source_system << "] " << event.message;

    pending_lines_.push_back(ss.str());

    // Backpressure guard: if logging spikes, flush immediately and bound memory.
    if (pending_lines_.size() >= kHardQueueLimit) {
        flushPendingLogs(pending_lines_.size());
    }
}

void LoggingSystem::flushPendingLogs(std::size_t max_lines) {
    if (pending_lines_.empty() || max_lines == 0) {
        return;
    }

    // Use a fixed path in /tmp for easier discovery in restricted environments.
    std::ofstream log_file("/tmp/neon_oubliette.log", std::ios::app);
    if (!log_file.is_open()) {
        return;
    }

    std::size_t lines_written = 0;
    while (!pending_lines_.empty() && lines_written < max_lines) {
        log_file << pending_lines_.front() << '\n';
        pending_lines_.pop_front();
        ++lines_written;
    }
}

} // namespace NeonOubliette
