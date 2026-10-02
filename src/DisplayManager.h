#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Colors
#define C_DARK_RED 0x8000
#define C_LIGHT_RED 0xF800
#define C_ORANGE 0xFD20
#define C_YELLOW 0xFFE0
#define C_GREEN 0x07E0
#define C_PINK 0xF81F

#define C_WHITE 0xFFFF
#define C_BLACK 0x0000

// Cyberpunk Palette
#define C_CYBER_PINK 0xFB56   // Hot Pink
#define C_CYBER_BLUE 0x06FF   // Cyan/Electric Blue
#define C_CYBER_PURPLE 0x901F // Violet
#define C_NEON_YELLOW 0xFFE0  // Bright Yellow

// Greys
#define G_0 0x1082
#define G_1 0x2104          // Menu Item 1 inactive
#define G_2 0x3186          // Menu Item 2 inactive
#define G_3 0x4208          // Menu Item 3 inactive
#define G_BASE_DARK 0x2104  // Dark grey for base
#define G_BASE_LIGHT 0x630C // Light grey for base (active)

class DisplayManager {
public:
  DisplayManager(int sck, int mosi, int dc, int cs, int rst, int bl);
  ~DisplayManager();
  void begin();

  // Core Rendering
  void update();                      // Called every loop for animations
  void drawMainMenu(int activeIndex); // 0=Top, 1=Middle, 2=Bottom
  void clearContentArea();
  void drawTimerScreen(int activeIndex, bool menuVisible);
  void drawTimerValue(unsigned long elapsedMs, int yOverride = -1);
  void animateTimerMenu(bool opening);
  void drawLightSettings(int activeIndex); // 0=Rainbow, 1=Coffee, 2=Back

  // New Screens
  void drawBootScreen();
  void drawBusScreen(int minutes, int activeIndex);
  void drawStandbyScreen(int minutes);
  void drawUnlockVisual(float progress); // 0.0 to 1.0
  void drawSuccessScreen(const char *message);
  void drawErrorScreen(const char *message);

  // Base Zone
  void updateBaseZone(bool active); // active if distance < 25cm
  void renderMenuSlider(int offset, const char *item1, const char *item2);

  Arduino_GFX *getGfx() { return _gfx; }

private:
  Arduino_DataBus *_bus;
  Arduino_GFX *_gfx;
  int _blPin;

  int _lastActiveIndex = -10;
  bool _lastBaseActive = false;
  char _lastTimerStr[9]; // MM:SS + null
  int _lastTimerY = -1;

  int _lastBusMinutes = -100;
  int _lastStandbyMinutes = -100;

  // Animation State
  bool _animating = false;
  unsigned long _animStartTime = 0;
  bool _animOpening = false;
  int _prevTimerY = 0;
  int _prevMenuLimit = 0;
  float _lastBootDist = -1.0;

  // Helpers
  void drawMenuItem(int y, int h, const char *label, bool active,
                    uint16_t activeColor, uint16_t inactiveColor);
  void fillRotatedRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       float angleDeg, uint16_t color);
};

#endif
