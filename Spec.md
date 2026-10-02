# Spec v2

## System Overview
This project is an ESP32-S3 based application that uses an ultrasonic sensor to create a touchless, distance-based navigation interface. The user interacts with the device by moving their hand up and down above the sensor.

**Key Features:**
- **Splash Screen:** Visual boot indicator with smart delay (waits for WiFi/Data).
- **Unlock UI:** Proximity-based wake-up (< 3cm).
- **Standby Mode:** Displays useful info (Lights, Bus) with **Breathing Animation**.
- **Menu Navigation:** Distance-mapped cursor to select Timer, Lights, or Bus Times.
- **Smart Integrations:** Controls WLED lights and fetches TfL Bus times.

## System Architecture & Connectivity
- **FreeRTOS NetworkManager**: All HTTP requests to the WLED Controller and TfL API are offloaded to a background task pinned to **Core 0** with 16KB stack size for HTTP safety. This ensures the main UI loop and distance sensor polling remains at 100% responsiveness without hanging during network latency.
- **Network Resilience**: Automatic WiFi reconnection every 30 seconds if connection is lost.
- **Safety Features**: Strict network timeouts (3 seconds), bounds checking on all HTTP operations, and protection against infinite JSON parsing loops.
- **Memory Management**: Proper cleanup of dynamically allocated objects to prevent memory leaks.

## Display Settings & Hardware
**Pins** (Now centralized in `Config.h`):
```cpp
// Pin definitions moved to Config.h for better maintainability
// Display pins for ESP32-S3
#define TFT_SCK 39
#define TFT_MOSI 38
#define TFT_MISO GFX_NOT_DEFINED
#define TFT_DC 42
#define TFT_CS 45
#define TFT_RST 0
#define TFT_BL 1

// Ultrasonic sensor pins
#define US_TRIG 17
#define US_ECHO 18
```

**Configuration Management**: All timing constants, validation limits, and hardware settings are now centralized in `Config.h` for easy tuning and maintenance.

## Core Interaction Logic

### Boot Sequence
- **Splash Screen**: 
    - **Visuals**: 3 Rotated Rectangles (White on Black).
    - **Duration**: Minimum 2 seconds.
    - **Smart Delay**: Remains active until WiFi is connected AND the first bus fetch answers (a "no buses" answer counts), capped at 10 seconds so the device never hangs on the splash at night or offline.
- **Sensor Warmup**: First 2 seconds of sensor readings are ignored to prevent false triggers.

### Unlock & Standby
- **Standby Mode (Default)**:
    - **Display**: Shows current **WLED Status** (Color/Effect) and **Next Bus Time**.
    - **Updates**: Bus info refreshes every 1 minute.
    - **Behavior**: Screen is ON but showing passive info.
    - **Flicker Fix**: Base zone updates are disabled in standby to prevent screen flickering.
- **WLED Integration**:
    - **Loading State**: Sets WLED strip to **Blue Breathing Animation** while fetching data.
    - **Status Indication**: Sets WLED strip color based on bus arrival time (Red/Green/Orange).
- **Unlock Sequence**:
    - **Trigger**: Hand must come closer than **3cm** to the sensor.
    - **Feedback**: "Activate" visualization (e.g., circle filling) to confirm wake-up.
    - **Action**: Transitions to **Main Screen (Homepage)**.
- **Timeout (Auto-Lock)**:
    - If no interaction/selection is made for **30 seconds**, the device returns to **Standby Mode**.
    - Exception: a running timer keeps the screen awake for up to 1 hour.

### Distance Zones (Active Mode)
- **Usable Zone (< 15cm)**: This is the active interaction area. The screen content (menus, sliders) is mapped to this physical distance range.
- **Base/Inactive Zone (> 15cm)**:
    - Represented by a Grey Base at the bottom of the screen.

### Interaction Mechanics
- **Navigation**: Menu options are vertically stacked and split the 15cm "Usable Zone" proportionally.
- **Selection ("Dwell Time")**: To select an item, the user must hold their hand steady within the target zone for **1 second**. A progress bar fills along the bottom of the hovered row during the dwell.
- **Re-arm**: After any selection or screen change, nothing can be selected until the hand moves to a different row (or away). This stops a still hand from chaining selections across screens.
- **Non-blocking Execution**: Menu interactions, success screens, and background tasks do not use explicit delays (`delay()`). UI fluidity is maintained using `millis()` state checks.
- **Smoothing & Stability**:
    - **EMA Smoothing**: Use Exponential Moving Average (Alpha ~0.15).
    - **Hysteresis**: Buffer (+/- 0.5cm) at zone boundaries.
    - **Input Validation**: Distance readings are validated (0.2cm - 400cm range) with pin validation.
    - **Error Recovery**: Robust error handling prevents crashes from invalid sensor data.

