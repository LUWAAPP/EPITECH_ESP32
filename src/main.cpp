#include <Arduino.h>

// Stage v0.0-toolchain
//
// Goal: prove the toolchain path end to end — compile, flash, monitor — before
// any sensor code exists. If nothing prints, the problem is the toolchain or the
// cable, not the firmware.
//
// Lesson: setup() runs once, loop() runs forever; Serial carries diagnostics.

static const unsigned long BANNER_INTERVAL_MS = 5000;

void setup() {
    Serial.begin(115200);
    delay(200);  // let the USB bridge settle before the first byte

    Serial.println();
    Serial.println("==========================================");
    Serial.println("  SenPork firmware ladder — stage 0");
    Serial.println("==========================================");
    Serial.printf("tag        : v0.0-toolchain\n");
    Serial.printf("compiled   : %s %s\n", __DATE__, __TIME__);
    Serial.printf("board      : %s\n", ARDUINO_BOARD);
    Serial.printf("cpu        : %u MHz\n", getCpuFrequencyMhz());
    Serial.println("------------------------------------------");
}

void loop() {
    static unsigned long lastBanner = 0;

    unsigned long now = millis();
    if (now - lastBanner >= BANNER_INTERVAL_MS) {
        lastBanner = now;
        Serial.printf("alive after %lu s\n", now / 1000);
    }
}
