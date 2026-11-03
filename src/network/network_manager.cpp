#include "network_manager.h"

#include "utils/config_manager.h"
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
        LOG_I("WiFi connected ip=%s rssi=%d dBm", ip().c_str(), (int)rssi());
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

bool NetworkManager::initMQTT(const char* broker, int port,
                              const char* username, const char* password) {
    mqttBroker = broker;
    mqttPort = port;
    mqttUsername = username;
    mqttPassword = password;

    mqttClient.setServer(broker, port);
    return reconnectMQTT();
}

bool NetworkManager::reconnectMQTT() {
    if (mqttClient.connected() || mqttBroker == nullptr) {
        return mqttClient.connected();
    }

    String clientId = Config().getSensorId();
    LOG_I("MQTT connect %s:%d as %s", mqttBroker, mqttPort, clientId.c_str());

    bool connected;
    if (mqttUsername != nullptr && mqttPassword != nullptr) {
        connected = mqttClient.connect(clientId.c_str(), mqttUsername, mqttPassword);
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }

    if (connected) {
        LOG_I("MQTT connected");
        mqttReconnectCount = 0;
        return true;
    }

    mqttReconnectCount++;
    LOG_W("MQTT connect failed rc=%d (attempt %d)",
          mqttClient.state(), mqttReconnectCount);
    return false;
}

void NetworkManager::setCallback(void (*callback)(char*, byte*, unsigned int)) {
    mqttClient.setCallback(callback);
}

bool NetworkManager::subscribe(const char* topic, uint8_t qos) {
    if (!mqttClient.connected()) {
        LOG_W("cannot subscribe to %s: MQTT disconnected", topic);
        return false;
    }
    bool ok = mqttClient.subscribe(topic, qos);
    if (ok) {
        LOG_I("subscribed %s (QoS %d)", topic, qos);
    } else {
        LOG_E("subscribe failed: %s", topic);
    }
    return ok;
}

bool NetworkManager::publishMessage(const char* topic, const char* payload) {
    if (!mqttClient.connected()) {
        LOG_W("cannot publish to %s: MQTT disconnected", topic);
        return false;
    }
    bool ok = mqttClient.publish(topic, payload);
    if (ok) {
        LOG_D("published %s: %s", topic, payload);
    } else {
        LOG_E("publish failed: %s", topic);
    }
    return ok;
}

void NetworkManager::loop() {
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - lastWiFiReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
            lastWiFiReconnectAttempt = now;
            LOG_W("WiFi lost; reconnect attempt %d of %d",
                  wifiReconnectCount + 1, MAX_WIFI_RETRIES);
            WiFi.reconnect();

            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
                delay(250);
            }

            if (WiFi.status() == WL_CONNECTED) {
                LOG_I("WiFi reconnected ip=%s rssi=%d dBm",
                      ip().c_str(), (int)rssi());
                wifiReconnectCount = 0;
            } else {
                wifiReconnectCount++;
                if (wifiReconnectCount >= MAX_WIFI_RETRIES) {
                    LOG_E("%d failed reconnects; keeping the node alive", wifiReconnectCount);
                    wifiReconnectCount = 0;
                }
            }
        }
        return;
    }

    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            if (reconnectMQTT()) {
                lastReconnectAttempt = 0;
            }
        }
        return;
    }

    mqttClient.loop();
}
