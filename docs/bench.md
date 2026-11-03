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





