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


