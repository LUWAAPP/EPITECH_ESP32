#include "co2_sensor.h"
#include "../../utils/logger.h"

// MHZ19 Sensor Implementation
bool MHZ19Sensor::init(int rxPin, int txPin, HardwareSerial& serial) {
    try {
        serialPort = &serial;
        serialPort->begin(9600);
        
        mhz19.begin(*serialPort);
        mhz19.autoCalibration();
        
        // Allow sensor to warm up before first reading
        LOG_I("Allowing MHZ19 CO2 sensor to warm up (3 seconds)...");
        delay(3000);
        
        // Test if the sensor is responding
        int retries = 5;
        int co2 = 0;
        int8_t temp = 0;
        
        while (retries > 0) {
            co2 = mhz19.getCO2();
            temp = mhz19.getTemperature();
            
            if (co2 > 0) {
                LOG_I("MHZ19 CO2 sensor initialized. Initial readings: %d ppm, %d°C", co2, temp);
                initialized = true;
                return true;
            } else {
                LOG_W("MHZ19 CO2 sensor returned invalid initial reading, retrying... (%d attempts left)", retries);
                retries--;
                delay(1000);
            }
        }
        
        LOG_W("MHZ19 CO2 sensor initialization failed after multiple attempts");
        return false;
    } catch (...) {
        LOG_E("Exception during MHZ19 CO2 sensor initialization");
        return false;
    }
}

bool MHZ19Sensor::read(int& co2, int8_t& temperature) {
    if (!initialized) {
        LOG_E("MHZ19 CO2 sensor not initialized");
        return false;
    }
    
    unsigned long startTime = millis();
    while (millis() - startTime < READ_TIMEOUT) {
        co2 = mhz19.getCO2();
        temperature = mhz19.getTemperature();
        
        // Basic validation
        if (co2 > 0 && co2 < 10000) {
            LOG_V("MHZ19 CO2 reading: %d ppm, temperature: %d°C", co2, temperature);
            return true;
        } else {
            LOG_W("MHZ19 CO2 sensor returned invalid reading: %d ppm, %d°C", co2, temperature);
            mhz19.verify();
            mhz19.recoveryReset();
        }
        
        delay(10);
    }
    
    LOG_W("MHZ19 CO2 sensor read timeout or invalid reading");
    return false;
}

void MHZ19Sensor::calibrate() {
    if (initialized) {
        LOG_I("Calibrating MHZ19 CO2 sensor");
        mhz19.autoCalibration();
    } else {
        LOG_W("Cannot calibrate uninitialized MHZ19 CO2 sensor");
    }
}

// SenseAir S8 Sensor Implementation
bool SenseAirS8Sensor::init(int rxPin, int txPin, HardwareSerial& serial) {
    try {
        serialPort = &serial;
        serialPort->begin(9600);
        
        s8 = new S8_UART(*serialPort);
        
        // Test the sensor by reading a value
        LOG_I("Initializing SenseAir S8 CO2 sensor...");
        delay(1000); // Give sensor time to stabilize
        
        int co2 = s8->get_co2();
        if (co2 > 0 && co2 < 10000) {
            LOG_I("SenseAir S8 CO2 sensor initialized. Initial reading: %d ppm", co2);
            initialized = true;
            LOG_I("SenseAir S8 has built-in automatic background calibration (ABC)");
            return true;
        } else {
            LOG_E("Failed to get valid reading from SenseAir S8 sensor (got %d ppm)", co2);
            delete s8;
            s8 = nullptr;
            return false;
        }
    } catch (...) {
        LOG_E("Exception during SenseAir S8 initialization");
        if (s8) {
            delete s8;
            s8 = nullptr;
        }
        return false;
    }
}

bool SenseAirS8Sensor::read(int& co2, int8_t& temperature) {
    if (!initialized || !s8) {
        LOG_E("SenseAir S8 sensor not initialized");
        return false;
    }
    
    try {
        co2 = s8->get_co2();
        temperature = 0; // S8_UART library doesn't provide temperature
        
        if (co2 > 0 && co2 < 10000) {
            return true;
        } else {
            LOG_W("Invalid CO2 reading from SenseAir S8: %d ppm", co2);
            return false;
        }
    } catch (...) {
        LOG_E("Exception while reading SenseAir S8 sensor");
        return false;
    }
}

