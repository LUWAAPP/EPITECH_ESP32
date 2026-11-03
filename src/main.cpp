#include <Arduino.h>

#include "data/data_manager.h"
#include "hal/hal.h"
#include "utils/logger.h"
#include "utils/task_manager.h"

// Stage v0.9-scheduler
//
// Goal: separate "when to sample" from "what to sample", without a single
// blocking sleep in the loop.
//
// Lesson: every sensor has its own natural cadence. The microphone is sampled
// every 120 ms because it measures an envelope; the CO2 sensor every 2 s; the
// temperature sensor every 15 s; a frame leaves every 15 s. TaskScheduler turns
// those constants into independent tasks, and each callback pushes a sample into
// a window. createDataFrame() averages the windows and clears them.
//
// This is the S6+7 measurement discipline in code: the reported value is the
// mean over a window, not one lucky reading.

static const int HEARTBEAT_LED = LED_BUILTIN;

static HAL& hal = HAL::getInstance();

static const char* ok(bool value) { return value ? "PASS" : "FAIL"; }

void setup() {
    Logger::getInstance().begin(115200, LOG_INFO);
    pinMode(HEARTBEAT_LED, OUTPUT);

    LOG_I("stage v0.9-scheduler on %s", ARDUINO_BOARD);

    if (!hal.init()) {
        LOG_W("some sensors failed to initialise; continuing with the rest");
    }

    LOG_I("self-test | temperature %s | co2 %s | pm %s | sound %s",
          ok(hal.isSensorInitialized(SENSOR_TEMPERATURE)),
          ok(hal.isSensorInitialized(SENSOR_CO2)),
          ok(hal.isSensorInitialized(SENSOR_PM)),
          ok(hal.isSensorInitialized(SENSOR_SOUND)));

    TaskManager::getInstance().init();
    DataManager::getInstance().init(hal, TaskManager::getInstance().getScheduler());
}

void loop() {
    static unsigned long lastBlink = 0;
    static bool ledOn = false;

    unsigned long now = millis();
    if (now - lastBlink >= 500) {
        lastBlink = now;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_LED, ledOn ? HIGH : LOW);
    }

    TaskManager::getInstance().execute();
    hal.update();
}
