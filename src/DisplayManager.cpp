#include "DisplayManager.h"
#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

DisplayManager::DisplayManager(int sck, int mosi, int dc, int cs, int rst,
                               int bl) {
  _blPin = bl;
  _bus = new Arduino_ESP32SPI(dc, cs, sck, mosi, GFX_NOT_DEFINED);
  _gfx = new Arduino_ST7789(_bus, rst, 0, true, 240, 320);

  // Initialize cache
  memset(_lastTimerStr, 0, 9);
  _lastTimerY = -1;
  _lastBusMinutes = -100;
  _lastStandbyMinutes = -100;
}

void DisplayManager::begin() {
  _gfx->begin();
  pinMode(_blPin, OUTPUT);
  digitalWrite(_blPin, HIGH);
  _gfx->fillScreen(C_BLACK);

  // Draw initial base
  updateBaseZone(false);
}

DisplayManager::~DisplayManager() {
  // Clean up dynamically allocated graphics objects
  if (_gfx) {
    delete _gfx;
    _gfx = nullptr;
  }
  if (_bus) {
    delete _bus;
    _bus = nullptr;
  }
}

void DisplayManager::clearContentArea() {
  _gfx->fillRect(0, 0, 240, 240, C_BLACK);
  _lastActiveIndex = -10; // Force redraw of menus
}

void DisplayManager::updateBaseZone(bool active) {
  if (_lastBaseActive == active)
    return;

  uint16_t color = active ? G_BASE_LIGHT : G_BASE_DARK;
  // Base is 240 to 320
  _gfx->fillRect(0, 240, 240, 80, color);
  _lastBaseActive = active;
}

void DisplayManager::drawMenuItem(int y, int h, const char *label, bool active,
                                  uint16_t activeColor,
                                  uint16_t inactiveColor) {
  int maxY = 240;

  if (y >= maxY)
    return; // Completely hidden

  int effectiveH = h;
  if (y + h > maxY) {
    effectiveH = maxY - y;
  }

  uint16_t color = active ? activeColor : inactiveColor;
  _gfx->fillRect(0, y, 240, effectiveH, color);

  // Text
  _gfx->setTextColor(C_WHITE);
  _gfx->setTextSize(3);

  int16_t x1, y1;
  uint16_t w, h_text;
  _gfx->getTextBounds(label, 0, 0, &x1, &y1, &w, &h_text);

  int xPos = (240 - w) / 2;
  int yPos = y + (h - h_text) / 2;

  if (yPos + h_text > maxY) {
    return;
  }
  if (yPos < 0)
    return;

  _gfx->setCursor(xPos, yPos);
  _gfx->print(label);
}

void DisplayManager::drawMainMenu(int activeIndex) {
  if (activeIndex == _lastActiveIndex)
    return;

  // 3 Items: Timer, Bus Times, Lights
  // Each ~80px height

  // Item 0: Timer
  drawMenuItem(0, 80, "Timer", (activeIndex == 0), C_CYBER_PINK, G_1);
  // Item 1: Bus Times
  drawMenuItem(80, 80, "Bus Times", (activeIndex == 1), C_NEON_YELLOW, G_2);
  // Item 2: Lights
  drawMenuItem(160, 80, "Lights", (activeIndex == 2), C_CYBER_BLUE, G_3);

  _lastActiveIndex = activeIndex;
}

