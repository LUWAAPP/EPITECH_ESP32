#include "config_manager.h"
#include "secrets.h"

ConfigManager::ConfigManager() :
    wifiSSID(WIFI_SSID),
    wifiPassword(WIFI_PASSWORD),
    mqttBroker(MQTT_BROKER),
    mqttPort(MQTT_PORT),
    mqttUsername(MQTT_USERNAME),
    mqttPassword(MQTT_PASSWORD),
    firmwareUrl(FIRMWARE_URL),
    firmwareSigUrl(FIRMWARE_SIG_URL),
    hardwareVersion(HARDWARE_VERSION),
    firmwareVersion(FIRMWARE_VERSION)
{
}

void ConfigManager::init(const String& deviceID) {
    sensorId = String(SENSOR_PREFIX) + deviceID;

    dataTopic       = String("tele/") + GEOHASH_DEFAULT + "/" + sensorId;
    staTopic        = String("sta/") + sensorId;
    cmdStatusTopic  = String("cmd/status/") + sensorId;
    cmdOtaTopic     = String("cmd/ota/") + sensorId;
    cmdConfigTopic  = String("cmd/config/") + sensorId;
    cmdRollbackTopic= String("cmd/rollback/") + sensorId;
    cmdGeohashTopic = String("cmd/geohash/") + sensorId;
    cmdRestartTopic = String("cmd/restart/") + sensorId;
}

ConfigManager& Config() {
    static ConfigManager instance;
    return instance;
}
