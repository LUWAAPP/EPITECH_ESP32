#include <Arduino.h>
#include <esp_task_wdt.h>

#include "data/data_manager.h"
#include "hal/hal.h"
#include "mqtt/mqtt_protocol.h"
#include "network/network_manager.h"
#include "utils/config_manager.h"
#include "utils/logger.h"
#include "utils/task_manager.h"

// Stage v0.13-reliability
//
// Goal: close the four gaps that make a node silently wrong — unsynchronised
// time, invisible node death, hung loops and frozen readings.
//
// Lesson: reliability is observable. `t` separates measurement time from
// delivery time; a retained Last Will leaves an offline marker when power dies;
// the watchdog reboots a hung loop instead of letting it pretend; three
// identical frames are flagged stale instead of republished as fresh.
//
// The command channel from v0.12 remains: status, config, geohash, restart.

static const int HEARTBEAT_LED = LED_BUILTIN;
static const unsigned long STATUS_INTERVAL_MS = 10000;
static const unsigned long HEARTBEAT_INTERVAL_MS = 60000;

static HAL& hal = HAL::getInstance();
static NetworkManager& network = NetworkManager::getInstance();
static MqttProtocol& mqtt = MqttProtocol::getInstance();

static const char* ok(bool value) { return value ? "PASS" : "FAIL"; }

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    mqtt.handleMqttMessage(topic, payload, length);
}

void setup() {
    Logger::getInstance().begin(115200, LOG_INFO);
    pinMode(HEARTBEAT_LED, OUTPUT);

    LOG_I("stage v0.12-commands on %s", ARDUINO_BOARD);

    String deviceId = network.generateDeviceID();
    Config().init(deviceId);
    LOG_I("sensor id %s, tele topic %s",
          Config().getSensorId(), Config().getDataTopic());

    mqtt.begin();  // loads boot count, subscribes nothing until connected

    if (!hal.init()) {
        LOG_W("some sensors failed to initialise; continuing with the rest");
    }

    LOG_I("self-test | temperature %s | co2 %s | pm %s | sound %s",
          ok(hal.isSensorInitialized(SENSOR_TEMPERATURE)),
          ok(hal.isSensorInitialized(SENSOR_CO2)),
          ok(hal.isSensorInitialized(SENSOR_PM)),
          ok(hal.isSensorInitialized(SENSOR_SOUND)));

    network.initWiFi();
    network.setCallback(mqttCallback);

    if (network.initMQTT(Config().getMqttBroker(), Config().getMqttPort(),
                         Config().getMqttUsername(), Config().getMqttPassword())) {
        mqtt.subscribeCommandTopics();
        mqtt.publishStatus("", "", 0);
    } else {
        LOG_W("broker unreachable now; loop() will keep retrying");
    }

    TaskManager::getInstance().init();
    DataManager::getInstance().init(hal, TaskManager::getInstance().getScheduler());

    // Watchdog: if the loop stops feeding it for 30 s, the board reboots. The
    // failure mode this catches is a hung driver or a blocked loop, not a
    // long-running task — those must yield.
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);
}

void loop() {
    static unsigned long lastBlink = 0;
    static unsigned long lastStatus = 0;
    static unsigned long lastHeartbeat = 0;
    static bool ledOn = false;
    static bool mqttWasConnected = false;

    unsigned long now = millis();

    if (now - lastBlink >= 500) {
        lastBlink = now;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_LED, ledOn ? HIGH : LOW);
    }

    network.loop();

    bool mqttConnected = network.isConnected();
    if (mqttConnected && !mqttWasConnected) {
        // Re-subscribe on every reconnect: subscriptions do not survive a session.
        mqtt.subscribeCommandTopics();
        mqtt.publishStatus("", "", 0);
    }
    mqttWasConnected = mqttConnected;

    if (mqttConnected && now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = now;
        mqtt.publishStatus("", "", 0);
    }

    if (now - lastStatus >= STATUS_INTERVAL_MS) {
        lastStatus = now;
        if (network.isWifiConnected()) {
            LOG_I("wifi ip=%s rssi=%d dBm mqtt=%s",
                  network.ip().c_str(), (int)network.rssi(),
                  mqttConnected ? "up" : "down");
        } else {
            LOG_W("wifi offline");
        }
    }

    TaskManager::getInstance().execute();
    hal.update();
    esp_task_wdt_reset();
}