void DisplayManager::drawBusScreen(int minutes, int activeIndex) {
  if (minutes == _lastBusMinutes && activeIndex == _lastActiveIndex)
    return;

  _lastBusMinutes = minutes;
  _lastActiveIndex = activeIndex;

  // Background based on time (Top 2/3rds)
  uint16_t bgColor = C_BLACK;
  uint16_t txtColor = C_WHITE;

  if (minutes < 0) {
    bgColor = G_1; // Grey for unknown
  } else if (minutes <= 5 || minutes > 12) {
    bgColor = C_DARK_RED;
  } else if (minutes >= 6 && minutes <= 9) {
    // 6-9 mins: Green (Comfortable)
    bgColor = C_GREEN;
    txtColor = C_BLACK; // Green is bright, use black text
  } else {
    bgColor = C_ORANGE;
    txtColor = C_BLACK;
  }

  // Draw Bus Info Area (Top 160px)
  _gfx->fillRect(0, 0, 240, 160, bgColor);

  _gfx->setTextColor(txtColor, bgColor);
  _gfx->setTextSize(4);

  String text;
  if (minutes == -1)
    text = "Loading...";
  else if (minutes == -3)
    text = "No Buses";
  else if (minutes < 0)
    text = "No Data";
  else if (minutes == 0)
    text = "Due";
  else
    text = String(minutes) + " min";

  int16_t x1, y1;
  uint16_t w, h;
  _gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  // Center in top 160px
  _gfx->setCursor((240 - w) / 2, (160 - h) / 2);
  _gfx->print(text);

  // Draw Back Button (Bottom 80px)
  drawMenuItem(160, 80, "Back", (activeIndex == 0), C_CYBER_PURPLE, G_2);
}

void DisplayManager::drawSuccessScreen(const char *message) {
  _gfx->fillScreen(C_GREEN);

  _gfx->setTextColor(C_BLACK, C_GREEN);
  _gfx->setTextSize(3);

  int16_t x1, y1;
  uint16_t w, h;
  _gfx->getTextBounds(message, 0, 0, &x1, &y1, &w, &h);

  _gfx->setCursor((240 - w) / 2, (240 - h) / 2);
  _gfx->print(message);

  _lastActiveIndex = -100; // Force redraw
}

void DisplayManager::drawErrorScreen(const char *message) {
  _gfx->fillScreen(C_DARK_RED);

  _gfx->setTextColor(C_WHITE, C_DARK_RED);
  _gfx->setTextSize(3);

  int16_t x1, y1;
  uint16_t w, h;
  _gfx->getTextBounds(message, 0, 0, &x1, &y1, &w, &h);

  _gfx->setCursor((240 - w) / 2, (240 - h) / 2);
  _gfx->print(message);

  _lastActiveIndex = -100; // Force redraw
}

void DisplayManager::drawStandbyScreen(int minutes) {
  if (minutes == _lastStandbyMinutes && _lastActiveIndex == -98)
    return;
  _lastStandbyMinutes = minutes;
  _lastActiveIndex = -98;

  // Passive info
  _gfx->fillRect(0, 0, 240, 240, C_BLACK);

  // 1. Title: "Coffee Zone Controller"
  // Cyberpunk Pink, Centered at top
  _gfx->setTextColor(C_CYBER_PINK, C_BLACK);
  _gfx->setTextSize(2); // Smaller to fit width? Or 2 lines?
  // "Coffee Zone"
  // "Controller"

  int16_t x1, y1;
  uint16_t w, h;

  const char *line1 = "COFFEE ZONE";
  const char *line2 = "CONTROLLER";

  _gfx->getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
  _gfx->setCursor((240 - w) / 2, 40);
  _gfx->print(line1);

  _gfx->getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
  _gfx->setCursor((240 - w) / 2, 40 + h + 5);
  _gfx->print(line2);

  // 2. Bus Time Underneath
  _gfx->setTextColor(C_CYBER_BLUE, C_BLACK);
  _gfx->setTextSize(4); // Make it big

  String busText = "";
  if (minutes < 0)
    busText = "--";
  else
    busText = String(minutes) + "m";

  _gfx->getTextBounds(busText, 0, 0, &x1, &y1, &w, &h);
  int busY = 140; // Approx center lower half
  _gfx->setCursor((240 - w) / 2, busY);
  _gfx->print(busText);

  // label for bus
  _gfx->setTextSize(2);
  const char *label = "NEXT BUS";
  _gfx->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
  _gfx->setCursor((240 - w) / 2, busY - h - 5);
  _gfx->setTextColor(G_2, C_BLACK); // Dimmer
  _gfx->print(label);
}

