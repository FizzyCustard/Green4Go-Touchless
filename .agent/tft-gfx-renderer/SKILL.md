---
name: tft-gfx-renderer
description: Guidelines for managing the ESP32 SPI TFT display using the Arduino_GFX library. Use this skill when modifying DisplayManager.cpp, drawing new shapes, handling custom color palettes, or animating UI elements without screen flickering.
---

# TFT GFX Renderer

This skill explains how to modify the physical display aspects of the project.

## Dirty Rectangles for Animations

Never use `_gfx->fillScreen(C_BLACK)` outside of a physical screen transition (e.g., from BOOT to MAIN MENU). The ESP32 SPI bus speed causes full screen redraws to visibly flash or tear. 

To animate elements without flickering, you must calculate the exact bounds of the previous frame and overdraw it with the background color (`C_BLACK`) **before** drawing the new frame.

**Non-blocking Animations**:
Lengthy UI animations (like menus sliding) must use a state machine rather than a blocking `for` loop with `delay()`. 
Track `_animStartTime` and `_animating` flags, calculate intermediate frames based on `(millis() - _animStartTime) / duration`, and call a `update()` tick method on every cycle of the main `loop()`. For an example, see `DisplayManager::animateTimerMenu()`.

## Custom Colors

The project uses a specific 16-bit RGB565 color palette. To use hex codes or RGB values, convert them using the macros in `Config.h`, or stick to the pre-defined constants like `C_CYBER_PINK` and `C_GREEN`.

For a cheat sheet of display layout properties and common UI elements, view [gfx_cheatsheet.md](references/gfx_cheatsheet.md).
