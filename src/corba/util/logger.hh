#pragma once

#include <syslog.h>
#include <ctime>
#include <string>
#include <vector>
#include <memory>
#include <format>

class LogDestination {
    public:
        virtual ~LogDestination();
        virtual void log(int priority, const char *message) = 0;
};

class Logger {
        static std::shared_ptr<LogDestination> destination;
        static int maxLevel;

    public:
        // RFC 5424 The Syslog Protocol

        /** system is unusable */
        template <typename... Args>
        static inline void emerg(std::format_string<Args...> fmt, Args &&...args) {
            log(LOG_EMERG, std::format(fmt, std::forward<Args>(args)...).c_str());
        }
        /** action must be taken immediately */
        template <typename... Args>
        static inline void alert(std::format_string<Args...> fmt, Args &&...args) {
            log(LOG_ALERT, std::format(fmt, std::forward<Args>(args)...).c_str());
        }
        /** critical */
        template <typename... Args>
        static inline void crit(std::format_string<Args...> fmt, Args &&...args) {
            log(LOG_CRIT, std::format(fmt, std::forward<Args>(args)...).c_str());
        }
        template <typename... Args>
        static inline void error(std::format_string<Args...> fmt, Args &&...args) {
            log(LOG_ERR, std::format(fmt, std::forward<Args>(args)...).c_str());
        }
        template <typename... Args>
        static inline void warn(std::format_string<Args...> fmt, Args &&...args) {
            log(LOG_WARNING, std::format(fmt, std::forward<Args>(args)...).c_str());
        }
        /** normal but significant condition */
        template <typename... Args>
        static inline void notice(std::format_string<Args...> fmt, Args &&...args) {
            log(LOG_NOTICE, std::format(fmt, std::forward<Args>(args)...).c_str());
        }
        /** informational */
        template <typename... Args>
        static inline void info(std::format_string<Args...> fmt, Args &&...args) {
            log(LOG_INFO, std::format(fmt, std::forward<Args>(args)...).c_str());
        }
        /** debug */
        template <typename... Args>
        static inline void debug(std::format_string<Args...> fmt, Args &&...args) {
            log(LOG_DEBUG, std::format(fmt, std::forward<Args>(args)...).c_str());
        }

        static inline void setLevel(int level) { Logger::maxLevel = level; }
        static inline void setDestination(std::shared_ptr<LogDestination> aDestination) { Logger::destination = aDestination; }

    protected:
        static void log(int level, const char *message);
};

class SysLogger : public LogDestination {
    protected:
        /**
         * \param option LOG_NOWAIT, LOG_ODELAY
         * \param facility LOG_AUTH, LOG_DAEMON, LOG_KERN, ..., LOG_USER
         */
        SysLogger(const char *id, int option, int facility) { openlog(id, option, facility); }
        virtual void log(int priority, const char *message) override;
};

struct LogEntry {
        int priority;
        std::time_t time;
        std::string message;

        LogEntry(int priority, std::time_t time, std::string message) : priority(priority), time(time), message(message) {}
        std::string toString() const;
};

class MemoryLogger : public LogDestination {
    public:
        std::vector<LogEntry> logs;
        inline void clear() { logs.clear(); }

    protected:
        virtual void log(int priority, const char *message) override;
};
