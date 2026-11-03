# Bench checklist

No board is attached to the machine that authored this repository, so every tag
is verified by compilation (`tools/build-all-tags.sh`). The runtime checks below
are the ones a team must run with the board in hand.

## v0.0-toolchain

- [ ] `pio run` SUCCESS, compile timestamp matches the edit you just made.
- [ ] Monitor shows the banner and `alive after 5 s` lines.
- [ ] Flash log shows esptool write + hash, no `Connecting...` retries.

## v0.1-timing

- [ ] LED blinks at 1 Hz (500 ms on, 500 ms off).
- [ ] Log line appears every 1 s regardless of LED phase — the loop is not
      blocked.
- [ ] Uptime increases monotonically; reset restarts it at 0.

## v0.2-adc

- [ ] Silence: `p2p` near 0, `mean` stable around the DC bias.
- [ ] Clap or speak: `p2p` spikes, then decays — the window catches transients.
- [ ] Cover the microphone: `p2p` collapses.
- [ ] `samples` per window is roughly 5000-15000 (ADC rate, not a fixed count).

## v0.3-i2c

- [ ] Scan lists `0x44` (SHT4x). Empty scan = wiring or power, not a code bug.
- [ ] Serial number prints and is stable across resets.
- [ ] Temperature/humidity change when you breathe on the sensor.
- [ ] Disconnect SDA: scan is empty and the read fails with an error, no crash.

## v0.4-temp-autodetect

- [ ] With the SHT4x populated: log says `temperature sensor: SHT40`.
- [ ] With only a DHT on GPIO 13: log says `DHT22` or `DHT11` after the settle
      time; readings match the room.
- [ ] With neither: `no temperature sensor detected`, and the loop keeps running.
- [ ] Note the boot time cost of failed DHT probes — it is seconds, not
      milliseconds.

## v0.5-co2-uart

- [ ] Boot log names the CO2 family (`MHZ19` or `S8`) after probing.
- [ ] CO2 value is plausible outdoors (~400-600 ppm); breathe on it, it climbs.
- [ ] Swap RX/TX: detection fails with an explicit message, no hang.
- [ ] Power-cycle and observe the warm-up behaviour — first readings may be 0.

## v0.6-pm-uart

- [ ] `PM2.5` prints every 2 s and rises when you disturb dust near the inlet.
- [ ] Disconnect the PMS TX line: reads time out with a warning, the other
      sensors keep reporting.
- [ ] Fan audibly spins; if not, check the PMS power path before the code.

## v0.7-hal

- [ ] Self-test line reports PASS/FAIL per sensor, matching what is populated.
- [ ] Remove one sensor: only its slot is FAIL, the other three keep sampling.
- [ ] Boot completes without a reset loop even with the CO2 sensor absent.
- [ ] `main.cpp` contains no pin numbers — they live in `BoardPins`.

## v0.8-arduinojson

- [ ] Boot prints the truncation demo with `overflowed=1` and a shorter payload.
- [ ] Frame every 5 s is valid JSON: `{"fields":[{"snd":..},...]}`.
- [ ] `frame round-trip OK` every time; if MISMATCH, a field did not survive.
- [ ] Paste a frame into `jq .` on the host — it parses.

## v0.9-scheduler

- [ ] A frame prints every 15 s with averaged `snd`, `co2`, `pm25`.
- [ ] `hum`/`temp` equal the last 15 s sample, not an average — the deliberate
      metrology defect to explain in the S6+7 audit.
- [ ] Changing one `*_SAMPLE_TIME` constant changes only that sensor's cadence.
- [ ] With a sensor absent, its averaged field stays 0 and the frame still ships.

## v0.10-wifi

- [ ] First boot with `CHANGE_ME` opens `SenPork-setup`; join it, enter lab
      credentials, board connects and stores them in NVS.
- [ ] Second boot joins directly, no portal.
- [ ] Status line every 10 s shows IP and RSSI; moving away lowers RSSI.
- [ ] Power off the AP: node logs reconnect attempts and keeps sampling frames.
- [ ] Power the AP back: node reconnects without a manual reset.

## v0.11-mqtt

- [ ] `mosquitto_sub -v -t 'tele/#'` shows a frame every 15 s with the full topic
      `tele/d347/aqa-<MAC>`.
- [ ] `mosquitto_sub -t 'sta/#'` is silent at this stage — status is v0.12.
- [ ] Stop the broker: log says frame dropped; sampling continues.
- [ ] Restart the broker: publishing resumes within one reconnect interval.
- [ ] Frames from two boards never share a topic.

## v0.12-commands

- [ ] `mosquitto_pub -t cmd/status/<id> -m ''` → status JSON on `sta/<id>`.
- [ ] `mosquitto_pub -t cmd/geohash/<id> -m '{"geohash":"d348"}'` → next frames
      arrive on `tele/d348/<id>` and the change survives a reboot.
- [ ] `cmd/config` with `{"interval":30}` survives a reboot in `parameters`.
- [ ] `cmd/restart` replies `in_progress`, then the board reboots and republishes
      status with an incremented `boot_count`.
- [ ] Subscribe with QoS 1: kill and restart the broker, commands still arrive.

## v0.13-reliability

- [ ] `t` is real epoch seconds and matches the wall clock within a second or
      two; before NTP answers it is 0, never uptime.
- [ ] Pull power for 60 s: `sta/<id>` (retained) changes to `"online":false`.
- [ ] Power back: `"online":true` and `boot_count` increments.
- [ ] Hold the loop in a long blocking call: watchdog resets the board after
      30 s instead of hanging silently.
- [ ] Leave a sensor unplugged: after three identical frames the payload carries
      `"stale":true`.
- [ ] `hum`/`temp` carry one decimal, not integers.

## v1.0-ota

- [ ] `tools/gen_keys.sh` produces `private_key.pem` (gitignored) and a fresh
      `src/public_key.h`; rebuild succeeds.
- [ ] Serve firmware + signature over HTTP on the lab laptop, then
      `mosquitto_pub -t cmd/ota/<id> -m '{"url":"http://<host>:8000/firmware.bin",...}'`.
- [ ] Status reports `ota` progress and then `completed`/`failed`.
- [ ] Flip one byte in the signature (or sign a different binary): verification
      fails, the old application keeps running, status says `failed`.
- [ ] `cmd/rollback` reboots into the previous OTA slot.
- [ ] Tampered firmware is never committed to the boot partition.













