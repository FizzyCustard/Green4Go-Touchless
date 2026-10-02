---
name: esp32-s3-board-specs
description: Specifications and pinout details for the specific ESP32-S3-LCD-2 board used in this project. Use this skill when modifying hardware pin assignments, configuring display settings, or working with the onboard peripherals like the screen or IMU.
---

# ESP32-S3-LCD-2 Board Specifications

This skill provides reference information for the project's development board, including its hardware features, display driver requirements, and physical pin mappings.

## Core Hardware Features
- **Processor:** ESP32-S3R8 Xtensa 32-bit LX7 dual-core, up to 240MHz
- **Memory:** 512KB SRAM, 384KB ROM, 8MB onboard PSRAM, 16MB external Flash
- **Connectivity:** 2.4GHz Wi-Fi (802.11 b/g/n) and Bluetooth 5 (LE)
- **Display:** 2" LCD, 240 x 320 resolution, 262K color, driven by **ST7789T3** via SPI
- **Sensors:** QMI8658 6-axis IMU (Accelerometer + Gyroscope)
- **Power/Data:** USB Type-C, 3.7V MX1.25 Li-po battery header, 5V power pin
- **Peripherals:** SD card slot, Camera interface (OV2640/OV5640 compatible)

## Display Initialization (Arduino GFX)
When initializing the display using the `Arduino_GFX` library, use the `Arduino_ST7789` driver class and the `Arduino_ESP32SPI` bus class:

```cpp
Arduino_DataBus *bus = new Arduino_ESP32SPI(
  DC_PIN, CS_PIN,
  SCK_PIN, MOSI_PIN, MISO_PIN);

Arduino_GFX *gfx = new Arduino_ST7789(
  bus, RST_PIN, ROTATION, true /* IPS */,
  WIDTH, HEIGHT);
```

## Pinout Reference

| Physical Pin | Header Label | GPIO | Internal Shared Function / Notes |
|---|---|---|---|
| 1 | 3V3 | - | 3.3V Power Output |
| 2 | GND | - | Ground |
| 3 | TXD | IO43 | UART Transmit |
| 4 | RXD | IO44 | UART Receive |
| 5 | GPIO47 | IO47 | Touch Panel SCL |
| 6 | GPIO48 | IO48 | Touch Panel SDA |
| 7 | GPIO15 | IO15 | Camera D2 |
| 8 | GPIO13 | IO13 | Camera D1 |
| 9 | GPIO11 | IO11 | Camera D3 |
| 10 | GPIO12 | IO12 | Camera D0 |
| 11 | GPIO14 | IO14 | Camera D4 |
| 12 | GPIO9 | IO9 | Camera PCLK |
| 13 | GND | - | Ground |
| 14 | VBAT | - | Battery Input (3.7V Li-po) |
| 15 | 5V | - | 5V Power Input/Output |
| 16 | GND | - | Ground |
| 17 | GPIO19 | IO19 | Free I/O |
| 18 | GPIO20 | IO20 | Free I/O |
| 19 | GPIO10 | IO10 | Camera D5 |
| 20 | GPIO7 | IO7 | Camera D6 |
| 21 | GPIO8 | IO8 | Camera XCLK |
| 22 | GPIO21 | IO21 | Camera SCCB SDA |
| 23 | GPIO18 | IO18 | Free I/O |
| 24 | GPIO17 | IO17 | Camera PWDN |
| 25 | GPIO16 | IO16 | Camera SCCB SCL |
| 26 | GPIO6 | IO6 | Camera VSYNC |
| 27 | GPIO4 | IO4 | Camera HREF |
| 28 | GPIO2 | IO2 | Camera D7 |

## References
For additional official documentation, visit the [Waveshare Wiki for ESP32-S3-LCD-2](https://www.waveshare.com/wiki/ESP32-S3-LCD-2).
