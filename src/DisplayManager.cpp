#include "DisplayManager.h"
#include "Config.h"
#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

// Unlock ring geometry
#define RING_CX 120
#define RING_CY 120
#define RING_OUTER 70
#define RING_INNER 58

// Timer layout
#define TIMER_BAND_H 60
#define TIMER_TEXT_SIZE 5

#define DWELL_BAR_H 6
#define TEXT_MAX_W 228 // Leave a 6px margin either side

static const char *STANDBY_HINT = "Hold close to wake";
#define STANDBY_HINT_SIZE 2
#define STANDBY_HINT_Y 272
#define BREATH_PERIOD_MS 3000
#define BREATH_MIN 40
#define BREATH_MAX 170

static uint16_t grey565(uint8_t level) {
  return ((level & 0xF8) << 8) | ((level & 0xFC) << 3) | (level >> 3);
}

DisplayManager::DisplayManager(int sck, int mosi, int dc, int cs, int rst,
                               int bl) {
  _blPin = bl;
  _bus = new Arduino_ESP32SPI(dc, cs, sck, mosi, GFX_NOT_DEFINED);
  _gfx = new Arduino_ST7789(_bus, rst, 0, true, 240, 320);
}

void DisplayManager::begin() {
  _gfx->begin();
  _gfx->setTextWrap(false);
  // Clear before the backlight comes on so stale panel RAM never shows
  _gfx->fillScreen(C_BLACK);
  pinMode(_blPin, OUTPUT);
  digitalWrite(_blPin, HIGH);
}

// --- Helpers ---

// Returns true the first time a screen is shown, so callers know to do a
// full draw. Resets all per-screen caches.
bool DisplayManager::enterScreen(Screen s) {
  if (_screen == s)
    return false;
  _screen = s;
  _activeRow = -10;
  _dwellRow = -1;
  _dwellW = 0;
  _animating = false;
  return true;
}

// Black text on bright backgrounds, white on dark ones
uint16_t DisplayManager::contrastColor(uint16_t bg) {
  uint32_t r = ((bg >> 11) & 0x1F) << 3;
  uint32_t g = ((bg >> 5) & 0x3F) << 2;
  uint32_t b = (bg & 0x1F) << 3;
  uint32_t lum = (299 * r + 587 * g + 114 * b) / 1000;
  return lum > 140 ? C_BLACK : C_WHITE;
}

// Largest built-in font size (6px * size per char) that fits maxWidth
uint8_t DisplayManager::fitTextSize(size_t len, uint8_t maxSize,
                                    int maxWidth) {
  for (uint8_t s = maxSize; s > 1; s--) {
    if ((int)(6 * s * len) <= maxWidth)
      return s;
  }
  return 1;
}

void DisplayManager::fillRectClipped(int x, int y, int w, int h,
                                     uint16_t color, int clipTop,
                                     int clipBottom) {
  if (y < clipTop) {
    h -= clipTop - y;
    y = clipTop;
  }
  if (y + h > clipBottom)
    h = clipBottom - y;
  if (w <= 0 || h <= 0)
    return;
  _gfx->fillRect(x, y, w, h, color);
}

// Flicker-free text: paints the band around the text box, then draws the
// text opaquely, so every pixel is written exactly once. Nothing ever
// flashes through black on its way to the new content.
void DisplayManager::drawTextInBand(const char *text, uint8_t size, int bandY,
                                    int bandH, uint16_t fg, uint16_t bg,
                                    int clipBottom) {
  int len = strlen(text);
  int tw = 6 * size * len;
  int th = 8 * size;
  if (tw > SCREEN_W)
    tw = SCREEN_W;
  int tx = (SCREEN_W - tw) / 2;
  int ty = bandY + (bandH - th) / 2;
  int bandBottom = bandY + bandH;

  fillRectClipped(0, bandY, SCREEN_W, ty - bandY, bg, 0, clipBottom);
  fillRectClipped(0, ty + th, SCREEN_W, bandBottom - (ty + th), bg, 0,
                  clipBottom);
  fillRectClipped(0, ty, tx, th, bg, 0, clipBottom);
  fillRectClipped(tx + tw, ty, SCREEN_W - (tx + tw), th, bg, 0, clipBottom);

  int visibleBottom = min(bandBottom, clipBottom);
  if (len == 0 || ty >= visibleBottom)
    return;

  // The font clips whole scaled pixels, so pre-fill any partial glyph row
  // at the clip edge (only happens while the menu is sliding)
  int visible = visibleBottom - ty;
  if (visible < th && visible % size) {
    _gfx->fillRect(tx, visibleBottom - visible % size, tw, visible % size, bg);
  }

  _gfx->setTextBound(0, 0, SCREEN_W, visibleBottom);
  _gfx->setTextSize(size);
  _gfx->setTextColor(fg, bg);
  _gfx->setCursor(tx, ty);
  _gfx->print(text);
  _gfx->setTextBound(0, 0, SCREEN_W, BASE_Y + BASE_H);
}