void DisplayManager::drawUnlockVisual(float progress) {
  // Draw a circle that grows
  // Progress 0.0 to 1.0
  int maxRadius = 100;
  int radius = (int)(maxRadius * progress);

  uint16_t color = C_GREEN;

  if (progress < 0.1) {
    // Clear screen logic if just starting?
    // Assuming this is called over Standby screen.
    // We might want to just draw the circle.
  }

  _gfx->fillCircle(120, 120, radius, color);

  // If full, maybe flash white?
}

void DisplayManager::drawTimerScreen(int activeIndex, bool menuVisible) {
  if (!menuVisible) {
    return;
  }
  if (activeIndex == _lastActiveIndex)
    return;

  // Layout: Reset (80-160), Back (160-240)
  drawMenuItem(80, 80, "Reset", (activeIndex == 0), C_NEON_YELLOW, G_1);
  drawMenuItem(160, 80, "Back", (activeIndex == 1), C_CYBER_PURPLE, G_2);

  _lastActiveIndex = activeIndex;
}

void DisplayManager::renderMenuSlider(int offset, const char *item1,
                                      const char *item2) {
  int yStart = 240 - offset;

  // Draw Item 1
  if (yStart < 240) {
    drawMenuItem(yStart, 80, item1, false, C_NEON_YELLOW, G_1);
  }
  // Draw Item 2
  if (yStart + 80 < 240) {
    drawMenuItem(yStart + 80, 80, item2, false, C_CYBER_PURPLE, G_2);
  }
}

void DisplayManager::animateTimerMenu(bool opening) {
  _animating = true;
  _animOpening = opening;
  _animStartTime = millis();

  int timerStart = opening ? 120 : 40;
  int menuStart = opening ? 0 : 160;

  _prevTimerY = timerStart;
  _prevMenuLimit = 240 - menuStart;
}

void DisplayManager::update() {
  if (!_animating)
    return;

  float durationMs = 1000.0;
  unsigned long elapsed = millis() - _animStartTime;
  float progress = (float)elapsed / durationMs;

  if (progress >= 1.0) {
    progress = 1.0;
    _animating = false;
  }

  int timerStart = _animOpening ? 120 : 40;
  int timerEnd = _animOpening ? 40 : 120;
  int menuStart = _animOpening ? 0 : 160;
  int menuEnd = _animOpening ? 160 : 0;

  const char *str = (_lastTimerStr[0] == '\0') ? "00:00" : _lastTimerStr;
  _gfx->setTextSize(5);

  int16_t x1, y1;
  uint16_t w, h;
  _gfx->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  int timerX = (240 - w) / 2;

  // 1. Calculate New Positions
  int currentTimerY = timerStart + (timerEnd - timerStart) * progress;
  int currentMenuOffset = menuStart + (menuEnd - menuStart) * progress;
  int currentMenuTop = 240 - currentMenuOffset;

  // 2. Clear Dirty Rects
  // Timer Trail:
  _gfx->fillRect(timerX - 10, _prevTimerY - (h / 2) - 10, w + 20, h + 20,
                 C_BLACK);

  // Menu Trail:
  if (currentMenuTop > _prevMenuLimit) {
    _gfx->fillRect(0, _prevMenuLimit, 240, currentMenuTop - _prevMenuLimit,
                   C_BLACK);
  }

  // 3. Draw New Timer
  _gfx->setTextColor(C_WHITE, C_BLACK);
  _gfx->setTextSize(5);
  int drawY = currentTimerY - (h / 2);
  _gfx->setCursor(timerX, drawY);
  _gfx->print(str);

  // 4. Draw New Menu
  renderMenuSlider(currentMenuOffset, "Reset", "Back");

  // Update History
  _prevTimerY = currentTimerY;
  _prevMenuLimit = currentMenuTop;

  // Final State Update
  if (!_animating) {
    if (_animOpening) {
      drawTimerScreen(-1, true); // Draws menu buttons
      _lastTimerY = 40;
    } else {
      _lastTimerY = 120;
    }
  }
}

