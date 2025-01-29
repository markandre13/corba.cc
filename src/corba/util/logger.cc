#include "logger.hh"

std::shared_ptr<LogDestination> Logger::destination;
int Logger::maxLevel = LOG_WARNING;

LogDestination::~LogDestination() {}

void Logger::log(int level, const char *message) {
    if (level < level) {
        return;
    }
    if (destination) {
        destination->log(level, message);
    }
};

void SysLogger::log(int priority, const char *message) { syslog(priority, "%s", message); }

void MemoryLogger::log(int priority, const char *message) { logs.push_back(LogEntry(priority, std::time(nullptr), message)); }

const char *priorityName[8] = {
    "EMERG ", // 0 system is unusable
    "ALERT ", // 1 action must be taken immediately
    "CRIT  ", // 2 critical conditions
    "ERROR ", // 3 error conditions
    "WARN  ", // 4 warning conditions
    "NOTICE", // 5 normal but significant condition
    "INFO  ", // 6 informational
    "DEBUG ", // 7 debug-level messages
};

std::string LogEntry::toString() const {
    char timeString[std::size("yyyy-mm-ddThh:mm:ssZ")];
    std::strftime(std::data(timeString), std::size(timeString), "%FT%TZ", std::gmtime(&time));

    return std::format("{} {} {}", timeString, priorityName[priority], message);
}