# Green4Go

A touchless kitchen-counter display for an ESP32-S3. Wave your hand above an ultrasonic sensor to drive the menus: no buttons, no touchscreen. It shows live London bus arrivals from TfL, controls a WLED light strip, runs a coffee timer, and tells you through the lights when it's time to leave for your bus.

## Features

- **Touchless navigation.** Hand distance (4-22cm) picks one of three menu rows. Hold still for a second to select, with a progress bar showing it's about to happen. A cursor down the right edge shows where your hand maps.
- **Live bus times.** The next two buses at your stop, counting down locally between fetches, colour-coded by how catchable they are.
- **Leave-now alert.** During your commute hours, the screen shows **LEAVE NOW** while a bus is 8 to 6 minutes away, and the WLED strip gives a short green pulse.
- **Bus status on the coffee light.** Whenever the lights are coffee white, the first three LEDs show the next bus you can catch: red while it's far off, fading through orange to green when it's time to go, then red once it's gone. The rest stays white, and it replaces the green pulse.
- **WLED control.** Light presets, and the strip shows bus status while you check times. Your own lighting is saved and restored around every effect.
- **Coffee timer** that keeps the screen awake while it runs.
- **Standby and sleep.** A passive standby screen with the next bus; the backlight fades out after 15 minutes idle and wakes when a hand comes near.
- **OTA updates** over WiFi, with progress on screen.
- **Self-healing.** Backs off politely when TfL or the internet is down, retries light restores, and restarts itself cleanly (only while idle) if it ever gets stuck.

## Hardware

