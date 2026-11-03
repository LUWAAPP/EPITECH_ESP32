# Firmware ladder — tag map

15 annotated tags. Each tag is a working tree that compiles, with exactly one new
idea and one observable acceptance test. Libraries appear in `platformio.ini`
only when the tag needs them.

Checkout a tag, build, flash, and read the serial console:

```bash
git checkout v0.7-hal
pio run -e ttgo-t7-v14-mini32
pio run -e ttgo-t7-v14-mini32 --target upload
pio device monitor
```

Verify every tag builds:

```bash
tools/build-all-tags.sh
```

## Map

| # | Tag | New idea | Hardware | Session |
|---|-----|----------|----------|---------|
| 0 | `v0.0-toolchain` | PlatformIO env, `setup`/`loop`, serial banner | none | S5 Mission 0 |
| 1 | `v0.1-timing` | `millis()` instead of `delay()`, logger levels, LED heartbeat | onboard LED (19) | S5 |
| 2 | `v0.2-adc` | ADC attenuation, windowed peak-to-peak | MAX9814 on 35 | S5/S6 |
| 3 | `v0.3-i2c` | `Wire` bus scan, SHT40, board pin header | SHT40 on 13/22 | S4 oracle / S5 |
| 4 | `v0.4-temp-autodetect` | Abstract driver, SHT40/DHT22/DHT11 autodetect | DHT on 13 | S5 |
| 5 | `v0.5-co2-uart` | `HardwareSerial`, MH-Z19 vs S8, warm-up verification | CO2 on 16/17 | S5 |
| 6 | `v0.6-pm-uart` | `SoftwareSerial`, PMS7003 frames, read timeout | PMS on 34/14 | S5 |
| 7 | `v0.7-hal` | HAL facade, status bitmap, init retries | all four | **S5 v0.1 met** |
| 8 | `v0.8-arduinojson` | `DynamicJsonDocument` sizing, nesting, round-trip, truncation | none | S6+7 |
| 9 | `v0.9-scheduler` | TaskScheduler windows, averaging, frame on serial | all four | S6+7 |
| 10 | `v0.10-wifi` | WiFiManager portal, MAC-derived ID, RSSI, backoff | +AP | S8 |
| 11 | `v0.11-mqtt` | PubSubClient, `tele/` + `sta/`, heartbeat, QoS 1 | +Mosquitto | S10 |
| 12 | `v0.12-commands` | four `cmd/*` handlers (six by v1.0), NVS runtime config | | S11 |
| 13 | `v0.13-reliability` | NTP `t`, LWT, watchdog, frozen-value flag | | S11/S12 |
| 14 | `v1.0-ota` | RSA-2048 signing, `cmd/ota`, verify, rollback | | S12 capstone |

## Session contract

- **S5 minimum (firmware v0.1):** ≥2 sensor drivers, timestamped output, startup
  self-test, explicit error states. Reached at `v0.7-hal`.
- **S6+7:** measurement windows and averaging, no network. `v0.8`, `v0.9`.
- **S10:** topic tree `tele/{geohash}/{sensor_id}`, `sta/{sensor_id}`, JSON
  payloads, QoS 1, heartbeat. `v0.11-mqtt`.
- **S11:** NTP timestamps in telemetry, retained LWT, device registry. `v0.13`.
- **S12:** reliability baseline (watchdog, frozen values) and signed OTA. `v1.0`.

Reveal policy: tags are the reveal. Do not hand out `v0.8` and later before the
matching mission is submitted; the diff between a student's branch and the tag is
the review material.

## Deliberate exclusions

The ladder implements the device, not the fleet infrastructure. Out of scope, and
left as student work: store-and-forward queue, TLS 8883 with per-device
credentials, and defence of the reboot-after-three-failures policy.
