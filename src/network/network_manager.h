#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "utils/logger.h"

class NetworkManager {
public:
    static NetworkManager& getInstance() {
        static NetworkManager instance;
        return instance;
    }

    // Fast path with credentials from secrets.h, then a captive portal.
    bool initWiFi(int portalTimeoutSeconds = 180);

    // Broker connection; retried by loop() with backoff.
    bool initMQTT(const char* broker, int port, const char* username, const char* password);

    bool publishMessage(const char* topic, const char* payload, bool retained = false);
    bool subscribe(const char* topic, uint8_t qos = 1);
    void setCallback(void (*callback)(char*, byte*, unsigned int));

    void loop();

    bool isConnected() { return mqttClient.connected(); }
    bool isWifiConnected() { return WiFi.status() == WL_CONNECTED; }
    int32_t rssi() { return WiFi.RSSI(); }
    String ip() { return WiFi.localIP().toString(); }

    // MAC-derived, stable across reboots, unique per board.
    String generateDeviceID();

private:
    NetworkManager() : mqttClient(wifiClient) {}
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    bool connectWithSecrets(unsigned long timeoutMs);
    bool connectWithPortal(int timeoutSeconds);
    bool reconnectMQTT();

    WiFiClient wifiClient;
    PubSubClient mqttClient;

    const char* mqttBroker = nullptr;
    int mqttPort = 0;
    const char* mqttUsername = nullptr;
    const char* mqttPassword = nullptr;

    unsigned long lastReconnectAttempt = 0;
    unsigned long lastWiFiReconnectAttempt = 0;
    int mqttReconnectCount = 0;
    int wifiReconnectCount = 0;

    static const unsigned long WIFI_RECONNECT_INTERVAL = 30000;
    static const unsigned long MQTT_RECONNECT_INTERVAL = 5000;
    static const int MAX_WIFI_RETRIES = 3;
};