void DisplayManager::drawRow(int row, const char *label, bool active,
                             uint16_t activeColor, uint16_t inactiveColor) {
  uint16_t bg = active ? activeColor : inactiveColor;
  drawTextInBand(label, 3, row * ROW_H, ROW_H, contrastColor(bg), bg);
  _rowColor[row] = activeColor;
  if (row == _dwellRow)
    _dwellW = 0; // Repaint wiped the bar
}

// Repaints only the rows whose highlight changed
void DisplayManager::drawMenuRows(const char *const labels[NUM_ROWS],
                                  const uint16_t on[NUM_ROWS],
                                  const uint16_t off[NUM_ROWS],
                                  int activeRow) {
  if (activeRow == _activeRow)
    return;
  bool full = (_activeRow == -10);
  for (int row = 0; row < NUM_ROWS; row++) {
    if (!labels[row])
      continue;
    if (full || row == activeRow || row == _activeRow)
      drawRow(row, labels[row], row == activeRow, on[row], off[row]);
  }
  _activeRow = activeRow;
}

// --- Animation tick ---

void DisplayManager::update() {
  unsigned long now = millis();
  if (now - _lastFrame < FRAME_INTERVAL_MS)
    return;
  _lastFrame = now;

  if (_animating && _screen == SCR_TIMER) {
    float target = _menuTarget ? 1.0f : 0.0f;
    // Duration scales with distance left, so a reversal mid-slide is quick
    float dur = fabsf(target - _animFrom) * TIMER_MENU_ANIM_MS;
    float t = dur > 0 ? (now - _animStart) / dur : 1.0f;
    if (t >= 1.0f) {
      t = 1.0f;
      _animating = false;
    }
    float eased = 1.0f - powf(1.0f - t, 3); // easeOutCubic
    _openness = _animFrom + (target - _animFrom) * eased;
    renderTimerFrame(_openness);
    if (!_animating)
      _activeRow = -10; // Let the next draw apply the hover state
  }

  if (_screen == SCR_STANDBY) {
    // Slow "breathing" hint so the idle screen feels alive
    float phase = (now % BREATH_PERIOD_MS) / (float)BREATH_PERIOD_MS;
    float b = 0.5f - 0.5f * cosf(2.0f * PI * phase);
    uint16_t c = grey565(BREATH_MIN + (uint8_t)(b * (BREATH_MAX - BREATH_MIN)));
    if (c != _breathColor)
      drawStandbyHint(c);
  }
}

// --- Screens ---

void DisplayManager::drawBootScreen() {
  if (!enterScreen(SCR_BOOT))
    return;

  _gfx->fillScreen(C_BLACK);
  _baseState = -1;

  // Splash Screen: 3 Rotated Rectangles (W=30, H=145, -13 deg)
  int16_t w = 30;
  int16_t h = 145;
  float angle = -13.0;
  fillRotatedRect(46, 86, w, h, angle, C_WHITE);
  fillRotatedRect(94, 86, w, h, angle, C_WHITE);
  fillRotatedRect(142, 86, w, h, angle, C_WHITE);
}

void DisplayManager::drawStandbyHint(uint16_t color) {
  _breathColor = color;
  int tw = 6 * STANDBY_HINT_SIZE * strlen(STANDBY_HINT);
  _gfx->setTextSize(STANDBY_HINT_SIZE);
  _gfx->setTextColor(color, C_BLACK);
  _gfx->setCursor((SCREEN_W - tw) / 2, STANDBY_HINT_Y);
  _gfx->print(STANDBY_HINT);
}