### UI Styling
- **Text Alignment**: Centered.
- **Animations**: Smooth transitions, sliders animating from bottom.

## Screens & Menu Structure

### 1. Main Screen (Homepage)
- **Layout**: Split into 3 vertical zones (each ~5cm physical height).
- **Options**:
    - **Top**: "Timer"
    - **Middle**: "Bus Times"
    - **Bottom**: "Lights"
- **Behavior**: Hover highlights; Hold 1s selects.

### 2. Timer Screen
- **Initial State (Idle)**:
    - **Visuals**: Displays current timer (MM:SS) in **Large Font**.
- **Active State (Menu Open)**:
    - **Zones**:
        - **Top (0-5cm)**: Dead Zone.
        - **Middle (5-10cm)**: "Reset".
        - **Bottom (10-15cm)**: "Back".

### 3. Light Settings Screen
- **Layout**: Split into 3 zones.
- **Options**:
    - **Rainbow**: Sets WLED to rainbow cycling.
    - **Solid**: Sets WLED to solid color.
    - **Back**: Returns to Main Screen.
- **Logic**: Sends HTTP request to WLED IP.

### 4. Bus Times Screen
- **Display**:
    - Shows **Time to Next Bus** (e.g., "7 mins"), "Due", or "Loading...".
    - Background/Element color reflects status.
    - **API Error State**: If the network times out or fails natively, displays a red "API Error" screen for 3 seconds before automatically returning to the Homepage.
- **Logic (Derived from Previous App)**:
    - **Red**: <= 5 mins OR > 12 mins.
    - **Green**: 6 - 9 mins.
    - **Orange**: 10 - 12 mins.
    - **WLED Control**: Also updates the WLED strip color to match the status.
- **Updates**: Fetches data on entry and every 30s. Minutes count down locally between fetches. A failed refresh keeps showing the last good data for up to 3 minutes.
- **Interaction** (same pattern as the Timer screen):
    - **Top (0-5cm)**: Bus info (dead zone).
    - **Middle (5-10cm)**: "Refresh" - fetch again and replay the WLED status effect.
    - **Bottom (10-15cm)**: "Back".

### 5. Always-on Features
- **Hand Cursor**: On menu screens a marker in the right-hand gutter shows where the hand maps (0-15cm over the rows, 15-25cm over the base).
- **Next Two Buses**: Standby and Bus Times show the bus after next ("then 11 min").
- **Leave Now Alert**: Mon-Fri 07:00-09:30, while a bus is 8 to 6 minutes away (5 min walk), WLED pulses green and standby shows "LEAVE NOW". Configurable in `Config.h`.
- **Display Sleep**: Backlight fades off after 15 minutes idle in standby; a hand within 25cm wakes it.
- **OTA Updates**: `pio run -e esp32s3_ota -t upload` once `OTA_PASSWORD` is set in `secrets.h`.

## System Reliability & Maintenance

### Recent Improvements (v2.1)
- **Screen Flickering Fix**: Resolved standby screen flickering by preventing unnecessary base zone updates
- **Memory Management**: Added proper destructors to prevent memory leaks from dynamic allocations
- **Network Stack Safety**: Increased network task stack from 8KB to 16KB to prevent stack overflow
- **JSON Parsing Safety**: Added iteration limits and bounds checking to prevent infinite parsing loops
- **Configuration Centralization**: Moved all constants to `Config.h` for better maintainability
- **WiFi Resilience**: Added automatic reconnection logic with 30-second intervals
- **Input Validation**: Comprehensive validation throughout codebase prevents crashes from invalid data
- **String Safety**: Added bounds checking for all HTTP URL construction and string operations

### Debugging & Diagnostics
- Serial output provides detailed status information for all major operations
- Network operations include timeout and error logging
- Sensor validation includes range checking and pin verification
- All critical operations have error handling with graceful degradation