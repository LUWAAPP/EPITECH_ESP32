#include <Arduino.h>
#include <SensirionI2cSht4x.h>
#include <Wire.h>

#include "hal/board/ttgo_t7_v14.h"
#include "utils/logger.h"

// Stage v0.3-i2c
//
// Goal: talk to a device on a two-wire bus, and prove the bus is alive before
// blaming the driver.
//
// Lesson: I2C is an addressed bus. A scan tells you which addresses respond; the
// datasheet tells you which address the part should use. The SHT4x answers at
// 0x44. If the scan is empty, the problem is wiring/power, not the library.
//
// The board header is introduced here: from now on pins have names, not magic
// numbers. It is also where the DHT/I2C pin-13 conflict is documented.

static const unsigned long SAMPLE_WINDOW_MS = 50;
static const unsigned long REPORT_INTERVAL_MS = 500;
static const unsigned long HT_INTERVAL_MS = 2000;
static const int HEARTBEAT_LED = LED_BUILTIN;

static SensirionI2cSht4x sht4x;

struct SoundWindow {
    uint16_t min;
    uint16_t max;
    uint32_t sum;
    uint32_t count;
};

static SoundWindow readWindow(int pin) {
    SoundWindow w = {4095, 0, 0, 0};
    unsigned long start = millis();
    while (millis() - start < SAMPLE_WINDOW_MS) {
        uint16_t sample = analogRead(pin);
        if (sample > w.max) w.max = sample;
        if (sample < w.min) w.min = sample;
        w.sum += sample;
        w.count++;
    }
    return w;
}

static void scanI2C() {
    LOG_I("scanning I2C bus SDA=%d SCL=%d", BoardPins::I2C_SDA, BoardPins::I2C_SCL);
    int found = 0;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            LOG_I("  device at 0x%02X", address);
            found++;
        }
    }
    LOG_I("scan complete: %d device(s)", found);
}

void setup() {
    Logger::getInstance().begin(115200, LOG_INFO);
    pinMode(HEARTBEAT_LED, OUTPUT);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    Wire.begin(BoardPins::I2C_SDA, BoardPins::I2C_SCL);
    scanI2C();

    sht4x.begin(Wire, 0x44);
    uint32_t serial = 0;
    int16_t err = sht4x.serialNumber(serial);
    if (err) {
        LOG_E("SHT4x did not answer (error %d); check 0x44 in the scan", err);
    } else {
        LOG_I("SHT4x serial 0x%08lX", (unsigned long)serial);
    }

    LOG_I("stage v0.3-i2c on %s", ARDUINO_BOARD);
}

void loop() {
    static unsigned long lastBlink = 0;
    static unsigned long lastSound = 0;
    static unsigned long lastHt = 0;
    static bool ledOn = false;

    unsigned long now = millis();

    if (now - lastBlink >= 500) {
        lastBlink = now;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_LED, ledOn ? HIGH : LOW);
    }

    if (now - lastSound >= REPORT_INTERVAL_MS) {
        lastSound = now;
        SoundWindow w = readWindow(BoardPins::ADC_SOUND);
        LOG_I("sound p2p=%u mean=%lu", w.max - w.min, w.count ? w.sum / w.count : 0);
    }

    if (now - lastHt >= HT_INTERVAL_MS) {
        lastHt = now;

        float temperature = 0.0f;
        float humidity = 0.0f;
        uint16_t err = sht4x.measureHighPrecision(temperature, humidity);
        if (err || isnan(temperature) || isnan(humidity)) {
            LOG_E("SHT4x read failed (error %u)", err);
        } else {
            LOG_I("SHT4x temp=%.2f C hum=%.2f %%", temperature, humidity);
        }
    }
}
