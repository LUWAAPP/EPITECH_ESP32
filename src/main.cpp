#include <Arduino.h>
#include <Wire.h>

#include "hal/board/ttgo_t7_v14.h"
#include "hal/hal.h"
#include "utils/logger.h"

// Stage v0.7-hal
//
// Goal: stop growing main.cpp. The HAL owns the four drivers, their retries and
// their status; the application asks for values and is told which sensors exist.
//
// Lesson: a hardware abstraction layer is a seam, not a religion. It centralises
// pin knowledge, gives every sensor the same "initialised? read? error?" shape,
// and lets one missing sensor degrade the node instead of killing it.
//
// Session S5 acceptance is met here: four drivers, timestamped output, startup
// self-test, explicit failure states.

static const unsigned long REPORT_INTERVAL_MS = 500;
static const unsigned long SLOW_INTERVAL_MS = 2000;
static const int HEARTBEAT_LED = LED_BUILTIN;

static HAL& hal = HAL::getInstance();

static const char* ok(bool value) { return value ? "PASS" : "FAIL"; }

void setup() {
    Logger::getInstance().begin(115200, LOG_INFO);
    pinMode(HEARTBEAT_LED, OUTPUT);

    LOG_I("stage v0.7-hal on %s", ARDUINO_BOARD);

    if (!hal.init()) {
        LOG_W("some sensors failed to initialise; continuing with the rest");
    }

    LOG_I("self-test | temperature %s | co2 %s | pm %s | sound %s",
          ok(hal.isSensorInitialized(SENSOR_TEMPERATURE)),
          ok(hal.isSensorInitialized(SENSOR_CO2)),
          ok(hal.isSensorInitialized(SENSOR_PM)),
          ok(hal.isSensorInitialized(SENSOR_SOUND)));
}

void loop() {
    static unsigned long lastBlink = 0;
    static unsigned long lastFast = 0;
    static unsigned long lastSlow = 0;
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
                LOG_I("sound p2p=%u", p2p);
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
                LOG_I("temp=%.2f C hum=%.2f %%", temperature, humidity);
            } else {
                LOG_E("temperature read failed");
            }
        }

        if (hal.isSensorInitialized(SENSOR_CO2)) {
            int co2 = 0;
            int8_t sensorTemp = 0;
            if (hal.getCO2Sensor().readWithRetry(co2, sensorTemp, 5, 100)) {
                LOG_I("co2=%d ppm sensor_temp=%d C", co2, sensorTemp);
            } else {
                LOG_W("CO2 read failed");
            }
        }

        if (hal.isSensorInitialized(SENSOR_PM)) {
            uint16_t pm25 = 0;
            if (hal.getPMSensor().readWithRetry(pm25, 3, 100)) {
                LOG_I("pm2.5=%u ug/m3", pm25);
            } else {
                LOG_W("PM read failed");
            }
        }
    }

    hal.update();
}
