---
name: pio-esp32-workflow
description: Guidelines for compiling, debugging, and managing the ESP32-S3 hardware environment via PlatformIO. Use this skill when modifying platformio.ini, debugging serial output, or trying to understand the physical pinout of the board.
---

# ESP32-S3 PlatformIO Workflow

This skill contains the hardware configurations for the waveshare ESP32-S3-LCD-2 board.

## Build Flags
Because the ESP32-S3 handles USB natively (rather than through a separate UART chip), `platformio.ini` MUST contain:
```ini
build_flags = 
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
```
Without these flags, `Serial.print()` will not output anything over the USB-C connection, preventing debugging.

## Hardware Overrides
Pin definitions are now centralized in `Config.h` for better maintainability:
```cpp
// Display pins for ESP32-S3
#define TFT_SCK 39
#define TFT_MOSI 38
#define TFT_DC 42
#define TFT_CS 45
#define TFT_RST 0
#define TFT_BL 1

// Ultrasonic sensor pins
#define US_TRIG 17
#define US_ECHO 18
```

**Configuration Management**: All timing constants, validation limits, and hardware settings are centralized in `Config.h`. When modifying pin assignments or timing values, always update `Config.h` rather than hardcoding values throughout the codebase.

For the full list of available GPIOs (such as camera pins that can be repurposed), see [pinout_map.md](references/pinout_map.md).
