#include "mqtt_protocol.h"
#include "utils/config_manager.h"
#include "utils/runtime_config.h"
#include "network/network_manager.h"
#include "utils/logger.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

void MqttProtocol::begin() {
    RuntimeConfig::getInstance().begin();
}

String MqttProtocol::buildStatusPayload(
    const char* lastCommand,
    const char* commandStatus,
    int commandProgress
) const {
    DynamicJsonDocument doc(1024);
    doc["sensor_id"] = Config().getSensorId();
    doc["firmware_version"] = Config().getFirmwareVersion();
    doc["uptime"] = (int)(millis() / 1000);
    doc["ip"] = WiFi.localIP().toString();
    doc["boot_count"] = RuntimeConfig::getInstance().getBootCount();
    doc["timestamp"] = (int)(time(nullptr) > 0 ? time(nullptr) : (millis() / 1000));

    JsonObject params = doc.createNestedObject("parameters");
    RuntimeConfig::getInstance().getParametersJson(params);

    if (lastCommand && lastCommand[0])
        doc["last_command"] = lastCommand;
    if (commandStatus && commandStatus[0])
        doc["command_status"] = commandStatus;
    doc["command_progress"] = commandProgress;

    String out;
    serializeJson(doc, out);
    return out;
}

void MqttProtocol::publishStatus(
    const char* lastCommand,
    const char* commandStatus,
    int commandProgress
) {
    String payload = buildStatusPayload(lastCommand, commandStatus, commandProgress);
    NetworkManager::getInstance().publishMessage(
        Config().getStaTopic(),
        payload.c_str()
    );
}

void MqttProtocol::handleMqttMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr(topic);
    const char* pl = length > 0 ? (const char*)payload : "";
    if (length > 0 && payload) {
        for (unsigned int i = 0; i < length; i++) {
            if (payload[i] == '\0') break;
        }
    }
    LOG_I("MQTT message on topic %s", topic);

    if (topicStr == Config().getCmdStatusTopic()) {
        handleCmdStatus();
        return;
    }
    if (topicStr == Config().getCmdConfigTopic()) {
        handleCmdConfig(pl, length);
        return;
    }
    if (topicStr == Config().getCmdGeohashTopic()) {
        handleCmdGeohash(pl, length);
        return;
    }
    if (topicStr == Config().getCmdRestartTopic()) {
        handleCmdRestart();
        return;
    }
}

void MqttProtocol::subscribeCommandTopics() {
    NetworkManager& nm = NetworkManager::getInstance();
    const uint8_t qos = 1;
    nm.subscribe(Config().getCmdStatusTopic(), qos);
    nm.subscribe(Config().getCmdConfigTopic(), qos);
    nm.subscribe(Config().getCmdGeohashTopic(), qos);
    nm.subscribe(Config().getCmdRestartTopic(), qos);
}

void MqttProtocol::handleCmdStatus() {
    publishStatus("", "", 0);
}

void MqttProtocol::handleCmdConfig(const char* payload, unsigned int length) {
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, payload, length);
    JsonObject obj = doc.as<JsonObject>();
    if (err || obj.isNull()) {
        LOG_E("Invalid config payload");
        publishStatus("config", "failed", 0);
        return;
    }
    RuntimeConfig::getInstance().setParametersFromJson(obj);
    publishStatus("config", "completed", 0);
}

void MqttProtocol::handleCmdGeohash(const char* payload, unsigned int length) {
    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err || !doc.containsKey("geohash")) {
        LOG_E("Invalid geohash payload");
        publishStatus("geohash", "failed", 0);
        return;
    }
    const char* geohash = doc["geohash"].as<const char*>();
    if (geohash && geohash[0]) {
        RuntimeConfig::getInstance().setGeohash(geohash);
        LOG_I("Geohash set to %s", geohash);
    }
    publishStatus("geohash", "completed", 0);
}

void MqttProtocol::handleCmdRestart() {
    publishStatus("restart", "in_progress", 0);
    delay(200);
    ESP.restart();
}
