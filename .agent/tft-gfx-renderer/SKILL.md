---
name: tft-gfx-renderer
description: How DisplayManager draws to the 240x320 ST7789 with Arduino_GFX without flicker - partial redraws, flicker-free text bands, the cursor gutter, frame-capped animation and backlight PWM. Use this skill when modifying DisplayManager, adding screens or UI elements, changing colours, or animating anything.
---

# TFT GFX Renderer

There is no frame buffer: every pixel goes straight over SPI, so anything painted twice in a frame (black, then content) visibly flickers. The rules below exist to avoid that.

## Screen lifecycle: `enterScreen()` + caches
- Every `drawX()` calls `enterScreen(SCR_X)`. It returns true only the first time that screen is shown: do the full draw then, and afterwards repaint **only what changed**. All `drawX()` are safe to call every 50ms tick.
- `enterScreen()` resets the per-screen caches (`_activeRow = -10`, dwell bar, `_animating`). Each screen keeps its own "last drawn" cache (`_sbLabel/_sbBus/_sbStatus` on standby, `_busMain/_busSub/_busBg` on Bus Times, `_timerStr`, ...) and compares before drawing. Seed caches with impossible values (e.g. `"\x01"`, colour `0x0001`) to force the first paint.
- `drawMenuRows(labels, on, off, activeRow)` repaints only the old and new highlighted rows; a `nullptr` label leaves that row to the screen's own code (info rows on Timer and Bus Times).
- `fillScreen()` is used only for boot, standby (except after a cancelled unlock, which just erases the ring) and OTA. Interactive screens repaint their rows in place.

## Flicker-free text: `drawTextInBand()`
`drawTextInBand(text, size, bandY, bandH, fg, bg, clipBottom = CONTENT_H)` centres text in a horizontal band. It fills the four rectangles **around** the text box with `bg`, then prints the text **opaquely** (`setTextColor(fg, bg)`), so every pixel is written once and nothing flashes through black. Use it for any text that changes. It clips at `clipBottom` (used while the timer menu slides) and pre-fills partial glyph rows at the clip edge. Text width is `6 * size * len` (built-in 6x8 font); `fitTextSize(len, maxSize, TEXT_MAX_W)` picks the largest size that fits 228px.

## Cursor gutter and `_paintW`
Interactive screens (main, lights, timer, bus, success/error) reserve an 8px gutter (`GUTTER_W`) on the right for the hand cursor, a white marker on a grey rail. `_paintW` is `SCREEN_W - GUTTER_W` (232) on those screens and 240 elsewhere; **content must paint to `_paintW`, never 240**, or it will overwrite the rail. `setHandCursor(dist)` maps 4-22cm to y 0-240 (closer pins to 0) and 22-32cm to the base strip (240-320); `update()` eases the marker towards it and `drawCursorAt()` repaints only the uncovered rail.

## Animation: frame-capped `update()`
`DisplayManager::update()` is called every `loop()` pass and runs at most every `FRAME_INTERVAL_MS` (16ms, ~60fps), separately from the 50ms state tick. It handles:
- **Timer menu slide:** `drawTimerScreen(row, menuOpen)` only sets a target; `update()` eases `_openness` 0..1 (easeOutCubic over `TIMER_MENU_ANIM_MS`, scaled by distance left so a reversal is quick) and calls `renderTimerFrame()`, which clears only the strips vacated since the last frame. `isAnimating()` lets StateManager block selection mid-slide.
- Backlight fade, cursor easing, and the standby "breathing" hint.
Never animate with a blocking loop or `delay()`.

## Other building blocks
- `drawDwellProgress(row, progress)`: 6px bar along the bottom of the hovered row in the row's contrast colour; grows/shrinks incrementally, skipped while animating.
- `updateBaseZone(active)`: base strip (y 240-320), dark grey idle, light grey "Closer to select" when 22-32cm.
- `drawUnlockVisual(progress)`: green ring filled clockwise from 12 o'clock with `fillArc`, drawing only the new slice.
- `contrastColor(bg)`: black text on bright backgrounds, white on dark.

## Backlight
`TFT_BL` is PWM via LEDC: `ledcSetup(0, 5000, 8)`, `ledcAttachPin(bl, 0)`, `ledcWrite(0, level)`. This is the Arduino-ESP32 **2.0.x** API (the platform is pinned for it). `setAwake(bool)` fades over `BACKLIGHT_FADE_MS`; StateManager sleeps the display after 15 min in standby with no hand. OTA snaps it on because OTA blocks `update()`.

## Colours and layout constants
Defined in `src/DisplayManager.h` (not Config.h), RGB565: `C_DARK_RED`, `C_LIGHT_RED`, `C_ORANGE`, `C_GREEN`, `C_WHITE`, `C_BLACK`, `C_CYBER_PINK`, `C_CYBER_BLUE`, `C_CYBER_PURPLE`, `C_NEON_YELLOW`, greys `G_1/G_2/G_3`, `G_TEXT_DIM`, `G_BASE_DARK`, `G_BASE_LIGHT`. Layout: `SCREEN_W` 240, `CONTENT_H` 240, `ROW_H` 80, `NUM_ROWS` 3, `BASE_Y` 240, `BASE_H` 80, `GUTTER_W` 8. Bus colour thresholds (`BUS_*_MAX_MIN`) are in Config.h and shared with the lights.

Layout map and API notes: [gfx_cheatsheet.md](references/gfx_cheatsheet.md).
