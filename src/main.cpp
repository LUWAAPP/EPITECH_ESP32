#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>

#include "hal/board/ttgo_t7_v14.h"
#include "hal/hal.h"
#include "utils/logger.h"

// Stage v0.8-arduinojson
//
// Goal: produce the payload the network stage will publish, and understand what
// the JSON library costs.
//
// Lesson: ArduinoJson serialises into a fixed-capacity document. Capacity is a
// budget: too small and fields are silently dropped; too large and RAM is wasted.
// The AQA frame is an array of single-key objects ( {
// "fields":[{"snd":..},{"co2":..},...] } ) — a Telegraf-friendly shape that is
// also why every field name repeats in the payload.
//
// The round-trip check below is the cheap contract test: build, serialise,
// deserialise, compare. If it fails, the payload never reaches a broker intact.

static const unsigned long REPORT_INTERVAL_MS = 500;
static const unsigned long SLOW_INTERVAL_MS = 2000;
static const unsigned long FRAME_INTERVAL_MS = 5000;
static const int HEARTBEAT_LED = LED_BUILTIN;
static const size_t FRAME_CAPACITY = 256;

static HAL& hal = HAL::getInstance();

// Latest readings. Production stores sound/PM/CO2 as windowed averages; at this
// stage every value is the instantaneous last sample.
static unsigned short latestSound = 0;
static unsigned short latestCo2 = 0;
static unsigned short latestPm25 = 0;
static unsigned short latestHumidity = 0;
static unsigned short latestTemperature = 0;

static const char* ok(bool value) { return value ? "PASS" : "FAIL"; }

static String buildFrame() {
    DynamicJsonDocument doc(FRAME_CAPACITY);
    doc["fields"][0]["snd"] = latestSound;
    doc["fields"][1]["co2"] = latestCo2;
    doc["fields"][2]["pm25"] = latestPm25;
    doc["fields"][3]["hum"] = latestHumidity;
    doc["fields"][4]["temp"] = latestTemperature;

    String out;
    serializeJson(doc, out);
    return out;
}

static bool roundTripMatches(const String& frame) {
    DynamicJsonDocument doc(FRAME_CAPACITY);
    if (deserializeJson(doc, frame) != DeserializationError::Ok) {
        return false;
    }
    return doc["fields"][0]["snd"].as<unsigned short>() == latestSound &&
           doc["fields"][1]["co2"].as<unsigned short>() == latestCo2 &&
           doc["fields"][2]["pm25"].as<unsigned short>() == latestPm25 &&
           doc["fields"][3]["hum"].as<unsigned short>() == latestHumidity &&
           doc["fields"][4]["temp"].as<unsigned short>() == latestTemperature;
}

static void demonstrateTruncation() {
    // 5 keys plus nesting do not fit in 32 bytes: ArduinoJson drops what it
    // cannot store and serialises a shorter, still-valid-looking document.
    DynamicJsonDocument tiny(32);
    tiny["fields"][0]["snd"] = 4321;
    tiny["fields"][1]["co2"] = 1234;
    tiny["fields"][2]["pm25"] = 56;
    tiny["fields"][3]["hum"] = 78;
    tiny["fields"][4]["temp"] = 21;

    String out;
    serializeJson(tiny, out);
    LOG_W("tiny doc capacity=%u used=%u overflowed=%d payload=%s",
          (unsigned)tiny.capacity(), (unsigned)tiny.memoryUsage(),
          tiny.overflowed() ? 1 : 0, out.c_str());
}

void setup() {
    Logger::getInstance().begin(115200, LOG_INFO);
    pinMode(HEARTBEAT_LED, OUTPUT);

    LOG_I("stage v0.8-arduinojson on %s", ARDUINO_BOARD);

    if (!hal.init()) {
        LOG_W("some sensors failed to initialise; continuing with the rest");
    }

    LOG_I("self-test | temperature %s | co2 %s | pm %s | sound %s",
          ok(hal.isSensorInitialized(SENSOR_TEMPERATURE)),
          ok(hal.isSensorInitialized(SENSOR_CO2)),
          ok(hal.isSensorInitialized(SENSOR_PM)),
          ok(hal.isSensorInitialized(SENSOR_SOUND)));

    demonstrateTruncation();
}

void loop() {
    static unsigned long lastBlink = 0;
    static unsigned long lastFast = 0;
    static unsigned long lastSlow = 0;
    static unsigned long lastFrame = 0;
    static bool ledOn = false;

    unsigned long now = millis();

    if (now - lastBlink >= 500) {
        lastBlink = now;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_LED, ledOn ? HIGH : LOW);
    }

    if (now - lastFast >= REPORT_INTERVAL_MS) {
        lastFast = now;
        if (hal.isSensorInitialized(SENSOR_SOUND)) {
            unsigned int p2p = 0;
            if (hal.getSoundSensor().readWithRetry(p2p, 3, 50)) {
                latestSound = (unsigned short)p2p;
                LOG_D("sound p2p=%u", latestSound);
            } else {
                LOG_W("sound read failed");
            }
        }
    }

    if (now - lastSlow >= SLOW_INTERVAL_MS) {
        lastSlow = now;

        if (hal.isSensorInitialized(SENSOR_TEMPERATURE)) {
            float temperature = 0.0f;
            float humidity = 0.0f;
            if (hal.getTemperatureSensor().readWithRetry(temperature, humidity, 3, 100)) {
                // Casting to integers here mirrors production; stage v0.13
                // revisits it as a metrology defect.
                latestTemperature = (unsigned short)temperature;
                latestHumidity = (unsigned short)humidity;
                LOG_D("temp=%.2f C hum=%.2f %%", temperature, humidity);
            } else {
                LOG_E("temperature read failed");
            }
        }

        if (hal.isSensorInitialized(SENSOR_CO2)) {
            int co2 = 0;
            int8_t sensorTemp = 0;
            if (hal.getCO2Sensor().readWithRetry(co2, sensorTemp, 5, 100)) {
                latestCo2 = (unsigned short)co2;
                LOG_D("co2=%d ppm sensor_temp=%d C", co2, sensorTemp);
            } else {
                LOG_W("CO2 read failed");
            }
        }

        if (hal.isSensorInitialized(SENSOR_PM)) {
            uint16_t pm25 = 0;
            if (hal.getPMSensor().readWithRetry(pm25, 3, 100)) {
                latestPm25 = pm25;
                LOG_D("pm2.5=%u ug/m3", pm25);
            } else {
                LOG_W("PM read failed");
            }
        }
    }

    if (now - lastFrame >= FRAME_INTERVAL_MS) {
        lastFrame = now;
        String frame = buildFrame();
        LOG_I("frame %s", frame.c_str());
        LOG_I("frame round-trip %s", roundTripMatches(frame) ? "OK" : "MISMATCH");
    }

    hal.update();
}
