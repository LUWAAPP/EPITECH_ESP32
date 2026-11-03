#include "hal.h"
#include "board/ttgo_t7_v14.h"
#include "../utils/logger.h"

HAL::HAL() : 
    tempSensorInitialized(false),
    co2SensorInitialized(false),
    pmSensorInitialized(false),
    soundSensorInitialized(false),
    co2SensorVerified(false),
    verifyCO2SensorTimestamp(0) {
}

bool HAL::init() {
    LOG_I("Initializing HAL...");
    bool success = true;
    
    // Initialize temperature sensor with auto-detection
    if (!initTemperatureSensor(BoardPins::DHT_PIN, BoardPins::I2C_SCL)) {
        LOG_W("Failed to initialize temperature sensor");
        sensorStatus &= ~SENSOR_TEMPERATURE;
        tempSensorInitialized = false;
        success = false;
    } else {
        sensorStatus |= SENSOR_TEMPERATURE;
        tempSensorInitialized = true;
    }
    
    // Initialize CO2 sensor with retries
    if (!initCO2Sensor(BoardPins::CO2_RX, BoardPins::CO2_TX, Serial2)) {
        LOG_E("Failed to initialize CO2 sensor!");
        success = false;
    } else {
        // Schedule CO2 sensor verification is now done in initCO2Sensor
        LOG_I("CO2 sensor initialized successfully, verification will occur in 30 seconds");
    }
    
    // Initialize PM sensor with retries
    if (!initPMSensor(BoardPins::PM_RX, BoardPins::PM_TX)) {
        LOG_E("Failed to initialize PM sensor!");
        success = false;
    }
    
    // Initialize sound sensor
    if (!initSoundSensor(BoardPins::ADC_SOUND)) {
        LOG_E("Failed to initialize sound sensor!");
        success = false;
    }
    
    if (success) {
        LOG_I("All sensors initialized successfully");
    } else {
        LOG_W("Some sensors failed to initialize");
    }
    
    return success;
}

// This method should be called regularly in the main loop
void HAL::update() {
    // Remove excessive logging that could flood the serial console
    if (co2SensorInitialized && !co2SensorVerified) {
        // Log only once during startup, not continuously
        static bool loggedOnce = false;
        if (!loggedOnce) {
            LOG_I("CO2 sensor verification status: verified=%d, timestamp=%lu, current=%lu, diff=%lu", 
                co2SensorVerified, verifyCO2SensorTimestamp, millis(), millis() - verifyCO2SensorTimestamp);
            loggedOnce = true;
        }
        
        // Check if it's time to verify (30 seconds from initialization)
        if (verifyCO2SensorTimestamp > 0 && millis() - verifyCO2SensorTimestamp >= 30000) {
            LOG_I("30 seconds elapsed since CO2 sensor init, performing verification now");
            verifyCO2Sensor();
        }
    }
}

void HAL::verifyCO2Sensor() {
    LOG_I("Verifying CO2 sensor readings after warmup period...");
    
    int co2Value;
    int8_t temperature;
    bool readSuccess = co2Sensor.readWithRetry(co2Value, temperature, 3, 1000);
    
    if (!readSuccess || co2Value == 0) {
        // Production firmware reboots here. On a teaching bench a missing or cold
        // sensor must not turn into a boot loop: report once and keep sampling.
        LOG_E("CO2 verification failed: still 0 ppm after warm-up; check power and UART");
        co2SensorVerified = true;
        return;
    }
    
    LOG_I("CO2 sensor verification successful! Current readings: %d ppm, %d°C", co2Value, temperature);
    co2SensorVerified = true;
}

bool HAL::isSensorInitialized(SensorType sensor) {
    switch (sensor) {
        case SENSOR_TEMPERATURE: return tempSensorInitialized;
        case SENSOR_CO2: return co2SensorInitialized;
        case SENSOR_PM: return pmSensorInitialized;
        case SENSOR_SOUND: return soundSensorInitialized;
        default: return false;
    }
}

