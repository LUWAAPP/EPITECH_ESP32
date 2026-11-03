#pragma once
#include <Arduino.h>

class SoundSensor {
public:
    SoundSensor() : pin(-1), initialized(false) {}
    bool init(int adcPin);
    bool read(unsigned int& peakToPeak);
    
    // New method with retry logic
    bool readWithRetry(unsigned int& peakToPeak, int maxRetries = 3, int retryDelayMs = 200);
    
private:
    int pin;
    bool initialized;
    static const int sampleWindow = 50;  // Sample window width in mS (50 mS = 20Hz)
}; 