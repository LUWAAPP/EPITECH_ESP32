#include "sound_sensor.h"
#include "../../utils/logger.h"

bool SoundSensor::init(int adcPin) {
    try {
        if (adcPin < 0) {
            LOG_E("Invalid ADC pin for sound sensor: %d", adcPin);
            return false;
        }
        
        pin = adcPin;
        pinMode(pin, INPUT);
        
        // Test if the ADC is working by taking a reading
        unsigned int sample = analogRead(pin);
        LOG_I("Sound sensor initialized on pin %d. Initial reading: %u", pin, sample);
        
        initialized = true;
        return true;
    } catch (...) {
        LOG_E("Exception during sound sensor initialization");
        return false;
    }
}

bool SoundSensor::read(unsigned int& peakToPeak) {
    if (!initialized) {
        LOG_E("Sound sensor not initialized");
        return false;
    }
    
    try {
        unsigned long startMillis = millis();
        unsigned int signalMax = 0;
        unsigned int signalMin = 4095;
        
        // Collect data for sample window duration
        while (millis() - startMillis < sampleWindow) {
            unsigned int sample = analogRead(pin);
            if (sample > signalMax) {
                signalMax = sample;
            } else if (sample < signalMin) {
                signalMin = sample;
            }
        }
        
        peakToPeak = signalMax - signalMin;
        LOG_V("Sound level: %u", peakToPeak);
        return true;
    } catch (...) {
        LOG_E("Exception during sound sensor reading");
        return false;
    }
}

bool SoundSensor::readWithRetry(unsigned int& peakToPeak, int maxRetries, int retryDelayMs) {
    for (int i = 0; i < maxRetries; i++) {
        if (read(peakToPeak)) {
            if (i > 0) {
                LOG_D("Sound sensor reading successful after %d retries", i);
            }
            return true;
        }
        
        if (i < maxRetries - 1) {
            LOG_D("Retrying sound sensor reading (%d/%d)...", i + 1, maxRetries);
            delay(retryDelayMs);
        }
    }
    
    LOG_W("Sound sensor reading failed after %d retries", maxRetries);
    return false;
} 