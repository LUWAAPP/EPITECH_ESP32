#pragma once

// Training defaults. This file is committed so every tag builds as cloned.
// Real credentials go in the gitignored secrets.local.h beside this file:
//
//     #define WIFI_SSID "my-lab"
//     #define WIFI_PASSWORD "not-a-real-password"
//
// Never commit a real password or a private key.

#define WIFI_SSID "CHANGE_ME"
#define WIFI_PASSWORD "CHANGE_ME"

// Identity and location defaults (Medellín, for the course).
#define SENSOR_PREFIX "aqa-"
#define GEOHASH_DEFAULT "d347"

#if __has_include("secrets.local.h")
#include "secrets.local.h"
#endif
