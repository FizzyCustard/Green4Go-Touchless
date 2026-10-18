# Green4Go

ESP32-S3 touchless kitchen display (PlatformIO, Arduino core 2.0.17).

## Project skills (`.agent/`)

Detailed guides for each part of the firmware. Read the relevant `SKILL.md` (and its `references/`) before changing that area, and update it when the code it describes changes.

| Skill | Covers |
|---|---|
| `.agent/touchless-ui-logic` | Sensor filtering, distance zones, dwell selection, re-arming, timer menu, unlock (`StateManager`, `DistanceSensor`) |
| `.agent/tft-gfx-renderer` | Drawing to the 240x320 ST7789 without flicker, screen layout, hand cursor (`DisplayManager`) |
| `.agent/wled-json-controller` | WLED JSON API, saving/restoring the user's lighting, coffee status LEDs (`WledClient`) |
| `.agent/tfl-api-parser` | Fetching and stream-parsing TfL bus arrivals, the network task (`BusClient`, `NetworkManager`) |
| `.agent/pio-esp32-workflow` | Building, flashing (USB, espota and HTTP OTA), `secrets.h`, serial debugging |
| `.agent/esp32-s3-board-specs` | Waveshare ESP32-S3-LCD-2 pinout and board details |

## Building and updating the device

- `pio` is at `~/.platformio/penv/bin/pio` if it isn't on PATH.
- `src/secrets.h` (gitignored) holds WiFi, TfL, WLED and `OTA_PASSWORD`.
- Build: `pio run -e esp32s3_http`
- **Update over WiFi (default):**
  ```bash
  GREEN4GO_OTA_PASSWORD=<OTA_PASSWORD from src/secrets.h> pio run -e esp32s3_http -t upload
  ```
  This POSTs the firmware to `http://green4go.local/update` (digest auth, user `admin`). Every connection is outbound from this machine, so no firewall port is needed. Success prints `OK, restarting`.
- Fallback `esp32s3_ota` (espota) needs the device to connect back to this machine; a host firewall (ufw) must allow it in first.
- The device's IP changes (DHCP): resolve with `getent hosts green4go.local`. Don't confuse it with the WLED boards (separate IPs, in `secrets.h`).
- Changing `platformio.ini` makes PlatformIO wipe other envs' build folders; rebuild before pointing at a `.bin`.

## Checking it after an update

- The sensor stream on TCP 2323 sends `millis raw_cm smoothed_cm` every tick; a small `millis` confirms a fresh boot:
  ```bash
  bash -c 'exec 3<>/dev/tcp/green4go.local/2323; cat <&3'
  ```
- There is no network log; serial (USB, 115200) is the only place crash backtraces appear.

## Conventions

- Tunables live in `src/Config.h`. When changing one, update README, `docs/` and the matching `.agent` skill.
- Add user-facing changes to `CHANGELOG.md` under Unreleased.
