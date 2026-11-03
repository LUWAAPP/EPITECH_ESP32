#pragma once

// Pin definitions for TTGO T7 V1.4 Mini32, as wired on the AQA shield.
//
// Derive these pins from the PCB continuity map (S2) before trusting them.
// Two board generations circulate in the course; the firmware must work on both:
//   - DHT sensors sit on GPIO 13;
//   - the SHT4x I2C bus uses SDA 13 / SCL 22.
// DHT_PIN and I2C_SDA therefore share GPIO 13. Only one of the two temperature
// families is populated on a given board, and autodetection (stage v0.4) decides
// which one answers.
//
// GPIO 34..39 are input-only on the ESP32: PMS TX must land on 34 or 35.

struct BoardPins {
    static const int DHT_PIN = 13;
    static const int I2C_SDA = 13;
    static const int I2C_SCL = 22;
    static const int ADC_SOUND = 35;
    static const int PM_TX = 34;
    static const int PM_RX = 14;
    static const int CO2_TX = 16;
    static const int CO2_RX = 17;
};
