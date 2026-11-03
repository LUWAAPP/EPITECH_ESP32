#pragma once

#include "board/ttgo_t7_v14.h"
#include "sensors/temperature_sensor.h"
#include "sensors/co2_sensor.h"
#include "sensors/pm_sensor.h"
#include "sensors/sound_sensor.h"
#include "../utils/logger.h"

enum SensorType {
    SENSOR_TEMPERATURE,
    SENSOR_CO2,
    SENSOR_PM,
    SENSOR_SOUND
};

class HAL {
public:
    static HAL& getInstance() {
        static HAL instance;
        return instance;
    }

    // Initialize all sensors, returns true if all sensors initialized successfully
    bool init();
    
    // Update method - should be called regularly in the main loop
    void update();
    
    // Check sensor initialization status
    bool isSensorInitialized(SensorType sensor);
    
    // Initialize specific sensors, returns true if successful
    bool initTemperatureSensor(int dhtPin, int sclPin);
    bool initCO2Sensor(int rxPin, int txPin, HardwareSerial& serial, 
                       CO2Sensor::SensorType co2Type = CO2Sensor::SensorType::NONE, 
                       bool autoDetect = true);
    bool initPMSensor(int rxPin, int txPin);
    bool initSoundSensor(int adcPin);

    // Getters with initialization check
    TemperatureSensor& getTemperatureSensor();
    CO2Sensor& getCO2Sensor();
    PMSensor& getPMSensor();
    SoundSensor& getSoundSensor();

private:
    HAL(); // Private constructor for singleton
    HAL(const HAL&) = delete; // Delete copy constructor
    HAL& operator=(const HAL&) = delete; // Delete assignment operator
    
    // CO2 sensor verification function
    void verifyCO2Sensor();
    
    TemperatureSensor tempSensor;
    CO2Sensor co2Sensor;
    PMSensor pmSensor;
    SoundSensor soundSensor;
    
    bool tempSensorInitialized;
    bool co2SensorInitialized;
    bool pmSensorInitialized;
    bool soundSensorInitialized;
    bool co2SensorVerified;
    
    unsigned long verifyCO2SensorTimestamp;
    
    const int MAX_INIT_RETRIES = 3;
    
    uint8_t sensorStatus = 0;  // Bitmap to track sensor status
}; 