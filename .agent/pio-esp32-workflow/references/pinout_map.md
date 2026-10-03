# ESP32-S3-LCD-2 GPIO Allocation

Based on the manufacturer's header table and the current `Config.h`.

## In use
- `IO39`: TFT SCK (on-board LCD)
- `IO38`: TFT MOSI (on-board LCD)
- `IO42`: TFT DC (on-board LCD)
- `IO45`: TFT CS (on-board LCD)
- `IO0`: TFT RST (on-board LCD)
- `IO1`: TFT backlight, LEDC PWM channel 0
- `IO17`: Ultrasonic TRIG (header pin 24; maker labels it Camera PWDN)
- `IO18`: Ultrasonic ECHO (header pin 23). **3.3V only**: a 5V HC-SR04 needs a voltage divider on ECHO, or use a 3.3V HC-SR04P / RCWL-1601.

## Free general-purpose I/O
- `IO19`, `IO20` (header pins 17, 18)

## Repurposable camera I/O (no camera fitted)
- `IO15` (D2), `IO13` (D1), `IO11` (D3), `IO12` (D0), `IO14` (D4), `IO9` (PCLK)
- `IO10` (D5), `IO7` (D6), `IO8` (XCLK), `IO21` (SCCB SDA), `IO16` (SCCB SCL)
- `IO6` (VSYNC), `IO4` (HREF), `IO2` (D7)

## Other header pins
- `IO43` / `IO44`: UART TX / RX
- `IO47` / `IO48`: labelled Touch panel SCL / SDA

## Built-in, not used by this firmware
- QMI8658 IMU and SD card slot (routed internally; check the Waveshare wiki before reusing their pins).

When adding hardware: add the pin to `Config.h`, keep it under `MAX_ESP32_PIN` (48), and update this list.
