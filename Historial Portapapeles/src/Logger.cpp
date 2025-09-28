#include "Logger.h"

Logger::Logger()
    : logToFile(false), consoleLogLevel(INFO) {}

Logger::~Logger() {
    if (fileStream.is_open()) {
        fileStream.close();
    }
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::enableFileLogging(bool enable, const std::string& filename) {
    logToFile = enable;

    if (logToFile) {
        if (fileStream.is_open()) {
            fileStream.close();
        }

        fileStream.open(filename.c_str(), std::ios::out); // overwrite mode
        if (!fileStream) {
            std::wcerr << L"[Logger] Failed to open file for logging." << std::endl;
            logToFile = false;
        }
    }
}

void Logger::setConsoleLogLevel(LogLevel level) {
    consoleLogLevel = level;
}

const wchar_t* Logger::levelToString(LogLevel level) {
    switch (level) {
        case INFO:    return L"INFO";
        case DEBUG:   return L"DEBUG";
        case WARNING: return L"WARN";
        case LERROR:   return L"ERROR";
        default:      return L"UNKNOWN";
    }
}

void Logger::log(LogLevel level, const std::wstring& message) {
    std::wstring fullMessage = L"[" + std::wstring(levelToString(level)) + L"] " + message;

    // Only print to console if log level is >= threshold
    if (level >= consoleLogLevel) {
        std::wcerr << fullMessage << std::endl;
    }

    // Always write to file if enabled
    if (logToFile && fileStream.is_open()) {
        fileStream << fullMessage << std::endl;
    }
}
