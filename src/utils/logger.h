#pragma once

#include <Arduino.h>

enum LogLevel {
    LOG_NONE = 0,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG,
    LOG_VERBOSE
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }
    
    void begin(unsigned long baud = 115200, LogLevel level = LOG_INFO);
    void setLogLevel(LogLevel level);
    
    void error(const char* format, ...);
    void warning(const char* format, ...);
    void info(const char* format, ...);
    void debug(const char* format, ...);
    void verbose(const char* format, ...);
    
private:
    Logger() : currentLevel(LOG_NONE), initialized(false) {}
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void log(LogLevel level, const char* prefix, const char* format, va_list args);
    
    LogLevel currentLevel;
    bool initialized;
    unsigned long startTime;
};

// Shorthand macros for easier use
#define LOG_E(...) Logger::getInstance().error(__VA_ARGS__)
#define LOG_W(...) Logger::getInstance().warning(__VA_ARGS__)
#define LOG_I(...) Logger::getInstance().info(__VA_ARGS__)
#define LOG_D(...) Logger::getInstance().debug(__VA_ARGS__)
#define LOG_V(...) Logger::getInstance().verbose(__VA_ARGS__) 