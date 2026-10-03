# TFT GFX Cheat Sheet

## Panel
ST7789, 240 wide x 320 tall, rotation 0, IPS. `(0,0)` is top-left. Created as `Arduino_ESP32SPI(dc, cs, sck, mosi, GFX_NOT_DEFINED)` + `Arduino_ST7789(bus, rst, 0, true, 240, 320)`; `setTextWrap(false)`.

## Layout
| y | Region | Hand distance |
|---|---|---|
| 0-80 | Row 0 | 0-5cm |
| 80-160 | Row 1 | 5-10cm |
| 160-240 | Row 2 | 10-15cm |
| 240-320 | Base strip | 15-25cm |

On interactive screens x 232-240 is the cursor gutter; content spans x 0 to `_paintW` (232).

Screen specifics:
- **Standby:** title lines (size 2) at y 24 / 46, label y 100 (size 2), next bus y 124-184 (up to size 5), status y 186 ("then N min", "No WiFi", "TfL unavailable"), breathing "Hold close to wake" at y 272. Label turns green "LEAVE NOW" during the alert.
- **Bus Times:** row 0 is info: main text y 0-56 (up to size 5), sub line y 56-80 (size 2); background green/orange/dark red by minutes, `G_1` when no data. Rows 1-2 Refresh / Back.
- **Timer:** size 5 timer in a 60px band, centre y 120 (menu hidden) moving to y 40 (menu open); menu rows slide up from y 240 to y 80.
- **Menus:** size 3 labels centred in 80px rows.

## Text
- Built-in font: 6 x 8 px per char at size 1, scaled by size. Width = `6 * size * strlen`, height = `8 * size`.
- Sizes in use: 2 labels/status, 3 menu items, up to 5 for bus minutes and the timer (via `fitTextSize`).
- Always draw changing text with `drawTextInBand()` (opaque fg/bg, paints around the box). Don't clear-then-print.
- `setTextBound()` is used to clip text; restore it to the full screen afterwards (as `drawTextInBand` does).

## Arduino_GFX calls used
- `fillRect`, `drawRect`, `fillTriangle` (via `fillRotatedRect` for the splash), `fillArc(cx, cy, rOuter, rInner, startDeg, endDeg, color)` (0 deg = 3 o'clock; avoid spanning the 360 wrap in one call).
- `setTextSize`, `setTextColor(fg, bg)`, `setCursor`, `print`.

## Partial-redraw checklist for a new element
1. Cache what was last drawn (string, colour, position).
2. Compare each tick; return early if unchanged.
3. Paint only the changed band/rect, within `_paintW`.
4. Reset the cache inside the `if (enterScreen(SCR_X))` first-draw branch of the screen's draw function.
5. If it moves, clear only the vacated strip (see `renderTimerFrame`, `drawCursorAt`).
