# v1.0 parity with `SenPork_firmware` @ `improve-mqtt`

The final tag is the production device, cleaned up for teaching. Every module
below exists in both trees with the same responsibility and the same file name.

## File map

| Ladder | Production | Note |
|--------|------------|------|
| `src/main.cpp` | `src/main.cpp` | same boot order; watchdog + OTA init added |
| `src/hal/board/ttgo_t7_v14.h` | same | unchanged pins |
| `src/hal/hal.{h,cpp}` | same | CO2 verification no longer reboots |
| `src/hal/sensors/*` | same | same four drivers |
| `src/data/data_manager.{h,cpp}` | same | `t`, stale flag, float temp/hum, MQTT publish |
| `src/network/network_manager.{h,cpp}` | same | portal behaviour fixed; LWT; no reboot policy |
| `src/mqtt/mqtt_protocol.{h,cpp}` | same | same six command topics |
| `src/ota/ota_manager.{h,cpp}` | same | unchanged verification path |
| `src/utils/config_manager.*` | same | same topic construction |
| `src/utils/runtime_config.*` | same | NVS geohash/params/boot count |
| `src/utils/logger.*` | same | unchanged |
| `src/utils/task_manager.h` | same | unchanged |
| `src/utils/secrets.h` | gitignored in production | committed training defaults + local override |
| `src/public_key.h` | gitignored in production | demo RSA public key committed |
| `tools/gen_keys.sh` | absent | keypair generation |
| `tools/sign_firmware.sh` | `sign_firmware.sh` | fixed path handling |

## Deltas (intentional)

1. **CO2 verification** logs instead of `ESP.restart()`: a teaching bench without
   a CO2 sensor must not boot-loop.
2. **WiFiManager usage**: production passes station credentials to
   `autoConnect()`, which configures the setup AP, not the station. The ladder
   tries `secrets` first, then opens a portal named `SenPork-setup`.
3. **Password leak removed**: production logs `Config().getWifiPassword()` over
   serial at every connect.
4. **Reconnect policy**: production reboots after three failures; the ladder
   retries and stays alive. Reboot-vs-retry is the S12 defence exercise.
5. **NTP**: `configTime()` is called after association; telemetry carries `t`
   (epoch seconds, 0 until synced). Production has neither.
6. **LWT**: retained `{"online":false}` on `sta/{id}`; production sets no will.
7. **Watchdog**: 30 s `esp_task_wdt`; production has none.
8. **Frozen values**: `"stale":true` after three identical frames.
9. **Precision**: `temp`/`hum` keep one decimal instead of integer truncation.
10. **Boot count**: incremented once per boot; production calls
    `RuntimeConfig::begin()` twice.
11. **Signing**: RSA-2048 + SHA-256, which is what the production `mbedtls`
    build can verify. ESP-IDF 4.4 ships mbedtls 2.28.7, whose legacy
    `mbedtls_pk_verify` path has no EdDSA support, so the syllabus' Ed25519 line
    cannot be exercised on this toolchain without moving to mbedtls 3.x.

## Not implemented (module scope ends here)

- Store-and-forward queue and replay after outage.
- TLS 8883 with per-device credentials (Module 3).
- Persistence, EMQX, dashboards (Module 3).
- `analyze_size.py` pre-build size report.
