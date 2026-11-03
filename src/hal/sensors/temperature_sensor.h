#pragma once

#include <Arduino.h>
#include <SensirionI2cSht4x.h>
#include <Wire.h>
#include <DHT.h>
#include <memory> // For std::unique_ptr
#include "../../utils/logger.h"

// Abstract base class for temperature sensors
class ITemperatureSensor {
public:
    virtual bool init(int pin1, int pin2) = 0;
    virtual bool read(float& temperature, float& humidity) = 0;
    virtual ~ITemperatureSensor() = default;
};

// SHT40 implementation
class SHT40Sensor : public ITemperatureSensor {
public:
    bool init(int sdaPin, int sclPin) override;
    bool read(float& temperature, float& humidity) override;
private:
    SensirionI2cSht4x sht4x;
};

// DHT22 implementation
class DHT22Sensor : public ITemperatureSensor {
public:
    bool init(int dhtPin, int unused) override;
    bool read(float& temperature, float& humidity) override;
private:
    DHT dht{0, DHT22}; // Default initialization, pin will be set in init()
};

// DHT11 implementation
class DHT11Sensor : public ITemperatureSensor {
public:
    bool init(int dhtPin, int unused) override;
    bool read(float& temperature, float& humidity) override;
private:
    DHT dht{0, DHT11}; // Default initialization, pin will be set in init()
};

// Main temperature sensor class that delegates to the chosen implementation
class TemperatureSensor {
public:
    enum class SensorType {
        NONE,
        SHT40,
        DHT22,
        DHT11
    };
    
    TemperatureSensor();
    ~TemperatureSensor() = default;
    
    // Initialize with auto-detection capability
    bool init(int pin1, int pin2 = 0, SensorType type = SensorType::NONE, bool autoDetect = true);
    
    // Reading methods
    bool read(float& temperature, float& humidity);
    bool readWithRetry(float& temperature, float& humidity, int maxRetries = 3, int retryDelayMs = 100);
    
    // Status methods
    bool isInitialized() const { return sensor != nullptr; }
    SensorType getType() const { return currentType; }
    
private:
    std::unique_ptr<ITemperatureSensor> sensor;
    SensorType currentType;
    bool autoDetectSensor(int pin1, int pin2);
}; 