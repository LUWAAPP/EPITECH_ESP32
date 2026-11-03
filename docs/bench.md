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
