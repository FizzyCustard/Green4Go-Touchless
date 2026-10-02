#include "Config.h"
#include "DisplayManager.h"
#include "DistanceSensor.h"
#include "StateManager.h"
#include <Arduino.h>
#include <WiFi.h>

DistanceSensor *sensor;
DisplayManager *displayManager;
StateManager *stateManager;

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
  Serial.println("Starting v2 App...");

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

  stateManager = new StateManager(sensor, displayManager);
}

void loop() {
  maintainWiFi();

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
