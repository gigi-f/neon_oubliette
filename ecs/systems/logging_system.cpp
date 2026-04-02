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
    // Logging system doesn't need to do anything per-tick unless we add batching
    (void)delta_time;
}

std::string LogSeverityToString(LogSeverity severity) {
    switch (severity) {
        case LogSeverity::DEBUG:
            return "DEBUG";
        case LogSeverity::INFO:
            return "INFO";
        case LogSeverity::WARNING:
            return "WARNING";
        case LogSeverity::ERROR:
            return "ERROR";
        case LogSeverity::CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

void LoggingSystem::handleLogEvent(const LogEvent& event) {
    // Use std::localtime_r for thread safety if needed, but local is fine for single-threaded CLI
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt{};
    localtime_r(&in_time_t, &bt);

    std::stringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S") << " [" << LogSeverityToString(event.severity) << "] "
       << "[" << event.source_system << "] " << event.message;

    /* Muted to prevent Notcurses terminal corruption
    if (event.severity == LogSeverity::ERROR || event.severity == LogSeverity::CRITICAL) {
        std::cerr << ss.str() << std::endl;
    } else {
        std::cout << ss.str() << std::endl;
    }
    */

    // Also write to a file for persistent logging and verification
    static std::ofstream log_file("/home/gianfiorenzo/Documents/ag2_workspace/neon_oubliette/game.log", std::ios::app);
    if (log_file.is_open()) {
        log_file << ss.str() << std::endl;
    }
}

} // namespace NeonOubliette
