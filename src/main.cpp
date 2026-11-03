#include <Arduino.h>

#include "data/data_manager.h"
#include "hal/hal.h"
#include "mqtt/mqtt_protocol.h"
#include "network/network_manager.h"
#include "utils/config_manager.h"
#include "utils/logger.h"
#include "utils/task_manager.h"

// Stage v0.12-commands
//
// Goal: stop being a one-way device. The node answers a status request, moves
// its geohash, accepts runtime parameters and reboots on command.
//
// Lesson: a command channel needs subscriptions, a dispatcher and a reply. Each
// cmd/* topic is a contract: payload schema in, status reply out. QoS 1 on
// subscriptions means the broker retries delivery until the client acknowledges.
//
// NVS is the other half: geohash and runtime parameters survive reboots, so a
// command changes behaviour without reflashing.

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
}