void SenseAirS8Sensor::calibrate() {
    LOG_I("Performing SenseAir S8 zero point calibration");
    performZeroPointCalibration();
}

bool SenseAirS8Sensor::performZeroPointCalibration() {
    if (!initialized || !s8) {
        LOG_E("SenseAir S8 sensor not initialized");
        return false;
    }
    
    LOG_W("Starting zero point calibration - ensure sensor is in fresh air (400 ppm CO2)!");
    LOG_W("Sensor should be in stable fresh air for at least 20 minutes before calibration");
    
    try {
        bool result = s8->manual_calibration();
        
        if (result) {
            LOG_I("SenseAir S8 zero point calibration completed successfully");
        } else {
            LOG_E("SenseAir S8 zero point calibration failed");
        }
        
        return result;
    } catch (...) {
        LOG_E("Exception during zero point calibration");
        return false;
    }
}

// Main CO2Sensor class implementation
bool CO2Sensor::init(int rxPin, int txPin, HardwareSerial& serial, SensorType type, bool autoDetect) {
    // Clear any existing sensor
    sensor.reset();
    
    // Auto-detect sensor type if requested
    if (autoDetect && type == SensorType::NONE) {
        return autoDetectSensor(rxPin, txPin, serial);
    }
    
    // Otherwise, create the specified sensor type
    currentType = type;
    
    // Create and initialize the requested sensor
    switch (type) {
        case SensorType::MHZ19:
            sensor.reset(new MHZ19Sensor());
            if (!sensor->init(rxPin, txPin, serial)) {
                LOG_E("Failed to initialize MHZ19 CO2 sensor");
                return false;
            }
            break;
            
        case SensorType::SENSEAIR_S8:
            sensor.reset(new SenseAirS8Sensor());
            if (!sensor->init(rxPin, txPin, serial)) {
                LOG_E("Failed to initialize SenseAir S8 CO2 sensor");
                return false;
            }
            break;
            
        default:
            LOG_E("Invalid or unspecified CO2 sensor type");
            return false;
    }
    
    return true;
}

bool CO2Sensor::autoDetectSensor(int rxPin, int txPin, HardwareSerial& serial) {
    LOG_I("Auto-detecting CO2 sensor type...");
    
    // First try MHZ19, which is the existing sensor
    sensor.reset(new MHZ19Sensor());
    if (sensor->init(rxPin, txPin, serial)) {
        LOG_I("Detected MHZ19 CO2 sensor");
        currentType = SensorType::MHZ19;
        return true;
    }
    
    // If MHZ19 fails, try SenseAir S8
    sensor.reset(new SenseAirS8Sensor());
    if (sensor->init(rxPin, txPin, serial)) {
        LOG_I("Detected SenseAir S8 CO2 sensor");
        currentType = SensorType::SENSEAIR_S8;
        return true;
    }
    
    // No sensor detected
    LOG_E("No CO2 sensor detected");
    sensor.reset();
    currentType = SensorType::NONE;
    return false;
}

bool CO2Sensor::read(int& co2, int8_t& temperature) {
    if (!sensor) {
        LOG_E("CO2 sensor not initialized");
        return false;
    }
    return sensor->read(co2, temperature);
}

bool CO2Sensor::readWithRetry(int& co2, int8_t& temperature, int maxRetries, int retryDelayMs) {
    for (int i = 0; i < maxRetries; i++) {
        if (read(co2, temperature)) {
            if (i > 0) {
                LOG_D("CO2 sensor reading successful after %d retries", i);
            }
            return true;
        }
        
        if (i < maxRetries - 1) {
            LOG_D("Retrying CO2 sensor reading (%d/%d)...", i + 1, maxRetries);
            delay(retryDelayMs);
        }
    }
    
    LOG_W("CO2 sensor reading failed after %d retries", maxRetries);
    return false;
}

void CO2Sensor::calibrate() {
    if (sensor) {
        sensor->calibrate();
    } else {
        LOG_W("Cannot calibrate uninitialized CO2 sensor");
    }
}

bool CO2Sensor::performZeroPointCalibration() {
    if (currentType == SensorType::SENSEAIR_S8) {
        SenseAirS8Sensor* s8Sensor = static_cast<SenseAirS8Sensor*>(sensor.get());
        return s8Sensor->performZeroPointCalibration();
    } else {
        LOG_W("Zero point calibration only supported for SenseAir S8 sensors");
        return false;
    }
} 