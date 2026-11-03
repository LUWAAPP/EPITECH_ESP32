#include <Arduino.h>

#include "utils/logger.h"

// Stage v0.2-adc
//
// Goal: turn an analog voltage into a number you can defend.
//
// Lesson: the ESP32 ADC is 12 bit (0..4095) but not a laboratory instrument.
// analogSetAttenuation(ADC_11db) gives roughly 0..3.1 V; readings compress near
// the rails; GPIO 34..39 are input-only. The MAX9814 microphone outputs an audio
// waveform around a DC bias, so the useful quantity is peak-to-peak amplitude
// over a window, not a single analogRead().
//
// A 50 ms window is 20 Hz of envelope resolution: long enough to catch a clap,
// short enough not to blur it. This is the same measurement the production
// sound_sensor.cpp performs.

static const int ADC_SOUND = 35;  // verify against your S2 continuity map
static const unsigned long SAMPLE_WINDOW_MS = 50;
static const unsigned long REPORT_INTERVAL_MS = 500;
static const int HEARTBEAT_LED = LED_BUILTIN;

struct SoundWindow {
    uint16_t min;
    uint16_t max;
    uint32_t sum;
    uint32_t count;
};

static SoundWindow readWindow(int pin) {
    SoundWindow w = {4095, 0, 0, 0};
    unsigned long start = millis();

    while (millis() - start < SAMPLE_WINDOW_MS) {
        uint16_t sample = analogRead(pin);
        if (sample > w.max) w.max = sample;
        if (sample < w.min) w.min = sample;
        w.sum += sample;
        w.count++;
    }
    return w;
}

void setup() {
    Logger::getInstance().begin(115200, LOG_INFO);
    pinMode(HEARTBEAT_LED, OUTPUT);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    LOG_I("stage v0.2-adc on %s", ARDUINO_BOARD);
    LOG_I("sound ADC on GPIO %d, window %lu ms, report %lu ms",
          ADC_SOUND, SAMPLE_WINDOW_MS, REPORT_INTERVAL_MS);
    LOG_I("attenuation 11 dB, resolution 12 bit");
}

void loop() {
    static unsigned long lastBlink = 0;
    static unsigned long lastReport = 0;
    static bool ledOn = false;

    unsigned long now = millis();

    if (now - lastBlink >= 500) {
        lastBlink = now;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_LED, ledOn ? HIGH : LOW);
    }

    if (now - lastReport >= REPORT_INTERVAL_MS) {
        lastReport = now;

        SoundWindow w = readWindow(ADC_SOUND);
        uint16_t p2p = w.max - w.min;
        uint32_t mean = w.count ? w.sum / w.count : 0;

        LOG_I("p2p=%u min=%u max=%u mean=%lu samples=%lu",
              p2p, w.min, w.max, mean, w.count);
    }
}
