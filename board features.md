# Features

- Equipped with ESP32-S3R8 Xtensa 32-bit LX7 dual-core processor, up to 240MHz main frequency
- Supports 2.4GHz Wi-Fi (802.11 b/g/n) and Bluetooth 5 (LE), with onboard antenna
- Built-in 512KB of SRAM and 384KB ROM, with onboard 8MB PSRAM and an external 16MB Flash memory
- 2" LCD for clear colour picture display, 240 x 320 resolution, 262K colour
- ST7789T3 display driver, using SPI communication, effectively saving the IO resources
- QMI8658 6-axis IMU (3-axis accelerometer and 3-axis gyroscope)
- 3.7V MX1.25 Lithium battery recharge/discharge header
- USB Type-C port for power supply, program downloading, and debugging, more convenient for development use
- SD card slot for external SD card storage of pictures or files
- 22x GPIO pins for flexible configuration of pin function
- Camera interface, compatible with cameras such as OV2640 and OV5640 for image and video acquisition

---

# example code

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  EXAMPLE_PIN_NUM_LCD_DC /* DC */, EXAMPLE_PIN_NUM_LCD_CS /* CS */,
  EXAMPLE_PIN_NUM_LCD_SCLK /* SCK */, EXAMPLE_PIN_NUM_LCD_MOSI /* MOSI */, EXAMPLE_PIN_NUM_LCD_MISO /* MISO */);

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
Arduino_GFX *gfx = new Arduino_ST7789(
  bus, EXAMPLE_PIN_NUM_LCD_RST /* RST */, EXAMPLE_LCD_ROTATION /* rotation */, true /* IPS */,
  EXAMPLE_LCD_H_RES /* width */, EXAMPLE_LCD_V_RES /* height */);


---

# Link to the wiki

https://www.waveshare.com/wiki/ESP32-S3-LCD-2


# Pins

Physical Pin,Header Label,GPIO,Internal Shared Function / Notes
1,3V3,-,3.3V Power Output
2,GND,-,Ground
3,TXD,IO43,UART Transmit
4,RXD,IO44,UART Receive
5,GPIO47,IO47,Touch Panel SCL
6,GPIO48,IO48,Touch Panel SDA
7,GPIO15,IO15,Camera D2
8,GPIO13,IO13,Camera D1
9,GPIO11,IO11,Camera D3
10,GPIO12,IO12,Camera D0
11,GPIO14,IO14,Camera D4
12,GPIO9,IO9,Camera PCLK
13,GND,-,Ground
14,VBAT,-,Battery Input (3.7V Li-po)
15,5V,-,5V Power Input/Output
16,GND,-,Ground
17,GPIO19,IO19,Free I/O
18,GPIO20,IO20,Free I/O
19,GPIO10,IO10,Camera D5
20,GPIO7,IO7,Camera D6
21,GPIO8,IO8,Camera XCLK
22,GPIO21,IO21,Camera SCCB SDA
23,GPIO18,IO18,Free I/O
24,GPIO17,IO17,Camera PWDN
25,GPIO16,IO16,Camera SCCB SCL
26,GPIO6,IO6,Camera VSYNC
27,GPIO4,IO4,Camera HREF
28,GPIO2,IO2,Camera D7