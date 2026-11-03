#include <Arduino.h>
#include <Wire.h>

#include "hal/board/ttgo_t7_v14.h"
#include "hal/sensors/temperature_sensor.h"
#include "utils/logger.h"

// Stage v0.4-temp-autodetect
//
// Goal: one firmware image that works on both board generations.
//
// Lesson: an abstract interface (ITemperatureSensor) buys runtime polymorphism.
// SHT4x answers on I2C, DHT22/DHT11 on a one-wire data pin; the caller does not
// care. Autodetection tries SHT4x first, then DHT22, then DHT11, and reports
// NONE instead of guessing.
//
// Caution: each failed DHT probe blocks for seconds (the library needs settle
// time). That is acceptable during boot, never inside a sampling loop.

static const unsigned long SAMPLE_WINDOW_MS = 50;
static const unsigned long REPORT_INTERVAL_MS = 500;
static const unsigned long HT_INTERVAL_MS = 2000;
static const int HEARTBEAT_LED = LED_BUILTIN;

static TemperatureSensor tempSensor;

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

static const char* sensorName(TemperatureSensor::SensorType type) {
    switch (type) {
        case TemperatureSensor::SensorType::SHT40: return "SHT40";
        case TemperatureSensor::SensorType::DHT22: return "DHT22";
        case TemperatureSensor::SensorType::DHT11: return "DHT11";
        default: return "NONE";
    }
}

void setup() {
    Logger::getInstance().begin(115200, LOG_INFO);
    pinMode(HEARTBEAT_LED, OUTPUT);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    LOG_I("stage v0.4-temp-autodetect on %s", ARDUINO_BOARD);

    bool ok = tempSensor.init(BoardPins::DHT_PIN, BoardPins::I2C_SCL,
                              TemperatureSensor::SensorType::NONE, true);
    if (!ok) {
        LOG_E("no temperature sensor detected (SHT40 0x44, DHT on GPIO %d)",
              BoardPins::DHT_PIN);
    } else {
        LOG_I("temperature sensor: %s", sensorName(tempSensor.getType()));
    }
}

void loop() {
    static unsigned long lastBlink = 0;
    static unsigned long lastSound = 0;
    static unsigned long lastHt = 0;
    static bool ledOn = false;

    unsigned long now = millis();

    if (now - lastBlink >= 500) {
        lastBlink = now;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_LED, ledOn ? HIGH : LOW);
    }

    if (now - lastSound >= REPORT_INTERVAL_MS) {
        lastSound = now;
        SoundWindow w = readWindow(BoardPins::ADC_SOUND);
        LOG_I("sound p2p=%u mean=%lu", w.max - w.min, w.count ? w.sum / w.count : 0);
    }

    if (now - lastHt >= HT_INTERVAL_MS) {
        lastHt = now;

        if (!tempSensor.isInitialized()) {
            LOG_W("temperature sensor not present, skipping sample");
            return;
        }

        float temperature = 0.0f;
        float humidity = 0.0f;
        if (tempSensor.readWithRetry(temperature, humidity, 3, 100)) {
            LOG_I("%s temp=%.2f C hum=%.2f %%", sensorName(tempSensor.getType()),
                  temperature, humidity);
        } else {
            LOG_E("temperature read failed after retries");
        }
    }
}
