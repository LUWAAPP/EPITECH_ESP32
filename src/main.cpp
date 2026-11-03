#include <Arduino.h>

#include "utils/logger.h"

// Stage v0.1-timing
//
// Goal: keep the loop responsive. The naive version of this program is
//   digitalWrite(led, HIGH); delay(500); digitalWrite(led, LOW); delay(500);
// which blocks everything else for half a second at a time. Once sensors and
// networking exist, blocking means missed samples and dropped connections.
//
// Lesson: millis() is a free-running counter; compare timestamps instead of
// sleeping. The LED and the log line run independently, at different periods,
// from the same loop.

static const int HEARTBEAT_LED = LED_BUILTIN;  // 19 on this board variant
static const unsigned long BLINK_INTERVAL_MS = 500;
static const unsigned long REPORT_INTERVAL_MS = 1000;

void setup() {
    // Logger::begin() owns Serial.begin() and prints the startup banner.
    Logger::getInstance().begin(115200, LOG_INFO);
    pinMode(HEARTBEAT_LED, OUTPUT);

    LOG_I("stage v0.1-timing on %s", ARDUINO_BOARD);
    LOG_I("heartbeat LED on GPIO %d", HEARTBEAT_LED);
    LOG_I("blink %lu ms, report %lu ms", BLINK_INTERVAL_MS, REPORT_INTERVAL_MS);
}

void loop() {
    static unsigned long lastBlink = 0;
    static unsigned long lastReport = 0;
    static unsigned long reports = 0;
    static bool ledOn = false;

    unsigned long now = millis();

    if (now - lastBlink >= BLINK_INTERVAL_MS) {
        lastBlink = now;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_LED, ledOn ? HIGH : LOW);
    }

    if (now - lastReport >= REPORT_INTERVAL_MS) {
        lastReport = now;
        reports++;
        LOG_I("uptime=%lu s reports=%lu", now / 1000, reports);
    }
}
