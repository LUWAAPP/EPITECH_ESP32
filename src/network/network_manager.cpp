#include "network_manager.h"

#include "utils/secrets.h"

String NetworkManager::generateDeviceID() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

bool NetworkManager::connectWithSecrets(unsigned long timeoutMs) {
    if (String(WIFI_SSID) == "CHANGE_ME") {
        LOG_I("no station credentials in secrets.local.h; using the config portal");
        return false;
    }

    LOG_I("connecting to SSID %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
}

bool NetworkManager::connectWithPortal(int timeoutSeconds) {
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(timeoutSeconds);

    LOG_I("opening config portal 'SenPork-setup' for %d s", timeoutSeconds);
    if (!wifiManager.autoConnect("SenPork-setup")) {
        LOG_E("config portal timed out without credentials");
        return false;
    }
    return WiFi.status() == WL_CONNECTED;
}

bool NetworkManager::initWiFi(int portalTimeoutSeconds) {
    if (connectWithSecrets(15000)) {
        LOG_I("WiFi connected ip=%s rssi=%d dBm",
              ip().c_str(), (int)rssi());
        wifiReconnectCount = 0;
        return true;
    }

    if (connectWithPortal(portalTimeoutSeconds)) {
        LOG_I("WiFi connected via portal ip=%s rssi=%d dBm",
              ip().c_str(), (int)rssi());
        wifiReconnectCount = 0;
        return true;
    }

    LOG_E("WiFi unavailable; sampling continues offline");
    return false;
}

void NetworkManager::loop() {
    if (WiFi.status() == WL_CONNECTED) {
        wifiReconnectCount = 0;
        return;
    }

    unsigned long now = millis();
    if (now - lastReconnectAttempt < WIFI_RECONNECT_INTERVAL) {
        return;
    }
    lastReconnectAttempt = now;

    LOG_W("WiFi lost; reconnect attempt %d of %d",
          wifiReconnectCount + 1, MAX_WIFI_RETRIES);
    WiFi.reconnect();

    // Give the association a bounded window before the next attempt.
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        LOG_I("WiFi reconnected ip=%s rssi=%d dBm", ip().c_str(), (int)rssi());
        wifiReconnectCount = 0;
        return;
    }

    wifiReconnectCount++;
    if (wifiReconnectCount >= MAX_WIFI_RETRIES) {
        // Production reboots here. The reboot policy is defended (or condemned)
        // with soak data at S12; the ladder keeps retrying and stays online for
        // local sampling.
        LOG_E("%d failed reconnects; keeping the node alive and retrying",
              wifiReconnectCount);
        wifiReconnectCount = 0;
    }
}
