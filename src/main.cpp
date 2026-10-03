#include "Config.h"
#include "DisplayManager.h"
#include "DistanceSensor.h"
#include "StateManager.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <time.h>

DistanceSensor *sensor;
DisplayManager *displayManager;
StateManager *stateManager;

#ifdef OTA_PASSWORD
// Over-the-air updates: `pio run -e esp32s3_ota -t upload` once the device
// is on WiFi. The screen shows progress; the update blocks the loop while it
// runs, then the device reboots into the new firmware.

// Anyone on the LAN who knows the password can flash this device, so refuse
// to build with the template value or a short one
constexpr bool strEq(const char *a, const char *b) {
  return *a == *b && (*a == '\0' || strEq(a + 1, b + 1));
}
static_assert(!strEq(OTA_PASSWORD, "change-me") &&
                  !strEq(OTA_PASSWORD, "use-a-long-random-password"),
              "Set your own OTA_PASSWORD in secrets.h (or remove it to "
              "disable OTA)");
static_assert(sizeof(OTA_PASSWORD) > 12,
              "OTA_PASSWORD must be at least 12 characters");

static bool otaReady = false;
static bool otaInProgress = false;

void setupOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    Serial.println("OTA: starting");
    otaInProgress = true;
    displayManager->drawOtaProgress(0);
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    displayManager->drawOtaProgress(
        total ? (unsigned int)((uint64_t)done * 100 / total) : 0);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA: done, rebooting");
    otaInProgress = false;
    displayManager->drawOtaProgress(100);
    displayManager->drawOtaMessage("Restarting...", C_GREEN);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA: error %u\n", error);
    // Only an upload that actually started takes over the screen. A bad
    // password attempt (anyone on the LAN can send one) is just logged, and
    // the library can report one failure twice, so show it once.
    if (!otaInProgress)
      return;
    otaInProgress = false;
    displayManager->drawOtaMessage("Update failed", C_LIGHT_RED);
    delay(2000); // Leave the message up before the UI takes over again
  });
  ArduinoOTA.begin();
  otaReady = true;
  Serial.printf("OTA ready at %s.local\n", OTA_HOSTNAME);
}
#endif

// Non-blocking WiFi upkeep. The ESP32 auto-reconnects on its own; this logs
// state changes and nudges a reconnect every WIFI_CHECK_INTERVAL_MS as a
// safety net. Never waits, so the UI keeps running while WiFi is down.
void maintainWiFi() {
  static bool wasConnected = false;
  static unsigned long lastCheck = 0;

  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected != wasConnected) {
    wasConnected = connected;
    if (connected) {
      Serial.print("WiFi Connected! IP Address: ");
      Serial.println(WiFi.localIP());
#ifdef OTA_PASSWORD
      if (!otaReady)
        setupOta();
#endif
    } else {
      Serial.println("WiFi disconnected");
    }
  }

  if (connected || millis() - lastCheck < WIFI_CHECK_INTERVAL_MS)
    return;
  lastCheck = millis();
  Serial.println("WiFi still down, retrying in background...");
  WiFi.reconnect();
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Green4Go starting (reset reason %d)\n", (int)esp_reset_reason());

  // Init Components
  sensor = new DistanceSensor(US_TRIG, US_ECHO);
  displayManager =
      new DisplayManager(TFT_SCK, TFT_MOSI, TFT_DC, TFT_CS, TFT_RST, TFT_BL);

  sensor->begin();
  displayManager->begin();
  displayManager->drawBootScreen(); // Show splash immediately

  // Connect in the background; the splash waits for data (up to
  // SPLASH_MAX_MS) instead of setup() blocking here
  Serial.printf("Connecting to %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Local time for the leave-now alert hours. Syncs once WiFi is up.
  configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);

#ifndef OTA_PASSWORD
  Serial.println("OTA disabled: set OTA_PASSWORD in secrets.h to enable");
#endif

  stateManager = new StateManager(sensor, displayManager);
}

void loop() {
  maintainWiFi();
#ifdef OTA_PASSWORD
  if (otaReady)
    ArduinoOTA.handle();
#endif

  // Sensor + state machine at a steady 20Hz (the HC-SR04 needs ~50ms
  // between pings so echoes don't overlap)
  static unsigned long lastTick = 0;
  if (millis() - lastTick >= MAIN_LOOP_INTERVAL_MS) {
    lastTick = millis();
    sensor->update();
    stateManager->update();
  }

  // Animations run at their own frame rate between ticks
  displayManager->update();

  delay(1); // Yield instead of spinning flat out
}
