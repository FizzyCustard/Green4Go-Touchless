#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Colors
#define C_DARK_RED 0x8000
#define C_LIGHT_RED 0xF800
#define C_ORANGE 0xFD20
#define C_GREEN 0x07E0

#define C_WHITE 0xFFFF
#define C_BLACK 0x0000

// Cyberpunk Palette
#define C_CYBER_PINK 0xFB56   // Hot Pink
#define C_CYBER_BLUE 0x06FF   // Cyan/Electric Blue
#define C_CYBER_PURPLE 0x901F // Violet
#define C_NEON_YELLOW 0xFFE0  // Bright Yellow

// Greys
#define G_1 0x2104          // Menu Item 1 inactive
#define G_2 0x3186          // Menu Item 2 inactive
#define G_3 0x4208          // Menu Item 3 inactive
#define G_TEXT_DIM 0x8410   // Secondary labels
#define G_BASE_DARK 0x2104  // Dark grey for base
#define G_BASE_LIGHT 0x630C // Light grey for base (active)

// Layout: a 240x240 content area made of three 80px rows (one per 6cm
// distance zone, 4-22cm), plus an 80px "base" strip underneath for 22-32cm.
#define SCREEN_W 240
#define CONTENT_H 240
#define ROW_H 80
#define NUM_ROWS 3
#define BASE_Y 240
#define BASE_H 80
#define GUTTER_W 8 // Right-hand strip for the hand cursor on menu screens

// Everything the screens need to know about the next bus.
struct BusView {
  int minutes;      // >= 0 valid, otherwise a BUS_* status code
  const char *line; // Route of the soonest bus ("" if unknown)
  bool refreshing;  // A fetch is in flight
  bool online;      // WiFi connected
  int secondMinutes; // The bus after, or -1
  bool leaveNow;     // A bus is in the leave-now window
  int alertMinutes;  // Minutes to the bus we're alerting for
};

class DisplayManager {
public:
  // Lives for the whole run, so there is no destructor (Arduino_DataBus has
  // no virtual destructor, so deleting _bus would be undefined behaviour)
  DisplayManager(int sck, int mosi, int dc, int cs, int rst, int bl);
  void begin();

  // Call every loop: runs animations (frame-capped internally)
  void update();
  bool isAnimating() const { return _animating; }

  // Screens. Each one redraws fully the first time it is shown and only
  // repaints what changed after that, so they are safe to call every tick.
  void drawBootScreen();
  void drawStandbyScreen(const BusView &bus);
  void drawUnlockVisual(float progress); // 0.0 to 1.0
  void drawMainMenu(int activeRow);      // 0=Timer, 1=Bus Times, 2=Lights
  void drawLightSettings(int activeRow); // 0=Rainbow, 1=Coffee, 2=Back
  void drawTimerScreen(int activeRow, bool menuOpen); // Rows 1=Reset, 2=Back
  void drawTimerValue(unsigned long elapsedMs);
  void drawBusScreen(const BusView &bus, int activeRow); // 1=Refresh, 2=Back
  void drawSuccessScreen(const char *message);
  void drawErrorScreen(const char *message);

  // Dwell feedback: a bar that fills along the bottom of the hovered row
  void drawDwellProgress(int row, float progress);

  // Base zone (22-32cm feedback strip)
  void updateBaseZone(bool active);

  // Hand-position marker in the right-hand gutter (menu screens only).
  // Pass a negative distance to hide it.
  void setHandCursor(float dist);

  // Backlight: fades to off/on
  void setAwake(bool awake);
  bool isAwake() const { return _blTarget > 0; }

  // Over-the-air update screen
  void drawOtaProgress(unsigned int percent);
  void drawOtaMessage(const char *message, uint16_t color);

private:
  enum Screen {
    SCR_NONE,
    SCR_BOOT,
    SCR_STANDBY,
    SCR_UNLOCK,
    SCR_MAIN,
    SCR_LIGHTS,
    SCR_TIMER,
    SCR_BUS,
    SCR_MESSAGE,
    SCR_OTA
  };

  Arduino_DataBus *_bus;
  Arduino_GFX *_gfx;
  int _blPin;

  Screen _screen = SCR_NONE;
  int _paintW = SCREEN_W; // Width content paints to (less the cursor gutter)

  // Hand cursor (y in screen pixels, -1 = hidden)
  float _cursorTarget = -1;
  float _cursorPos = -1;
  int _cursorDrawnY = -1;

  // Backlight fade
  int _blLevel = 0;
  int _blTarget = 0;
  int _blFrom = 0;
  unsigned long _blFadeStart = 0;

  int _otaW = 0;
  int _activeRow = -10;
  uint16_t _rowColor[NUM_ROWS] = {0, 0, 0}; // Highlight colour per row

  // Dwell bar
  int _dwellRow = -1;
  int _dwellW = 0;

  // Base strip: -1 unknown (must repaint), 0 idle, 1 hand detected
  int _baseState = -1;

  // Standby
  char _sbLabel[20] = "";
  char _sbBus[20] = "";
  char _sbStatus[20] = "";
  uint16_t _sbBusColor = 0;
  uint16_t _breathColor = 0;
  unsigned long _lastFrame = 0;

  // Unlock ring
  float _unlockAngle = 0;

  // Bus screen
  char _busMain[20] = "";
  char _busSub[24] = "";
  uint16_t _busBg = 0;

  // Timer + menu slide animation. "Openness" goes 0 (menu hidden, timer
  // centred) to 1 (menu fully up, timer at the top).
  char _timerStr[12] = "00:00";
  bool _menuTarget = false;
  float _openness = 0;
  float _animFrom = 0;
  bool _animating = false;
  unsigned long _animStart = 0;
  int _drawnTimerY = -1;
  int _drawnMenuTop = CONTENT_H;

  bool enterScreen(Screen s);
  static uint16_t contrastColor(uint16_t bg);
  static uint8_t fitTextSize(size_t len, uint8_t maxSize, int maxWidth);

  void fillRectClipped(int x, int y, int w, int h, uint16_t color, int clipTop,
                       int clipBottom);
  void drawTextInBand(const char *text, uint8_t size, int bandY, int bandH,
                      uint16_t fg, uint16_t bg, int clipBottom = CONTENT_H);
  void drawRow(int row, const char *label, bool active, uint16_t activeColor,
               uint16_t inactiveColor);
  void drawMenuRows(const char *const labels[NUM_ROWS],
                    const uint16_t on[NUM_ROWS], const uint16_t off[NUM_ROWS],
                    int activeRow);
  void drawStandbyHint(uint16_t color);
  void drawCursorAt(int y);
  void renderTimerFrame(float openness);
  int timerCentreY(float openness) const;
  int menuTop(float openness) const;
  void fillRotatedRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       float angleDeg, uint16_t color);
};

#endif