| Part | Notes |
|---|---|
| [Waveshare ESP32-S3-LCD-2](https://www.waveshare.com/wiki/ESP32-S3-LCD-2) | ESP32-S3 with a built-in 2" 240x320 ST7789 display, 16MB flash |
| HC-SR04 ultrasonic sensor | Or a 3.3V-native variant (HC-SR04P, RCWL-1601) |
| WLED controller (optional) | Any [WLED](https://kno.wled.ge/) device on the same network |
| 2 resistors (1kΩ + 2kΩ) | Only for a 5V HC-SR04, see below |

### Wiring

The display is wired on the board. The sensor connects to the header:

| HC-SR04 | ESP32-S3-LCD-2 |
|---|---|
| VCC | 5V (header pin 15), or 3V3 for a 3.3V sensor |
| GND | GND |
| TRIG | GPIO17 |
| ECHO | GPIO18 **through a voltage divider** |

> **Important:** a 5V HC-SR04 outputs 5V on ECHO, and the ESP32-S3's GPIOs are 3.3V only. Put a divider between them: ECHO → 1kΩ → GPIO18, and GPIO18 → 2kΩ → GND. Skipping it may work for a while, then damage the pin. A 3.3V sensor (HC-SR04P, RCWL-1601) powered from 3V3 doesn't need one.

Mount the sensor facing the space where your hand will be. Screen position maps to distance: the top row is closest to the sensor, the grey base strip at the bottom is furthest (22-32cm). More detail in [docs/hardware.md](docs/hardware.md).

## Setup

This is a [PlatformIO](https://platformio.org/) project (it won't build in the Arduino IDE as-is).

1. **Install PlatformIO**, either the VS Code extension or `pip install platformio`.
2. **Create your settings file:**
   ```bash
   cp src/secrets.example.h src/secrets.h
   ```
   Then edit `src/secrets.h`:
   - `WIFI_SSID` / `WIFI_PASS`: your 2.4GHz WiFi.
   - `TFL_STOP_ID`: your stop's NaPTAN ID. Find your stop on the [TfL website](https://tfl.gov.uk/travel-information/stations-stops-and-piers/); the ID is the code in the stop page's URL, e.g. `490000077E`.
   - `TFL_FILTER_LINES`: optional, e.g. `"36,185"` to only show those routes.
   - `WLED_IP`: your WLED controller's IP address.
   - `OTA_PASSWORD`: optional, see [OTA updates](#ota-updates).

   `src/secrets.h` is gitignored, so your details stay out of the repo.
3. **Build and flash over USB:**
   ```bash
   pio run -t upload
   pio device monitor   # optional: serial logs
   ```

## Using it

- **Wake:** hold your hand close to the sensor (under 3cm) for a second. A green ring fills to unlock.
- **Navigate:** move your hand between about 4 and 22cm to highlight a row. Hold still and the bar along the row fills; when it's full, that row is selected. To select again, move your hand away or to another row.
- **Timer screen:** move your hand away (over 37cm) then bring it back to the grey base zone (22-32cm) to open the Reset/Back menu.
- **Bus Times:** the top row shows the next bus. Middle row refreshes, bottom row goes back. The lights show the bus status for 10 seconds.
- After 30 seconds without interaction it returns to standby.

## Configuration

Behaviour is tuned in `src/Config.h`. The ones you're most likely to change:

| Setting | Default | What it does |
|---|---|---|
| `LEAVE_WINDOW_MAX_MIN` / `LEAVE_WINDOW_MIN_MIN` | 8 / 6 | Leave-now alert runs while a bus is between these many minutes away |
| `LEAVE_ALERT_DAYS` | Mon-Fri | Bitmask, bit 0 = Sunday |
| `LEAVE_ALERT_START_HHMM` / `_END_HHMM` | 700 / 930 | Alert hours, local time |
| `TIMEZONE` | UK (GMT/BST) | POSIX TZ string for local time |
| `LEAVE_PULSE_MS` | 3000 | How long the green leave-now pulse plays |
| `STATUS_LEDS` / `STATUS_BRIGHTNESS` | 3 / 100 | Status LEDs on the coffee light, and their brightness in % |
| `STATUS_RED_MIN` / `STATUS_ORANGE_MIN` | 15 / 10 | Status fades from red (15 min) to orange (10) to green (the leave window) |
| `BUS_RED_MAX_MIN` / `BUS_GREEN_MAX_MIN` / `BUS_ORANGE_MAX_MIN` | 5 / 9 / 12 | Bus status colour thresholds |
| `DISPLAY_SLEEP_MS` | 15 min | Idle time before the backlight fades out |
| `UNLOCK_DISTANCE_CM` | 3 | How close to hold your hand to wake it |
| `DWELL_THRESHOLD_MS` | 1000 | How long to hold still to select |
| `TITLE_LINE_1` / `TITLE_LINE_2` | COFFEE ZONE / CONTROLLER | Standby screen title |

## OTA updates

Once the device has been flashed over USB, you can update it over WiFi.

1. In `src/secrets.h`, uncomment `OTA_PASSWORD` and set a long random password (12+ characters). The build refuses the template value or anything shorter, because anyone on your network with the password can flash the device.
2. Flash once over USB so the device knows the password.
3. From then on, on the same network:
   ```bash
   export GREEN4GO_OTA_PASSWORD=your-password
   pio run -e esp32s3_http -t upload
   ```

`esp32s3_http` sends the firmware straight to the device over HTTP (user `admin`, digest auth), so it works through a firewall on your computer. Without PlatformIO it's one curl:

```bash
curl --digest -u admin:your-password --data-binary @firmware.bin \
  -H "Content-Type: application/octet-stream" http://green4go.local/update
```

The older `esp32s3_ota` environment (espota) still works, but the device has to connect back to your computer during the upload, so a firewall like ufw has to let it in.

The device advertises itself as `green4go.local`. If your network blocks mDNS, set `upload_port` in `platformio.ini` to its IP address (printed in the serial log at boot).

> **Upgrading from an earlier version?** The partition layout changed to use the board's full 16MB flash. A partition change can't be applied over OTA, so flash this version over USB once.

## Project layout

```
src/
  main.cpp            Setup, main loop, WiFi upkeep, OTA
  Config.h            All tunable settings and pin assignments
  secrets.example.h   Template for your WiFi/TfL/WLED/OTA settings
  StateManager.*      Screens, selection logic, bus data, alerts, health checks
  DisplayManager.*    All drawing: flicker-free partial redraws and animation
  DistanceSensor.*    HC-SR04 reading and filtering
  NetworkManager.*    Background task (core 0) that runs all network calls
  BusClient.*         TfL arrivals fetch and streaming parser
  WledClient.*        WLED JSON API, save/restore of the user's lighting
docs/                 Hardware notes and how the interaction works
```

How it works under the hood is in [docs/how-it-works.md](docs/how-it-works.md).

## Known limitations

- **Unlock distance.** The HC-SR04's minimum range is about 2cm, so unlocking at under 3cm leaves a narrow window. If it feels finicky, raise `UNLOCK_DISTANCE_CM` to 5.
- **TfL TLS isn't certificate-checked** (`setInsecure()`). The data is public and read-only, so the risk is spoofed bus times on a hostile network. Pinning TfL's root CA is a small change if you need it.
- **Anonymous TfL API.** Fine for one device. For more, get a free key from the [TfL API portal](https://api-portal.tfl.gov.uk/) and set `TFL_APP_KEY` in `secrets.h`.
- **Arduino-ESP32 2.0.x only.** The platform is pinned to `espressif32 @ 6.12.0` (core 2.0.17). Core 3.x changed the backlight PWM API.

## Credits

Powered by TfL Open Data. Contains OS data © Crown copyright and database rights 2016 and Geomni UK Map data © and database rights 2019.

Display driver: [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX).

## Licence

[MIT](LICENSE) © FizzyCustard