void DisplayManager::drawStandbyScreen(const BusView &bus) {
  if (enterScreen(SCR_STANDBY)) {
    _gfx->fillScreen(C_BLACK);
    _baseState = -1;
    drawTextInBand("COFFEE ZONE", 2, 24, 22, C_CYBER_PINK, C_BLACK);
    drawTextInBand("CONTROLLER", 2, 46, 22, C_CYBER_PINK, C_BLACK);
    drawStandbyHint(grey565(BREATH_MIN));
    // Impossible values, so the dynamic bands below all paint
    strcpy(_sbLabel, "\x01");
    strcpy(_sbBus, "\x01");
    strcpy(_sbStatus, "\x01");
  }

  char label[20];
  char main[20];
  char status[20] = "";

  if (bus.minutes >= 0 && bus.line && bus.line[0])
    snprintf(label, sizeof(label), "NEXT %s", bus.line);
  else
    snprintf(label, sizeof(label), "NEXT BUS");

  if (bus.minutes == 0)
    snprintf(main, sizeof(main), "Due");
  else if (bus.minutes > 0)
    snprintf(main, sizeof(main), "%d min", bus.minutes);
  else if (bus.minutes == BUS_NO_BUSES)
    snprintf(main, sizeof(main), "No buses");
  else
    snprintf(main, sizeof(main), "--");

  if (!bus.online)
    snprintf(status, sizeof(status), "No WiFi");
  else if (bus.minutes == BUS_API_ERROR)
    snprintf(status, sizeof(status), "TfL unavailable");

  if (strcmp(label, _sbLabel) != 0) {
    strcpy(_sbLabel, label);
    drawTextInBand(label, 2, 100, 24, G_TEXT_DIM, C_BLACK);
  }
  if (strcmp(main, _sbBus) != 0) {
    strcpy(_sbBus, main);
    drawTextInBand(main, fitTextSize(strlen(main), 5, TEXT_MAX_W), 124, 60,
                   C_CYBER_BLUE, C_BLACK);
  }
  if (strcmp(status, _sbStatus) != 0) {
    strcpy(_sbStatus, status);
    drawTextInBand(status, 2, 186, 26, C_LIGHT_RED, C_BLACK);
  }
}

void DisplayManager::drawUnlockVisual(float progress) {
  if (enterScreen(SCR_UNLOCK)) {
    _gfx->fillRect(0, 0, SCREEN_W, CONTENT_H, C_BLACK);
    _gfx->fillArc(RING_CX, RING_CY, RING_OUTER, RING_INNER, 0, 360, G_1);
    drawTextInBand("Hold", 3, RING_CY - 20, 40, C_WHITE, C_BLACK);
    _unlockAngle = 0;
  }

  if (progress < 0)
    progress = 0;
  if (progress > 1)
    progress = 1;
  float target = progress * 360.0f;
  if (target - _unlockAngle < 1.0f)
    return;

  // Fill the ring clockwise from 12 o'clock, only drawing the new slice.
  // fillArc is happiest without a 360 wrap, so split there.
  float s = 270.0f + max(0.0f, _unlockAngle - 1.0f); // 1 deg overlap, no seams
  float e = 270.0f + target;
  if (e <= 359.9f) {
    _gfx->fillArc(RING_CX, RING_CY, RING_OUTER, RING_INNER, s, e, C_GREEN);
  } else if (s >= 360.0f) {
    _gfx->fillArc(RING_CX, RING_CY, RING_OUTER, RING_INNER, s - 360, e - 360,
                  C_GREEN);
  } else {
    _gfx->fillArc(RING_CX, RING_CY, RING_OUTER, RING_INNER, s, 359.9f,
                  C_GREEN);
    _gfx->fillArc(RING_CX, RING_CY, RING_OUTER, RING_INNER, 0, e - 360,
                  C_GREEN);
  }
  _unlockAngle = target;
}

void DisplayManager::drawMainMenu(int activeRow) {
  enterScreen(SCR_MAIN);
  static const char *const labels[NUM_ROWS] = {"Timer", "Bus Times",
                                               "Lights"};
  static const uint16_t on[NUM_ROWS] = {C_CYBER_PINK, C_NEON_YELLOW,
                                        C_CYBER_BLUE};
  static const uint16_t off[NUM_ROWS] = {G_1, G_2, G_3};
  drawMenuRows(labels, on, off, activeRow);
}

void DisplayManager::drawLightSettings(int activeRow) {
  enterScreen(SCR_LIGHTS);
  static const char *const labels[NUM_ROWS] = {"Rainbow", "Coffee", "Back"};
  static const uint16_t on[NUM_ROWS] = {C_CYBER_PINK, C_CYBER_BLUE,
                                        C_CYBER_PURPLE};
  static const uint16_t off[NUM_ROWS] = {G_1, G_2, G_3};
  drawMenuRows(labels, on, off, activeRow);
}

