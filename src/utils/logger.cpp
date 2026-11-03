#include "logger.h"
#include <stdarg.h>

void Logger::begin(unsigned long baud, LogLevel level) {
    if (!initialized) {
        Serial.begin(baud);
        initialized = true;
        startTime = millis();
    }
    currentLevel = level;
    
    // Print startup banner
    Serial.println();
    Serial.println("==========================================");
    Serial.println("           Environmental Sensor           ");
    Serial.println("==========================================");
    Serial.print("Log level: ");
    
    switch (level) {
        case LOG_ERROR:   Serial.println("ERROR"); break;
        case LOG_WARNING: Serial.println("WARNING"); break;
        case LOG_INFO:    Serial.println("INFO"); break;
        case LOG_DEBUG:   Serial.println("DEBUG"); break;
        case LOG_VERBOSE: Serial.println("VERBOSE"); break;
        default:          Serial.println("NONE");
    }
    
    Serial.println("------------------------------------------");
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel = level;
}

void Logger::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_ERROR, "[ERROR] ", format, args);
    va_end(args);
}

void Logger::warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_WARNING, "[WARN]  ", format, args);
    va_end(args);
}

void Logger::info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_INFO, "[INFO]  ", format, args);
    va_end(args);
}

void Logger::debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_DEBUG, "[DEBUG] ", format, args);
    va_end(args);
}

void Logger::verbose(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_VERBOSE, "[VERB]  ", format, args);
    va_end(args);
}

void Logger::log(LogLevel level, const char* prefix, const char* format, va_list args) {
    if (level > currentLevel || !initialized) {
        return;
    }
    
    // Print timestamp
    unsigned long now = millis();
    unsigned long seconds = now / 1000;
    unsigned long millis = now % 1000;
    
    char timestamp[20];
    sprintf(timestamp, "[%5lu.%03lu] ", seconds, millis);
    
    Serial.print(timestamp);
    Serial.print(prefix);
    
    // Print formatted message
    char temp[256];
    vsnprintf(temp, sizeof(temp), format, args);
    Serial.println(temp);
} 