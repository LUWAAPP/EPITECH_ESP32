#pragma once

#include <Arduino.h>

class ConfigManager;

extern ConfigManager& Config();

class ConfigManager {
public:
    void init(const String& deviceID);

    // Network Configuration
    const char* getWifiSSID() const { return wifiSSID; }
    const char* getWifiPassword() const { return wifiPassword; }

    // MQTT Configuration
    const char* getMqttBroker() const { return mqttBroker; }
    int getMqttPort() const { return mqttPort; }
    const char* getMqttUsername() const { return mqttUsername; }
    const char* getMqttPassword() const { return mqttPassword; }

    // Identity
    const char* getSensorId() const { return sensorId.c_str(); }

    // Topic Configuration (built at runtime from sensor_id)
    const char* getDataTopic() const { return dataTopic.c_str(); }
    const char* getStaTopic() const { return staTopic.c_str(); }
    const char* getCmdStatusTopic() const { return cmdStatusTopic.c_str(); }
    const char* getCmdOtaTopic() const { return cmdOtaTopic.c_str(); }
    const char* getCmdConfigTopic() const { return cmdConfigTopic.c_str(); }
    const char* getCmdRollbackTopic() const { return cmdRollbackTopic.c_str(); }
    const char* getCmdGeohashTopic() const { return cmdGeohashTopic.c_str(); }
    const char* getCmdRestartTopic() const { return cmdRestartTopic.c_str(); }

    // OTA Configuration
    const char* getFirmwareUrl() const { return firmwareUrl; }
    const char* getFirmwareSigUrl() const { return firmwareSigUrl; }

    // Device Configuration
    int getHardwareVersion() const { return hardwareVersion; }
    const char* getFirmwareVersion() const { return firmwareVersion; }

private:
    ConfigManager();
    friend ConfigManager& Config();

    const char* wifiSSID;
    const char* wifiPassword;

    const char* mqttBroker;
    int mqttPort;
    const char* mqttUsername;
    const char* mqttPassword;

    String sensorId;
    String dataTopic;
    String staTopic;
    String cmdStatusTopic;
    String cmdOtaTopic;
    String cmdConfigTopic;
    String cmdRollbackTopic;
    String cmdGeohashTopic;
    String cmdRestartTopic;

    const char* firmwareUrl;
    const char* firmwareSigUrl;

    int hardwareVersion;
    const char* firmwareVersion;
};
