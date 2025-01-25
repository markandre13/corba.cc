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
