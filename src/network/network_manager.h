#pragma once

#include <Arduino.h>
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

    // Reconnect with backoff; never reboots.
    void loop();

    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    int32_t rssi() const { return WiFi.RSSI(); }
    String ip() const { return WiFi.localIP().toString(); }

    // MAC-derived, stable across reboots, unique per board.
    String generateDeviceID();

private:
    NetworkManager() = default;
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    bool connectWithSecrets(unsigned long timeoutMs);
    bool connectWithPortal(int timeoutSeconds);

    unsigned long lastReconnectAttempt = 0;
    int wifiReconnectCount = 0;

    static const unsigned long WIFI_RECONNECT_INTERVAL = 30000;
    static const int MAX_WIFI_RETRIES = 3;
};
