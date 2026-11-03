#include <Arduino.h>

#include "data/data_manager.h"
#include "hal/hal.h"
#include "network/network_manager.h"
#include "utils/logger.h"
#include "utils/secrets.h"
#include "utils/task_manager.h"

// Stage v0.10-wifi
//
// Goal: join the network, keep a stable identity, and survive an access point
// that comes and goes.
//
// Lesson: Wi-Fi association is a state machine, not a function call. Credentials
// can come from firmware (secrets.local.h) or from a captive portal persisted in
// NVS. The device ID is derived from the MAC address so it survives reflashing;
// the human-readable name is prefix + ID. RSSI is logged with every status line
// because "connected" and "usable" are different claims.
//
// Production reboots after three failed reconnects. That policy is defended with
// soak data at S12; here the node keeps sampling offline and retries forever.

static const int HEARTBEAT_LED = LED_BUILTIN;
static const unsigned long STATUS_INTERVAL_MS = 10000;

static HAL& hal = HAL::getInstance();
static NetworkManager& network = NetworkManager::getInstance();

static const char* ok(bool value) { return value ? "PASS" : "FAIL"; }

void setup() {
    Logger::getInstance().begin(115200, LOG_INFO);
    pinMode(HEARTBEAT_LED, OUTPUT);

    LOG_I("stage v0.10-wifi on %s", ARDUINO_BOARD);

    if (!hal.init()) {
        LOG_W("some sensors failed to initialise; continuing with the rest");
    }

    LOG_I("self-test | temperature %s | co2 %s | pm %s | sound %s",
          ok(hal.isSensorInitialized(SENSOR_TEMPERATURE)),
          ok(hal.isSensorInitialized(SENSOR_CO2)),
          ok(hal.isSensorInitialized(SENSOR_PM)),
          ok(hal.isSensorInitialized(SENSOR_SOUND)));

    String deviceId = network.generateDeviceID();
    LOG_I("device id %s%s (MAC-derived)", SENSOR_PREFIX, deviceId.c_str());

    network.initWiFi();

    TaskManager::getInstance().init();
    DataManager::getInstance().init(hal, TaskManager::getInstance().getScheduler());
}

void loop() {
    static unsigned long lastBlink = 0;
    static unsigned long lastStatus = 0;
    static bool ledOn = false;

    unsigned long now = millis();

    if (now - lastBlink >= 500) {
        lastBlink = now;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_LED, ledOn ? HIGH : LOW);
    }

    if (now - lastStatus >= STATUS_INTERVAL_MS) {
        lastStatus = now;
        if (network.isConnected()) {
            LOG_I("wifi ip=%s rssi=%d dBm", network.ip().c_str(), (int)network.rssi());
        } else {
            LOG_W("wifi offline");
        }
    }

    network.loop();
    TaskManager::getInstance().execute();
    hal.update();
}
