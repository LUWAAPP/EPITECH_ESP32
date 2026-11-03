#include <Arduino.h>
#include <Wire.h>

#include "hal/board/ttgo_t7_v14.h"
#include "hal/sensors/co2_sensor.h"
#include "hal/sensors/temperature_sensor.h"
#include "utils/logger.h"

// Stage v0.5-co2-uart
//
// Goal: read a sensor that speaks UART, and survive it not answering.
//
// Lesson: UART is a byte stream with no framing of its own — the sensor's
// datasheet defines packets (start byte, length, payload, checksum) and a baud
// rate. The MH-Z19 and the SenseAir S8 use the same physical bus at 9600 8N1 but
// different protocols, so detection is by asking, not by assuming.
//
// CO2 sensors also need warm-up. The production firmware verifies a non-zero
// reading 30 s after init and reboots if the sensor is still silent; here the
// verification is visible in the log instead of a reboot.

static const unsigned long SAMPLE_WINDOW_MS = 50;
static const unsigned long REPORT_INTERVAL_MS = 500;
static const unsigned long HT_INTERVAL_MS = 2000;
static const unsigned long CO2_INTERVAL_MS = 2000;
static const int HEARTBEAT_LED = LED_BUILTIN;

static TemperatureSensor tempSensor;
static CO2Sensor co2Sensor;

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

static const char* temperatureName(TemperatureSensor::SensorType type) {
    switch (type) {
        case TemperatureSensor::SensorType::SHT40: return "SHT40";
        case TemperatureSensor::SensorType::DHT22: return "DHT22";
        case TemperatureSensor::SensorType::DHT11: return "DHT11";
        default: return "NONE";
    }
}

static const char* co2Name(CO2Sensor::SensorType type) {
    switch (type) {
        case CO2Sensor::SensorType::MHZ19: return "MHZ19";
        case CO2Sensor::SensorType::SENSEAIR_S8: return "S8";
        default: return "NONE";
    }
}

void setup() {
    Logger::getInstance().begin(115200, LOG_INFO);
    pinMode(HEARTBEAT_LED, OUTPUT);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    LOG_I("stage v0.5-co2-uart on %s", ARDUINO_BOARD);

    if (tempSensor.init(BoardPins::DHT_PIN, BoardPins::I2C_SCL,
                        TemperatureSensor::SensorType::NONE, true)) {
        LOG_I("temperature sensor: %s", temperatureName(tempSensor.getType()));
    } else {
        LOG_E("no temperature sensor detected");
    }

    // Serial2 defaults to RX 16 / TX 17 on the ESP32; the board header matches.
    LOG_I("probing CO2 sensor on Serial2 RX=%d TX=%d (9600 8N1)",
          BoardPins::CO2_RX, BoardPins::CO2_TX);
    if (co2Sensor.init(BoardPins::CO2_RX, BoardPins::CO2_TX, Serial2,
                       CO2Sensor::SensorType::NONE, true)) {
        LOG_I("CO2 sensor: %s", co2Name(co2Sensor.getType()));
    } else {
        LOG_E("no CO2 sensor answered; UART RX/TX may be swapped");
    }
}

void loop() {
    static unsigned long lastBlink = 0;
    static unsigned long lastSound = 0;
    static unsigned long lastHt = 0;
    static unsigned long lastCo2 = 0;
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
        if (tempSensor.isInitialized()) {
            float temperature = 0.0f;
            float humidity = 0.0f;
            if (tempSensor.readWithRetry(temperature, humidity, 3, 100)) {
                LOG_I("%s temp=%.2f C hum=%.2f %%",
                      temperatureName(tempSensor.getType()), temperature, humidity);
            } else {
                LOG_E("temperature read failed after retries");
            }
        }
    }

    if (now - lastCo2 >= CO2_INTERVAL_MS) {
        lastCo2 = now;
        if (co2Sensor.isInitialized()) {
            int co2 = 0;
            int8_t sensorTemp = 0;
            if (co2Sensor.readWithRetry(co2, sensorTemp, 5, 200)) {
                LOG_I("%s co2=%d ppm sensor_temp=%d C",
                      co2Name(co2Sensor.getType()), co2, sensorTemp);
            } else {
                LOG_W("CO2 read failed after retries (warm-up can take minutes)");
            }
        }
    }
}
