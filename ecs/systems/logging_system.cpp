#include "logging_system.h"

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
    (void)delta_time;
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

    // Use a fixed path in /tmp for easier discovery in restricted environments
    std::ofstream log_file("/tmp/neon_oubliette.log", std::ios::app);
    if (log_file.is_open()) {
        log_file << ss.str() << std::endl;
    }
}

} // namespace NeonOubliette
