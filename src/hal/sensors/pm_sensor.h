#pragma once
#include <PMS.h>
#include <SoftwareSerial.h>
#include <memory>

class PMSensor {
public:
    PMSensor() = default;
    
    bool init(int rxPin, int txPin);
    bool read(uint16_t& pm25);
    
    void wakeUp();
    void sleep();
    
    // Method for retry logic
    bool readWithRetry(uint16_t& pm25, int maxRetries = 3, int retryDelayMs = 200);
    
private:
    std::unique_ptr<SoftwareSerial> serialPort;
    std::unique_ptr<PMS> pms;
    PMS::DATA data;
    
    // Timeout value in milliseconds
    static const unsigned long READ_TIMEOUT = 1000;
}; 