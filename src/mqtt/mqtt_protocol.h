#pragma once

#include <Arduino.h>

class MqttProtocol {
public:
    static MqttProtocol& getInstance() {
        static MqttProtocol instance;
        return instance;
    }

    void begin();

    /** Build AQA status payload JSON (sta topic). */
    String buildStatusPayload(
        const char* lastCommand,
        const char* commandStatus,
        int commandProgress
    ) const;

    /** Publish status JSON to sta/{sensor_id}. Used by OTA and command handlers. */
    void publishStatus(
        const char* lastCommand,
        const char* commandStatus,
        int commandProgress
    );

    /** Handle incoming MQTT message; dispatch by topic. */
    void handleMqttMessage(char* topic, byte* payload, unsigned int length);

    /** Subscribe to all command topics (call after MQTT connected). */
    void subscribeCommandTopics();

private:
    MqttProtocol() = default;
    MqttProtocol(const MqttProtocol&) = delete;
    MqttProtocol& operator=(const MqttProtocol&) = delete;

    void handleCmdStatus();
    void handleCmdOta(const char* payload, unsigned int length);
    void handleCmdConfig(const char* payload, unsigned int length);
    void handleCmdRollback();
    void handleCmdGeohash(const char* payload, unsigned int length);
    void handleCmdRestart();
};
