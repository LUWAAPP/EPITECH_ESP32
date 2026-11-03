# SenPork firmware ladder

Progressive Arduino/ESP32 course repository for the AQA smart sensor node.
Start at `v0.0-toolchain` and climb: each tag adds one idea until the device
acquires all four sensors, publishes telemetry, accepts commands, survives faults,
and updates itself over signed OTA.

Reference: `daquina-io/aqav80` @ `improve-mqtt`, `SenPork_firmware/`.
Board: TTGO T7 V1.4 Mini32 (`ttgo-t7-v14-mini32`), Arduino framework, 4 MB flash.

## Quick start

```bash
pio run -e ttgo-t7-v14-mini32                        # build
pio run -e ttgo-t7-v14-mini32 --target upload        # flash
pio device monitor                                    # 115200, raw
```

Requirements: PlatformIO Core or the VS Code extension, a data-capable USB cable
and, on Linux, membership of the `dialout` group.

Every tag builds: `tools/build-all-tags.sh` builds all of them in isolated
worktrees. The full tag map, session mapping and reveal policy live in
[docs/ladder.md](docs/ladder.md).

## Secrets

`src/utils/secrets.h` ships with training defaults so every tag compiles as
cloned. Real credentials belong in a gitignored `secrets.local.h` beside it;
`secrets.h` includes that file when present. Never commit a real password or a
private key.

## Stage 00 — `v0.0-toolchain`

Nothing but the toolchain. The firmware prints a banner with its compile time and
board name, then a liveness line every 5 s.

Acceptance:

- `pio run` ends in SUCCESS;
- `pio device monitor` shows the banner and `alive after N s` lines;
- the compile timestamp changes after an edit — proof the binary you flashed is
  the one you just built.

If the port is not detected: `ls /dev/ttyUSB* /dev/ttyACM*`, and set
`upload_port` in `platformio.ini` only if auto-detection fails.

## Stage 01 — `v0.1-timing`

Non-blocking timing. `delay()` stops the whole CPU; `millis()` lets independent
periodic jobs share one loop. The heartbeat LED blinks at 1 Hz while the log line
fires at 1 Hz, independently.

Acceptance:

- LED blinks 500 ms on / 500 ms off;
- `uptime` and `reports` still advance while the LED blinks;
- log lines are prefixed with `[seconds.milliseconds]` and a level.

## Stage 02 — `v0.2-adc`

Analog input done honestly: 12-bit resolution, 11 dB attenuation, and a 50 ms
peak-to-peak window instead of a single `analogRead()`. GPIO 35 is input-only on
the ESP32.

Acceptance:

- silence gives `p2p` near 0; a clap spikes it;
- `mean` stays near the microphone's DC bias regardless of loudness;
- the LED keeps blinking — the 50 ms window is the only blocking section.

## Stage 03 — `v0.3-i2c`

The bus before the driver: scan I2C, confirm `0x44`, then read the SHT4x. The
board pin header appears here, including the pin-13 conflict between the DHT data
line and the I2C SDA line.

Acceptance:

- scan lists `0x44`;
- SHT4x serial number and plausible temperature/humidity print every 2 s;
- unplugging SDA produces an empty scan and an explicit read error, not a hang.

## Stage 04 — `v0.4-temp-autodetect`

One image, two board generations: an abstract `ITemperatureSensor` with SHT4x,
DHT22 and DHT11 implementations, probed in that order. Failure to find any sensor
is an explicit state, not a crash.

Acceptance:

- the detected type is logged (`SHT40`, `DHT22`, `DHT11` or none);
- readings are plausible and refresh every 2 s;
- with no sensor the loop keeps running with a warning.

## Stage 05 — `v0.5-co2-uart`

UART sensors. Both CO2 families share 9600 8N1 but not the protocol; the firmware
asks instead of assuming. Warm-up is real: an early 0 ppm is a warm-up symptom,
not necessarily a dead sensor.

Acceptance:

- the detected CO2 family is logged;
- ppm is plausible and reacts to exhaled air;
- swapped RX/TX yields an explicit failure message, no hang.

## Stage 06 — `v0.6-pm-uart`

A second UART device, on SoftwareSerial. GPIO 34 is input-only, which is fine for
a receive-only PMS7003. Reads are bounded by a one-second timeout so a missing
sensor cannot freeze the loop.

Acceptance:

- PM2.5 prints every 2 s;
- disturbing dust near the inlet moves the value;
- unplugging the PMS data line produces a timeout warning while the other sensors
  keep reporting.

## Stage 07 — `v0.7-hal`

