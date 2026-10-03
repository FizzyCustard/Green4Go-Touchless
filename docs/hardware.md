# Hardware

## Board: Waveshare ESP32-S3-LCD-2

- ESP32-S3R8: dual-core Xtensa LX7 up to 240MHz, 2.4GHz WiFi, Bluetooth LE
- 512KB SRAM, 8MB PSRAM (not used by this firmware), 16MB flash
- 2" 240x320 IPS LCD, ST7789 driver over SPI
- QMI8658 6-axis IMU, SD card slot, camera connector, LiPo charging header
- USB-C for power, flashing and serial

Official docs: [Waveshare wiki](https://www.waveshare.com/wiki/ESP32-S3-LCD-2)

## Pins used

| Function | GPIO | Notes |
|---|---|---|
| LCD SCK | 39 | On-board |
| LCD MOSI | 38 | On-board |
| LCD DC | 42 | On-board |
| LCD CS | 45 | On-board |
| LCD RST | 0 | On-board |
| LCD backlight | 1 | PWM (LEDC channel 0) for sleep fades |
| Sensor TRIG | 17 | Header |
| Sensor ECHO | 18 | Header, 3.3V max: see below |

Pin assignments live in `src/Config.h`.

## Ultrasonic sensor

The HC-SR04 is a 5V part and its ECHO output swings to 5V. The ESP32-S3 GPIOs are 3.3V only, so use a voltage divider:

```
HC-SR04 ECHO ──[1kΩ]──┬── GPIO18
                      │
                    [2kΩ]
                      │
                     GND
```

Or use a 3.3V-native sensor (HC-SR04P, RCWL-1601) powered from 3V3, which needs no divider.

Practical notes:
- The minimum reliable range is about 2cm. Closer than that the echo is often missed, which reads as "nothing there".
- The firmware pings at 20Hz with a ~2m echo timeout, takes the median of the last three readings, bridges up to three missed echoes, then smooths with an exponential moving average.
- Soft or angled surfaces reflect poorly. A flat palm facing the sensor works best.

## Header pinout

| Pin | Label | GPIO | Shared with |
|---|---|---|---|
| 1 | 3V3 | - | 3.3V output |
| 2 | GND | - | |
| 3 | TXD | 43 | UART TX |
| 4 | RXD | 44 | UART RX |
| 5 | GPIO47 | 47 | Touch panel SCL |
| 6 | GPIO48 | 48 | Touch panel SDA |
| 7 | GPIO15 | 15 | Camera D2 |
| 8 | GPIO13 | 13 | Camera D1 |
| 9 | GPIO11 | 11 | Camera D3 |
| 10 | GPIO12 | 12 | Camera D0 |
| 11 | GPIO14 | 14 | Camera D4 |
| 12 | GPIO9 | 9 | Camera PCLK |
| 13 | GND | - | |
| 14 | VBAT | - | LiPo input (3.7V) |
| 15 | 5V | - | 5V in/out |
| 16 | GND | - | |
| 17 | GPIO19 | 19 | Free |
| 18 | GPIO20 | 20 | Free |
| 19 | GPIO10 | 10 | Camera D5 |
| 20 | GPIO7 | 7 | Camera D6 |
| 21 | GPIO8 | 8 | Camera XCLK |
| 22 | GPIO21 | 21 | Camera SCCB SDA |
| 23 | GPIO18 | 18 | Free (sensor ECHO) |
| 24 | GPIO17 | 17 | Camera PWDN (sensor TRIG) |
| 25 | GPIO16 | 16 | Camera SCCB SCL |
| 26 | GPIO6 | 6 | Camera VSYNC |
| 27 | GPIO4 | 4 | Camera HREF |
| 28 | GPIO2 | 2 | Camera D7 |

The camera pins are free to reuse when no camera is fitted.
