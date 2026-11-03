#include <Arduino.h>

#include "data/data_manager.h"
#include "hal/hal.h"
#include "network/network_manager.h"
#include "utils/config_manager.h"
#include "utils/logger.h"
#include "utils/task_manager.h"

// Stage v0.11-mqtt
//
// Goal: publish the frame to a local Mosquitto broker using the AQA topic tree.
//
// Lesson: MQTT separates identity, topic and payload. The client id and the
// topic suffix are the MAC-derived sensor id; the topic also carries the geohash
// location (tele/{geohash}/{sensor_id}); the payload is the JSON frame from the
// previous stage. Publish happens every 15 s; a broker outage must not stop the
// sampler, it only drops frames until the connection returns.
//
// Topic tree:
//   tele/{geohash}/{sensor_id}   measurements
//   sta/{sensor_id}              status (heartbeat arrives at v0.12)
//   cmd/{command}/{sensor_id}    commands (v0.12)

static const int HEARTBEAT_LED = LED_BUILTIN;
static const unsigned long STATUS_INTERVAL_MS = 10000;

static HAL& hal = HAL::getInstance();
static NetworkManager& network = NetworkManager::getInstance();

static const char* ok(bool value) { return value ? "PASS" : "FAIL"; }

void setup() {
    Logger::getInstance().begin(115200, LOG_INFO);
    pinMode(HEARTBEAT_LED, OUTPUT);

    LOG_I("stage v0.11-mqtt on %s", ARDUINO_BOARD);

    String deviceId = network.generateDeviceID();
    Config().init(deviceId);
    LOG_I("sensor id %s, tele topic %s",
          Config().getSensorId(), Config().getDataTopic());

    if (!hal.init()) {
        LOG_W("some sensors failed to initialise; continuing with the rest");
    }

    LOG_I("self-test | temperature %s | co2 %s | pm %s | sound %s",
          ok(hal.isSensorInitialized(SENSOR_TEMPERATURE)),
          ok(hal.isSensorInitialized(SENSOR_CO2)),
          ok(hal.isSensorInitialized(SENSOR_PM)),
          ok(hal.isSensorInitialized(SENSOR_SOUND)));

    network.initWiFi();

    if (!network.initMQTT(Config().getMqttBroker(), Config().getMqttPort(),
                          Config().getMqttUsername(), Config().getMqttPassword())) {
        LOG_W("broker unreachable now; loop() will keep retrying");
    }

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
        if (network.isWifiConnected()) {
            LOG_I("wifi ip=%s rssi=%d dBm mqtt=%s",
                  network.ip().c_str(), (int)network.rssi(),
                  network.isConnected() ? "up" : "down");
        } else {
            LOG_W("wifi offline");
        }
    }

    network.loop();
    TaskManager::getInstance().execute();
    hal.update();
}
