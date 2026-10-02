# ESP32-S3-LCD-2 Available GPIOs

If new hardware is added (such as buttons, buzzers, or I2C sensors), these are the remaining free pins based on the manufacturer spec and current allocations.

## In Use
- `IO39`: TFT SCK
- `IO38`: TFT MOSI
- `IO42`: TFT DC
- `IO45`: TFT CS
- `IO0`: TFT RST
- `IO1`: TFT BL (Backlight)
- `IO17`: Ultrasonic Trigger
- `IO18`: Ultrasonic Echo

## Free General Purpose IO (from "Free I/O" headers)
- `IO19`
- `IO20`

## Repurposable Camera IO (If OV2640 is not connected)
- `IO15`: Camera D2
- `IO13`: Camera D1
- `IO11`: Camera D3
- `IO12`: Camera D0
- `IO14`: Camera D4
- `IO9`: Camera PCLK
- `IO10`: Camera D5
- `IO7`: Camera D6
- `IO8`: Camera XCLK
- `IO21`: Camera SCCB SDA
- `IO16`: Camera SCCB SCL
- `IO6`: Camera VSYNC
- `IO4`: Camera HREF
- `IO2`: Camera D7

## Built-In Features
- Sub-board IMU `QMI8658` (6-axis). I2C pins internally routed.
- Built-in SD Card slot (SPI pins internally routed).
