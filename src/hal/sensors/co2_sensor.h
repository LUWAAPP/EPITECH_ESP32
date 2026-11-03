#pragma once
#include <MHZ19.h>
#include <s8_uart.h>
#include "HardwareSerial.h"
#include <memory>

// Abstract base class for CO2 sensors
class ICO2Sensor {
public:
    virtual bool init(int rxPin, int txPin, HardwareSerial& serial) = 0;
    virtual bool read(int& co2, int8_t& temperature) = 0;
    virtual void calibrate() = 0;
    virtual ~ICO2Sensor() = default;
};

// MHZ19 implementation
class MHZ19Sensor : public ICO2Sensor {
public:
    bool init(int rxPin, int txPin, HardwareSerial& serial) override;
    bool read(int& co2, int8_t& temperature) override;
    void calibrate() override;
    
private:
    MHZ19 mhz19;
    HardwareSerial* serialPort;
    bool initialized = false;
    static const unsigned long READ_TIMEOUT = 1000;
};

// SenseAir S8 implementation  
class SenseAirS8Sensor : public ICO2Sensor {
public:
    bool init(int rxPin, int txPin, HardwareSerial& serial) override;
    bool read(int& co2, int8_t& temperature) override;
    void calibrate() override;
    
    // Basic calibration method (zero point only)
    bool performZeroPointCalibration();
    
private:
    S8_UART* s8;
    HardwareSerial* serialPort;
    bool initialized = false;
    static const unsigned long READ_TIMEOUT = 1000;
    static const uint16_t FRESH_AIR_CO2_LEVEL = 400;
};

// Main CO2 sensor class that delegates to the chosen implementation
class CO2Sensor {
public:
    enum class SensorType {
        NONE,
        MHZ19,
        SENSEAIR_S8
    };
    
    CO2Sensor() = default;
    ~CO2Sensor() = default;
    
    // Initialize with auto-detection capability
    bool init(int rxPin, int txPin, HardwareSerial& serial, SensorType type = SensorType::NONE, bool autoDetect = true);
    
    // Reading methods
    bool read(int& co2, int8_t& temperature);
    bool readWithRetry(int& co2, int8_t& temperature, int maxRetries = 3, int retryDelayMs = 200);
    
    // Calibration
    void calibrate();
    
    // Basic calibration method (SenseAir S8 specific)
    bool performZeroPointCalibration();
    
    // Status methods
    bool isInitialized() const { return sensor != nullptr; }
    SensorType getType() const { return currentType; }
    
private:
    std::unique_ptr<ICO2Sensor> sensor;
    SensorType currentType = SensorType::NONE;
    bool autoDetectSensor(int rxPin, int txPin, HardwareSerial& serial);
}; 