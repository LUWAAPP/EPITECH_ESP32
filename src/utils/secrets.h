#pragma once

// Training defaults. This file is committed so every tag builds as cloned.
// Real credentials go in the gitignored secrets.local.h beside this file:
//
//     #define WIFI_SSID "my-lab"
//     #define WIFI_PASSWORD "not-a-real-password"
//     #define MQTT_BROKER "192.168.1.10"
//     #define MQTT_USERNAME "student"
//     #define MQTT_PASSWORD "not-a-real-password"
//
// Never commit a real password or a private key.

#define WIFI_SSID "CHANGE_ME"
#define WIFI_PASSWORD "CHANGE_ME"

// MQTT broker for the local Mosquitto instance (S10). Port 1883 is plaintext.
#define MQTT_BROKER "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USERNAME "student"
#define MQTT_PASSWORD "student"

// Identity and location defaults (Medellín, for the course).
#define SENSOR_PREFIX "aqa-"
#define GEOHASH_DEFAULT "d347"

// Metadata carried in status payloads; used by the OTA stage too.
#define HARDWARE_VERSION 14
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_URL "http://192.168.1.10:8000/firmware.bin"
#define FIRMWARE_SIG_URL "http://192.168.1.10:8000/firmware.sig"

#if __has_include("secrets.local.h")
#include "secrets.local.h"
#endif
