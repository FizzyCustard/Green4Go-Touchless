---
name: esp32-s3-board-specs
description: Specifications and pinout details for the Waveshare ESP32-S3-LCD-2 board used in this project, plus how the firmware wires the display and ultrasonic sensor to it. Use this skill when changing pin assignments, display initialisation, flash/PSRAM settings, or adding hardware to the board's header.
---

# ESP32-S3-LCD-2 Board Specifications

Reference for the project's board (Waveshare ESP32-S3-LCD-2) and how this firmware uses it. Pin numbers used by the code live in `src/Config.h`; build settings live in `platformio.ini` (see the `pio-esp32-workflow` skill).

## Board hardware (per the manufacturer)
- **SoC:** ESP32-S3R8, dual-core Xtensa LX7 up to 240MHz
- **Memory:** 512KB SRAM, 384KB ROM, 8MB PSRAM, 16MB flash
- **Radio:** 2.4GHz Wi-Fi (802.11 b/g/n), Bluetooth 5 LE
- **Display:** 2" IPS LCD, 240 x 320, ST7789T3 controller over SPI
- **Also on board (unused by this firmware):** QMI8658 6-axis IMU, SD card slot, camera connector (OV2640/OV5640), MX1.25 Li-po header
- **USB:** Type-C, native USB (no separate UART bridge)

## How this firmware uses it
- **Flash:** `platformio.ini` overrides the generic `esp32-s3-devkitc-1` board to 16MB with `default_16MB.csv` (two OTA app slots).
- **PSRAM:** physically present but **not enabled** in the build (no PSRAM flags). Don't assume `ps_malloc`/PSRAM-backed canvases work without first enabling it and testing on hardware.
- **Display init** (`DisplayManager` constructor). No MISO; rotation 0; IPS = true; library default SPI clock:
  ```cpp
  _bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED);
  _gfx = new Arduino_ST7789(_bus, TFT_RST, 0 /* rotation */, true /* IPS */, 240, 320);
  ```
- **Backlight:** `TFT_BL` (GPIO1) driven by LEDC PWM (channel 0, 5kHz, 8-bit) so it can fade. Uses the Arduino-ESP32 **2.0.x** API (`ledcSetup` / `ledcAttachPin` / `ledcWrite`); core 3.x renamed these.

## Pins used by the firmware (`Config.h`)
| Define | GPIO | Use |
|---|---|---|
| `TFT_SCK` | 39 | LCD SPI clock (on-board) |
| `TFT_MOSI` | 38 | LCD SPI data (on-board) |
| `TFT_DC` | 42 | LCD data/command (on-board) |
| `TFT_CS` | 45 | LCD chip select (on-board) |
| `TFT_RST` | 0 | LCD reset (on-board) |
| `TFT_BL` | 1 | LCD backlight, PWM (on-board) |
| `US_TRIG` | 17 | HC-SR04 TRIG (header pin 24, listed by the maker as Camera PWDN) |
| `US_ECHO` | 18 | HC-SR04 ECHO (header pin 23, free I/O) |

**ECHO voltage:** a standard HC-SR04 powered from 5V drives a **5V** ECHO pulse, and ESP32-S3 GPIOs are 3.3V-only. Put a resistor divider between ECHO and GPIO18, or use a 3.3V-capable sensor (HC-SR04P, RCWL-1601) powered from 3V3. Nothing in the code can detect or compensate for this.

## Header pinout (28-pin, per the manufacturer)

| Pin | Label | GPIO | Maker's note / use here |
|---|---|---|---|
| 1 | 3V3 | - | 3.3V out |
| 2 | GND | - | Ground |
| 3 | TXD | IO43 | UART TX |
| 4 | RXD | IO44 | UART RX |
| 5 | GPIO47 | IO47 | Touch panel SCL |
| 6 | GPIO48 | IO48 | Touch panel SDA |
| 7 | GPIO15 | IO15 | Camera D2 |
| 8 | GPIO13 | IO13 | Camera D1 |
| 9 | GPIO11 | IO11 | Camera D3 |
| 10 | GPIO12 | IO12 | Camera D0 |
| 11 | GPIO14 | IO14 | Camera D4 |
| 12 | GPIO9 | IO9 | Camera PCLK |
| 13 | GND | - | Ground |
| 14 | VBAT | - | Li-po input (3.7V) |
| 15 | 5V | - | 5V in/out |
| 16 | GND | - | Ground |
| 17 | GPIO19 | IO19 | Free I/O |
| 18 | GPIO20 | IO20 | Free I/O |
| 19 | GPIO10 | IO10 | Camera D5 |
| 20 | GPIO7 | IO7 | Camera D6 |
| 21 | GPIO8 | IO8 | Camera XCLK |
| 22 | GPIO21 | IO21 | Camera SCCB SDA |
| 23 | GPIO18 | IO18 | Free I/O - **US_ECHO** |
| 24 | GPIO17 | IO17 | Camera PWDN - **US_TRIG** |
| 25 | GPIO16 | IO16 | Camera SCCB SCL |
| 26 | GPIO6 | IO6 | Camera VSYNC |
| 27 | GPIO4 | IO4 | Camera HREF |
| 28 | GPIO2 | IO2 | Camera D7 |

The LCD pins (0, 1, 38, 39, 42, 45) are wired on the board and are not on this header. For free pins when adding hardware, see `pio-esp32-workflow/references/pinout_map.md`.

## Rules
- Change pins only in `Config.h`; nothing else should hard-code a GPIO.
- `DistanceSensor` rejects pins outside 0..`MAX_ESP32_PIN` (48) and then does nothing, so a typo shows as "ERROR: Invalid pin numbers" on serial and a dead sensor.

Official docs: https://www.waveshare.com/wiki/ESP32-S3-LCD-2
