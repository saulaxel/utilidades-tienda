#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <string>

class Logger {
public:
    enum LogLevel {
        INFO,
        DEBUG,
        WARNING,
        LERROR
    };

    static Logger& instance();

    // Enable/disable file logging
    void enableFileLogging(bool enable, const std::string& filename = "debug.txt");

    // Set minimum level to display on console (default: INFO)
    void setConsoleLogLevel(LogLevel level);

    // Log a message
    void log(LogLevel level, const std::wstring& message);

private:
    Logger();
    ~Logger();

    Logger(const Logger&);
    Logger& operator=(const Logger&);

    bool logToFile;
    std::wofstream fileStream;

    LogLevel consoleLogLevel;

    const wchar_t* levelToString(LogLevel level);
};

// Logging macros
#define LOG_INFO(msg)    Logger::instance().log(Logger::INFO, msg)
#define LOG_DEBUG(msg)   Logger::instance().log(Logger::DEBUG, msg)
#define LOG_WARN(msg)    Logger::instance().log(Logger::WARNING, msg)
#define LOG_ERROR(msg)   Logger::instance().log(Logger::LERROR, msg)

#endif // LOGGER_H