bool HAL::initTemperatureSensor(int dhtPin, int sclPin) {
    LOG_I("Initializing temperature sensor...");
    
    // Use the default I2C SDA pin from the board definition
    int sdaPin = BoardPins::I2C_SDA;
    
    for (int i = 0; i < MAX_INIT_RETRIES; i++) {
        try {
            // Auto-detect sensor type
            if (tempSensor.init(dhtPin, sclPin, TemperatureSensor::SensorType::NONE, true)) {
                // Test the sensor
                float temp, humidity;
                if (tempSensor.read(temp, humidity)) {
                    LOG_I("Temperature sensor initialized successfully: %.1f°C, %.1f%%", temp, humidity);
                    tempSensorInitialized = true;
                    return true;
                }
            }
            
            LOG_W("Temperature sensor initialization attempt %d failed, retrying...", i + 1);
            delay(100);
        } catch (...) {
            LOG_E("Exception during temperature sensor initialization");
        }
    }
    
    return false;
}

bool HAL::initCO2Sensor(int rxPin, int txPin, HardwareSerial& serial, 
                       CO2Sensor::SensorType co2Type, bool autoDetect) {
    LOG_I("Initializing CO2 sensor...");
    
    for (int i = 0; i < MAX_INIT_RETRIES; i++) {
        try {
            if (co2Sensor.init(rxPin, txPin, serial, co2Type, autoDetect)) {
                // Test the sensor by reading values
                int co2;
                int8_t temp;
                if (co2Sensor.readWithRetry(co2, temp, 5, 500)) {
                    const char* sensorTypeStr = "";
                    switch (co2Sensor.getType()) {
                        case CO2Sensor::SensorType::MHZ19:
                            sensorTypeStr = "MHZ19";
                            break;
                        case CO2Sensor::SensorType::SENSEAIR_S8:
                            sensorTypeStr = "SenseAir S8";
                            break;
                        default:
                            sensorTypeStr = "Unknown";
                            break;
                    }
                    
                    LOG_I("CO2 sensor (%s) initialized. Current readings: %d ppm, %d°C", 
                          sensorTypeStr, co2, temp);
                    co2SensorInitialized = true;
                    co2SensorVerified = false;
                    
                    // Set the verification timestamp HERE, when we know initialization succeeded
                    verifyCO2SensorTimestamp = millis();
                    LOG_I("CO2 sensor verification scheduled in 30 seconds (timestamp: %lu)", verifyCO2SensorTimestamp);
                    
                    return true;
                }
            }
            
            LOG_W("CO2 sensor initialization attempt %d failed, retrying...", i + 1);
            delay(100);
        } catch (...) {
            LOG_E("Exception during CO2 sensor initialization");
        }
    }
    
    return false;
}

bool HAL::initPMSensor(int rxPin, int txPin) {
    LOG_I("Initializing PM sensor...");
    
    for (int i = 0; i < MAX_INIT_RETRIES; i++) {
        try {
            pmSensor.init(rxPin, txPin);
            
            // Test the sensor by reading values
            uint16_t pm25;
            if (pmSensor.read(pm25)) {
                LOG_I("PM sensor initialized. Current PM2.5 reading: %u µg/m³", pm25);
                pmSensorInitialized = true;
                return true;
            }
            
            LOG_W("PM sensor initialization attempt %d failed, retrying...", i + 1);
            delay(100);
        } catch (...) {
            LOG_E("Exception during PM sensor initialization");
        }
    }
    
    return false;
}

bool HAL::initSoundSensor(int adcPin) {
    LOG_I("Initializing sound sensor...");
    
    try {
        soundSensor.init(adcPin);
        
        // Test the sensor by reading values
        unsigned int peakToPeak;
        if (soundSensor.read(peakToPeak)) {
            LOG_I("Sound sensor initialized. Current reading: %u", peakToPeak);
            soundSensorInitialized = true;
            return true;
        }
    } catch (...) {
        LOG_E("Exception during sound sensor initialization");
    }
    
    return false;
}

CO2Sensor& HAL::getCO2Sensor() {
    if (!co2SensorInitialized) {
        LOG_W("Accessing uninitialized CO2 sensor!");
    }
    return co2Sensor;
}

PMSensor& HAL::getPMSensor() {
    if (!pmSensorInitialized) {
        LOG_W("Accessing uninitialized PM sensor!");
    }
    return pmSensor;
}

SoundSensor& HAL::getSoundSensor() {
    if (!soundSensorInitialized) {
        LOG_W("Accessing uninitialized sound sensor!");
    }
    return soundSensor;
}

TemperatureSensor& HAL::getTemperatureSensor() {
    if (!tempSensorInitialized) {
        LOG_W("Accessing uninitialized temperature sensor!");
    }
    return tempSensor;
} 