void DisplayManager::drawTimerValue(unsigned long elapsedMs, int yOverride) {
  unsigned long totalSeconds = elapsedMs / 1000;
  unsigned long minutes = totalSeconds / 60;
  unsigned long seconds = totalSeconds % 60;

  char buf[9];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", minutes, seconds);

  int targetY = (yOverride != -1) ? yOverride : 120;

  // Check if changed AND not moving
  bool posChanged = (targetY != _lastTimerY);
  bool textChanged = (strncmp(buf, _lastTimerStr, 8) != 0);

  if (!posChanged && !textChanged) {
    return;
  }

  // Save new
  strncpy(_lastTimerStr, buf, 9);
  _lastTimerY = targetY;

  _gfx->setTextColor(C_WHITE, C_BLACK); // Use BG color to overwrite
  _gfx->setTextSize(5);

  int16_t x1, y1;
  uint16_t w, h;
  _gfx->getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);

  int x = (240 - w) / 2;
  int y = targetY - (h / 2);

  // Clear previous area safely (padding 10)
  _gfx->fillRect(x - 10, y - 10, w + 20, h + 20, C_BLACK);

  _gfx->setCursor(x, y);
  _gfx->print(buf);
}

void DisplayManager::drawLightSettings(int activeIndex) {
  if (activeIndex == _lastActiveIndex)
    return;

  // Rainbow, Coffee, Back
  drawMenuItem(0, 80, "Rainbow", (activeIndex == 0), C_CYBER_PINK, G_1);
  drawMenuItem(80, 80, "Coffee", (activeIndex == 1), C_CYBER_BLUE, G_2);
  drawMenuItem(160, 80, "Back", (activeIndex == 2), C_CYBER_PURPLE, G_3);

  _lastActiveIndex = activeIndex;
}

void DisplayManager::drawBootScreen() {
  bool firstDraw = (_lastActiveIndex != -99);

  if (!firstDraw) {
    return;
  }

  _lastActiveIndex = -99;

  if (firstDraw) {
    _gfx->fillScreen(C_BLACK);

    // Splash Screen: 3 Rotated Rectangles
    // Props: W=30, H=145, Angle=-13 deg
    // Coords (Top-Left): (41, 86), (89, 86), (137, 86)

    int16_t w = 30;
    int16_t h = 145;
    float angle = -13.0;

    // Rect 1 (Shifted +5x to 46)
    fillRotatedRect(46, 86, w, h, angle, C_WHITE);
    // Rect 2 (Shifted +5x to 94)
    fillRotatedRect(94, 86, w, h, angle, C_WHITE);
    // Rect 3 (Shifted +5x to 142)
    fillRotatedRect(142, 86, w, h, angle, C_WHITE);

    // Draw Base (Grey Area) - Optional on splash? User asked for black screen.
    // "black screen with 3 rectangles" -> Maybe no base?
    // But system warms up. Let's keep it clean black as requested.
    // updateBaseZone(false);
  }
}

void DisplayManager::fillRotatedRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                     float angleDeg, uint16_t color) {
  float rad = angleDeg * PI / 180.0;
  float c = cos(rad);
  float s = sin(rad);

  // Pivot at Center
  float cx = x + w / 2.0;
  float cy = y + h / 2.0;

  // 4 Local Corners relative to center
  // TL: -w/2, -h/2
  // TR: w/2, -h/2
  // BR: w/2, h/2
  // BL: -w/2, h/2

  float corners[4][2] = {{-w / 2.0f, -h / 2.0f},
                         {w / 2.0f, -h / 2.0f},
                         {w / 2.0f, h / 2.0f},
                         {-w / 2.0f, h / 2.0f}};

  int16_t px[4], py[4];

  for (int i = 0; i < 4; i++) {
    // Rotate
    float rx = corners[i][0] * c - corners[i][1] * s;
    float ry = corners[i][0] * s + corners[i][1] * c;

    // Translate back
    px[i] = (int16_t)(cx + rx);
    py[i] = (int16_t)(cy + ry);
  }

  // Ensure we have fillTriangle (Arduino_GFX usually has it)
  _gfx->fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], color);
  _gfx->fillTriangle(px[0], py[0], px[2], py[2], px[3], py[3], color);
}