void DisplayManager::drawBusScreen(const BusView &bus, int activeRow) {
  if (enterScreen(SCR_BUS)) {
    _busMain[0] = _busSub[0] = '\0';
    _busBg = 0x0001; // Never a real colour, forces the info row to paint
  }

  // Info row (row 0) colour reflects how good the timing is
  uint16_t bg;
  int m = bus.minutes;
  if (m < 0)
    bg = G_1;
  else if (m <= 5 || m > 12)
    bg = C_DARK_RED;
  else if (m <= 9)
    bg = C_GREEN;
  else
    bg = C_ORANGE;

  char main[20];
  char sub[24] = "";
  if (m == 0)
    snprintf(main, sizeof(main), "Due");
  else if (m > 0)
    snprintf(main, sizeof(main), "%d min", m);
  else if (m == BUS_NO_BUSES)
    snprintf(main, sizeof(main), "No buses");
  else if (m == BUS_OFFLINE && !bus.refreshing)
    snprintf(main, sizeof(main), "No WiFi");
  else if (m == BUS_API_ERROR && !bus.refreshing)
    snprintf(main, sizeof(main), "No data");
  else
    snprintf(main, sizeof(main), "Loading...");

  if (bus.refreshing && m >= 0)
    snprintf(sub, sizeof(sub), "Updating...");
  else if (m >= 0 && bus.line && bus.line[0])
    snprintf(sub, sizeof(sub), "Route %s", bus.line);

  bool bgChanged = (bg != _busBg);
  _busBg = bg;
  uint16_t fg = contrastColor(bg);
  if (bgChanged || strcmp(main, _busMain) != 0) {
    strcpy(_busMain, main);
    drawTextInBand(main, fitTextSize(strlen(main), 5, TEXT_MAX_W), 0, 56, fg,
                   bg);
  }
  if (bgChanged || strcmp(sub, _busSub) != 0) {
    strcpy(_busSub, sub);
    drawTextInBand(sub, 2, 56, ROW_H - 56, fg, bg);
  }

  static const char *const labels[NUM_ROWS] = {nullptr, "Refresh", "Back"};
  static const uint16_t on[NUM_ROWS] = {0, C_CYBER_BLUE, C_CYBER_PURPLE};
  static const uint16_t off[NUM_ROWS] = {0, G_2, G_3};
  drawMenuRows(labels, on, off, activeRow);
}

void DisplayManager::drawSuccessScreen(const char *message) {
  enterScreen(SCR_MESSAGE);
  drawTextInBand(message, fitTextSize(strlen(message), 3, TEXT_MAX_W), 0,
                 CONTENT_H, C_BLACK, C_GREEN);
}

void DisplayManager::drawErrorScreen(const char *message) {
  enterScreen(SCR_MESSAGE);
  drawTextInBand(message, fitTextSize(strlen(message), 3, TEXT_MAX_W), 0,
                 CONTENT_H, C_WHITE, C_DARK_RED);
}

void DisplayManager::drawDwellProgress(int row, float progress) {
  if (row != _dwellRow) {
    // A new row is hovered; it was just repainted so it has no bar yet
    _dwellRow = row;
    _dwellW = 0;
  }
  if (row < 0 || row >= NUM_ROWS || row != _activeRow || _animating)
    return;

  if (progress < 0)
    progress = 0;
  if (progress > 1)
    progress = 1;
  int w = (int)(progress * SCREEN_W);
  int y = row * ROW_H + ROW_H - DWELL_BAR_H;

  if (w > _dwellW) {
    _gfx->fillRect(_dwellW, y, w - _dwellW, DWELL_BAR_H,
                   contrastColor(_rowColor[row]));
  } else if (w < _dwellW) {
    _gfx->fillRect(w, y, _dwellW - w, DWELL_BAR_H, _rowColor[row]);
  }
  _dwellW = w;
}

void DisplayManager::updateBaseZone(bool active) {
  int state = active ? 1 : 0;
  if (state == _baseState)
    return;
  _baseState = state;

  if (active) {
    drawTextInBand("Closer to select", 2, BASE_Y, BASE_H, C_WHITE,
                   G_BASE_LIGHT, BASE_Y + BASE_H);
  } else {
    _gfx->fillRect(0, BASE_Y, SCREEN_W, BASE_H, G_BASE_DARK);
  }
}

// --- Timer ---

int DisplayManager::timerCentreY(float openness) const {
  return 120 - (int)lroundf(80.0f * openness);
}

int DisplayManager::menuTop(float openness) const {
  return CONTENT_H - (int)lroundf(160.0f * openness);
}

