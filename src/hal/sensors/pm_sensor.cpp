#include "pm_sensor.h"
#include "../../utils/logger.h"

bool PMSensor::init(int rxPin, int txPin) {
    try {
        // Create new instances with smart pointers
        serialPort = std::unique_ptr<SoftwareSerial>(new SoftwareSerial(rxPin, txPin));
        if (!serialPort) {
            LOG_E("Failed to allocate memory for PM sensor serial port");
            return false;
        }
        
        serialPort->begin(9600);
        
        pms = std::unique_ptr<PMS>(new PMS(*serialPort));
        if (!pms) {
            LOG_E("Failed to allocate memory for PM sensor");
            serialPort.reset(); // Not strictly necessary as the unique_ptr will clean up
            return false;
        }
        
        pms->wakeUp();
        delay(500); // Give the sensor time to wake up and initialize
        
        LOG_I("PM sensor initialized on pins RX:%d, TX:%d", rxPin, txPin);
        return true;
    } catch (...) {
        LOG_E("Exception during PM sensor initialization");
        return false;
    }
}

bool PMSensor::read(uint16_t& pm25) {
    if (!pms) {
        LOG_E("PM sensor not initialized");
        return false;
    }
    
    unsigned long startTime = millis();
    while (millis() - startTime < READ_TIMEOUT) {
        if (pms->readUntil(data)) {
            // Basic validation - reject implausible values
            if (data.PM_AE_UG_2_5 > 1000) {
                LOG_W("PM sensor reading out of range: %d µg/m³", data.PM_AE_UG_2_5);
                return false; // Unlikely valid reading
            }
            
            pm25 = data.PM_AE_UG_2_5;
            LOG_V("PM2.5 reading: %d µg/m³", pm25);
            return true;
        }
        
        // Short delay before next read attempt
        delay(10);
    }
    
    LOG_W("PM sensor read timeout");
    return false;
}

void PMSensor::wakeUp() {
    if (pms) {
        pms->wakeUp();
    } else {
        LOG_W("Cannot wake up uninitialized PM sensor");
    }
}

void PMSensor::sleep() {
    if (pms) {
        pms->sleep();
    } else {
        LOG_W("Cannot sleep uninitialized PM sensor");
    }
}

bool PMSensor::readWithRetry(uint16_t& pm25, int maxRetries, int retryDelayMs) {
    for (int i = 0; i < maxRetries; i++) {
        if (read(pm25)) {
            if (i > 0) {
                LOG_D("PM sensor reading successful after %d retries", i);
            }
            return true;
        }
        
        if (i < maxRetries - 1) {
            LOG_D("Retrying PM sensor reading (%d/%d)...", i + 1, maxRetries);
            delay(retryDelayMs);
        }
    }
    
    LOG_W("PM sensor reading failed after %d retries", maxRetries);
    return false;
}