`main.cpp` stops accumulating drivers. The HAL owns the four sensors, their
retries and a status bitmap; the application reads through `HAL` and gets an
explicit PASS/FAIL self-test. This is the S5 firmware v0.1 contract: ≥2 drivers,
timestamped output, self-test, explicit error states.

One deliberate change from production: a CO2 sensor stuck at 0 ppm after warm-up
logs an error instead of rebooting, so a bench without that sensor cannot
boot-loop.

Acceptance:

- self-test line matches the populated sensors;
- one missing sensor does not stop the others;
- no pin numbers remain in `main.cpp`.

## Stage 08 — `v0.8-arduinojson`

The AQA payload shape and its memory budget. A `DynamicJsonDocument` has a fixed
capacity; overflow is silent, so the stage prints a deliberately undersized
document next to a correct frame, then verifies every frame by round-trip.

Frame shape: `{"fields":[{"snd":..},{"co2":..},{"pm25":..},{"hum":..},{"temp":..}]}`.

Acceptance:

- truncation demo shows `overflowed=1` and a shortened payload;
- the real frame parses with `jq`;
- `frame round-trip OK` on every frame.

## Stage 09 — `v0.9-scheduler`

`TaskScheduler` replaces the hand-rolled timers. Each sensor samples at its own
cadence (120 ms sound, 400 ms PM, 2 s CO2, 15 s temperature) into a window; the
15 s frame averages the windows and clears them.

Deliberate defect carried over from production, for the S6+7 audit: the three
averaged fields are windowed means, while `temp` and `hum` are the last
instantaneous sample cast to an integer. Explain the discrepancy with your own
measurements.

Acceptance:

- frames every 15 s with averaged `snd`, `co2`, `pm25`;
- changing one interval constant changes only that sensor;
- an absent sensor yields 0 in its field, never a stalled frame.

## Stage 10 — `v0.10-wifi`

Association as a state machine. Credentials come from `secrets.local.h` when
present, otherwise a captive portal persisted in NVS. The device ID is the Wi-Fi
MAC with the `aqa-` prefix, so it survives reflashing. RSSI is logged because
"connected" is not "usable".

Production reboots after three failed reconnects; this stage keeps sampling
offline and retries forever. The reboot policy is defended with soak data at S12.

Acceptance:

- portal on first boot, direct join afterwards;
- IP and RSSI every 10 s;
- AP down/up: reconnect without a manual reset and without losing samples.

## Stage 11 — `v0.11-mqtt`

Publish/subscribe with the AQA topic tree. Identity is the MAC-derived sensor id;
location is the geohash in the topic; payload is the JSON frame. A broker outage
drops frames but never stops the sampler.

Topics:

```
tele/{geohash}/{sensor_id}   measurements, every 15 s
sta/{sensor_id}              status (added at v0.12)
cmd/{command}/{sensor_id}    commands (added at v0.12)
```

Acceptance:

- `mosquitto_sub -v -t 'tele/#'` prints frames with the expected topic;
- stopping the broker logs drops without stalling sampling;
- restarting the broker resumes publishing automatically;
- two boards never collide on a topic.

## Stage 12 — `v0.12-commands`

Bi-directional MQTT. Four command topics now (six after the OTA stage), a
dispatcher, status replies, and NVS for things that must survive a reboot
(geohash, parameters, boot count).

```
cmd/status/{id}    -> status snapshot
cmd/config/{id}    -> runtime parameters as JSON
cmd/geohash/{id}   -> move the node's tele topic
cmd/restart/{id}   -> reboot
sta/{id}           -> status replies + 60 s heartbeat
```

Subscriptions use QoS 1. They are re-issued on every reconnect because they do
not survive a session.

Acceptance:

- a status request produces a status payload on `sta/{id}`;
- changing geohash retargets frames and survives a reboot;
- `cmd/config` parameters persist;
- `cmd/restart` replies before rebooting and increments `boot_count`.

## Stage 13 — `v0.13-reliability`

Four production gaps closed, the ones the syllabus makes mandatory:

- **NTP:** `configTime()` runs after association; every telemetry frame carries
  `t` (epoch seconds, 0 until synced). Status timestamps are never uptime.
- **LWT:** a retained `"online":false` will on `sta/{id}`, so a power cut leaves
  a visible offline marker at the broker.
- **Watchdog:** 30 s hardware watchdog; a hung loop reboots instead of freezing.
- **Frozen values:** three identical frames in a row add `"stale":true`.

Also fixed here: temperature and humidity keep one decimal instead of being
truncated to integers.

Acceptance:

- `t` is wall-clock epoch; LWT flips to offline within the keep-alive window;
- watchdog resets a deliberately hung board;
- stale flag appears for a frozen sensor; decimals survive.