void DisplayManager::drawTimerScreen(int activeRow, bool menuOpen) {
  if (enterScreen(SCR_TIMER)) {
    _gfx->fillRect(0, 0, SCREEN_W, CONTENT_H, C_BLACK);
    strcpy(_timerStr, "00:00");
    _openness = menuOpen ? 1.0f : 0.0f;
    _menuTarget = menuOpen;
    _drawnTimerY = -1;
    _drawnMenuTop = CONTENT_H;
    renderTimerFrame(_openness);
  }

  if (menuOpen != _menuTarget) {
    // Start (or reverse) the slide from wherever it currently is
    _menuTarget = menuOpen;
    _animFrom = _openness;
    _animStart = millis();
    _animating = true;
    _activeRow = -10;
    return;
  }

  if (_animating || !_menuTarget)
    return;

  static const char *const labels[NUM_ROWS] = {nullptr, "Reset", "Back"};
  static const uint16_t on[NUM_ROWS] = {0, C_NEON_YELLOW, C_CYBER_PURPLE};
  static const uint16_t off[NUM_ROWS] = {0, G_1, G_2};
  drawMenuRows(labels, on, off, activeRow);
}

// One frame of the timer/menu layout at a given openness. Clears only the
// strips that were vacated since the last frame, then paints the new state.
void DisplayManager::renderTimerFrame(float openness) {
  int cy = timerCentreY(openness);
  int top = menuTop(openness);
  int bandTop = cy - TIMER_BAND_H / 2;
  int bandBottom = cy + TIMER_BAND_H / 2;

  // 1. Strip the menu uncovered while sliding down
  if (top > _drawnMenuTop)
    _gfx->fillRect(0, _drawnMenuTop, SCREEN_W, top - _drawnMenuTop, C_BLACK);

  // 2. Strip the timer uncovered while moving
  if (_drawnTimerY >= 0 && _drawnTimerY != cy) {
    int oldTop = _drawnTimerY - TIMER_BAND_H / 2;
    int oldBottom = _drawnTimerY + TIMER_BAND_H / 2;
    int clearTop, clearBottom;
    if (cy < _drawnTimerY) {
      clearTop = max(oldTop, bandBottom);
      clearBottom = oldBottom;
    } else {
      clearTop = oldTop;
      clearBottom = min(oldBottom, bandTop);
    }
    fillRectClipped(0, clearTop, SCREEN_W, clearBottom - clearTop, C_BLACK, 0,
                    top);
  }

  // 3. Timer, clipped so it never paints over the menu
  drawTextInBand(_timerStr, TIMER_TEXT_SIZE, bandTop, TIMER_BAND_H, C_WHITE,
                 C_BLACK, top);

  // 4. Menu rows riding up with the slide
  if (top < CONTENT_H) {
    drawTextInBand("Reset", 3, top, ROW_H, contrastColor(G_1), G_1);
    drawTextInBand("Back", 3, top + ROW_H, ROW_H, contrastColor(G_2), G_2);
  }

  _drawnTimerY = cy;
  _drawnMenuTop = top;
}

void DisplayManager::drawTimerValue(unsigned long elapsedMs) {
  unsigned long totalSeconds = elapsedMs / 1000;
  char buf[sizeof(_timerStr)];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", totalSeconds / 60,
           totalSeconds % 60);

  if (strcmp(buf, _timerStr) == 0)
    return;
  strcpy(_timerStr, buf);

  // While sliding, the next animation frame picks up the new value
  if (_screen != SCR_TIMER || _animating)
    return;

  int cy = timerCentreY(_openness);
  drawTextInBand(_timerStr, TIMER_TEXT_SIZE, cy - TIMER_BAND_H / 2,
                 TIMER_BAND_H, C_WHITE, C_BLACK, menuTop(_openness));
}

void DisplayManager::fillRotatedRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                     float angleDeg, uint16_t color) {
  float rad = angleDeg * PI / 180.0;
  float c = cos(rad);
  float s = sin(rad);

  // Pivot at Center
  float cx = x + w / 2.0;
  float cy = y + h / 2.0;

  float corners[4][2] = {{-w / 2.0f, -h / 2.0f},
                         {w / 2.0f, -h / 2.0f},
                         {w / 2.0f, h / 2.0f},
                         {-w / 2.0f, h / 2.0f}};

  int16_t px[4], py[4];

  for (int i = 0; i < 4; i++) {
    float rx = corners[i][0] * c - corners[i][1] * s;
    float ry = corners[i][0] * s + corners[i][1] * c;
    px[i] = (int16_t)(cx + rx);
    py[i] = (int16_t)(cy + ry);
  }

  _gfx->fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], color);
  _gfx->fillTriangle(px[0], py[0], px[2], py[2], px[3], py[3], color);
}
