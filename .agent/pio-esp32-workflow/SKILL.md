---
name: pio-esp32-workflow
description: How to build, flash (USB and OTA), configure and debug this ESP32-S3 firmware with PlatformIO. Use this skill when modifying platformio.ini, setting up secrets.h, uploading over the air, reading serial output, or choosing pins for new hardware.
---

# ESP32-S3 PlatformIO Workflow

## Toolchain (pinned, don't float it)
`platformio.ini` pins `platform = platformio/espressif32 @ 6.12.0`, which bundles **Arduino-ESP32 2.0.17**. The code is written against that core: the backlight uses the 2.0.x LEDC API (`ledcSetup`/`ledcAttachPin`), which does not compile on core 3.x. Library: `moononournation/GFX Library for Arduino @ 1.4.9` (also pinned).

Other settings that matter:
- `board = esp32-s3-devkitc-1` with `board_upload.flash_size = 16MB` and `board_build.partitions = default_16MB.csv` (two ~6.25MB app slots, needed for OTA).
- `monitor_speed = 115200`, `monitor_filters = esp32_exception_decoder` (decodes crash backtraces).
- Native USB serial needs both flags, or `Serial` prints nothing over USB-C:
  ```ini
  build_flags =
      -D ARDUINO_USB_MODE=1
      -D ARDUINO_USB_CDC_ON_BOOT=1
  ```
- PSRAM is not enabled in the build.

## First-time setup: secrets.h
`Config.h` `#include`s `src/secrets.h` and stops the build with `#error` if it's missing. It is gitignored.
1. `cp src/secrets.example.h src/secrets.h`
2. Fill in `WIFI_SSID`, `WIFI_PASS`, `TFL_STOP_ID`, `TFL_FILTER_LINES` (`""` = all lines), `WLED_IP`.
3. Optional: `TFL_APP_KEY` (higher TfL rate limit), `OTA_PASSWORD` (enables OTA), `OTA_HOSTNAME` (defaults to `green4go`).

Never commit `secrets.h` or put real credentials, stop IDs or IPs in tracked files.

## Build and flash
Two environments exist, so always pass `-e` (a bare `pio run -t upload` acts on both):
```bash
pio run -e esp32s3                 # build
pio run -e esp32s3 -t upload       # flash over USB
pio device monitor -e esp32s3      # serial at 115200
```

## OTA uploads
OTA is **opt-in**: it is compiled in only when `OTA_PASSWORD` is defined in `secrets.h`. `main.cpp` has `static_assert`s that refuse to build if the password is the template value (`"use-a-long-random-password"` or `"change-me"`) or shorter than 12 characters. When enabled, `ArduinoOTA` starts once WiFi connects and the screen shows an "Updating" progress bar during an upload.
1. Set `OTA_PASSWORD` and flash once over USB.
2. Then:
   ```bash
   export GREEN4GO_OTA_PASSWORD=your-password
   pio run -e esp32s3_ota -t upload
   ```
`esp32s3_ota` extends `esp32s3` with `upload_protocol = espota`, `upload_port = green4go.local`, and passes the password from the env var. If you change `OTA_HOSTNAME`, change `upload_port` to match.

## Serial output worth knowing
- Tagged logs: `[Bus]`, `[WLED]`, `[Net]`, `[Alert]`, `[Health]`.
- `[Health]` prints heap, largest free block and stack headroom once a minute. The firmware restarts itself (only while idle in standby) if the network task is stuck > 2 min, the largest heap block stays < 24KB for 5 min, or there has been no good TfL data for 2 h while on WiFi.
- Set `DEBUG_SENSOR 1` in `Config.h` to print raw/smoothed distance every tick.
- Reset reason is printed at boot (`esp_reset_reason()`).

## Configuration rules
- Pins, timings, thresholds and limits live in `src/Config.h`; personal settings in `src/secrets.h`; display colours and layout constants in `src/DisplayManager.h`.
- Current pins: TFT SCK 39, MOSI 38, DC 42, CS 45, RST 0, BL 1; HC-SR04 TRIG 17, ECHO 18. A 5V HC-SR04's ECHO needs a divider into GPIO18 (or use a 3.3V HC-SR04P / RCWL-1601).
- The UI loop runs on core 1 (Arduino `loop()`); all HTTP runs on a FreeRTOS network task on core 0. Never add blocking network calls to `loop()`.

For free GPIOs when adding hardware, see [pinout_map.md](references/pinout_map.md). Board details: `esp32-s3-board-specs` skill.
