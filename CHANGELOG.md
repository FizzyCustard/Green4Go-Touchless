# Changelog

## 1.0.0

First public release.

### Features
- Touchless navigation with dwell-to-select, a dwell progress bar and a hand-position cursor
- Bus Times screen with Refresh and Back; next two buses with a local countdown
- Leave-now alert: WLED green pulse and LEAVE NOW on standby while a bus is 8 to 6 minutes away, during configurable commute hours
- Display sleep with backlight fade; wakes on approach
- OTA updates with on-screen progress (opt-in, password required)
- Health monitor: logs heap and stack, restarts cleanly when idle if the device gets stuck

### Reliability
- Selections can't chain across screens: after any selection the hand must move before the next one
- Zone hysteresis, median filter and missed-echo bridging on the sensor
- Non-blocking WiFi reconnect; splash screen can no longer hang
- TfL fetches back off while failing (15s up to 5 min); stalled or truncated responses are detected; HTTP/1.0 streaming parse with hard time and size limits
- WLED: fail-fast timeouts with backoff, save/restore of the user's lighting around every effect with retry, presets only touch segment 0
- An object left in front of the sensor no longer keeps the screen awake

### Display
- Flicker-free partial redraws throughout; eased timer menu animation; unlock progress ring; readable text on every highlight colour

### Build
- Platform pinned to `platformio/espressif32 @ 6.12.0` (Arduino-ESP32 2.0.17)
- 16MB flash with two 6.25MB OTA app slots. Flash once over USB when upgrading from an earlier build.

## 0.1.0

Initial prototype: touchless menu, TfL bus times, WLED presets, timer.
