# TFT Screen GFX Cheat Sheet

## Resolution
The display driver (`ST7789`) is initialized for **240 width x 320 height**. 
`(0,0)` is the top-left corner. `(240, 320)` is the bottom-right corner.

## Base Zone Map
The screen is vertically segmented based on the interaction model:
- `y = 0 to 240`: Main content area menus (typically displaying 3 items, 80px tall each).
- `y = 240 to 320`: Base inactive area (80px tall), used to show when the user's hand is detected but outside active range.

## `Arduino_GFX` Common Macros & Methods

### Drawing Basics
- `fillRect(x, y, w, h, color)`: Draw a solid rectangle.
- `fillCircle(x, y, r, color)`: Draw a solid circle.
- `fillTriangle(x0, y0, x1, y1, x2, y2, color)`: Necessary for rendering rotated geometries (as used in `DisplayManager::fillRotatedRect`).

### Text Basics
- Always set both foreground AND background color when writing text over a solid area to avoid partial redraw glitches: 
  `_gfx->setTextColor(txtColor, bgColor);`
- Default TextSizes:
  - `Size 2`: Medium Labels
  - `Size 3`: Menu Items (~21px tall)
  - `Size 4`: Large status (Bus time main digits)
  - `Size 5`: Huge timer display

### Text Centering
Use `getTextBounds` to center text dynamically.
```cpp
int16_t x1, y1;
uint16_t w, h;
_gfx->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);

int timerX = (240 - w) / 2;    // Center X
int drawY = targetY - (h / 2); // Center Y
```
