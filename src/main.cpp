#include "Config.h"
#include "DisplayManager.h"
#include "DistanceSensor.h"
#include "StateManager.h"
#include <Arduino.h>
#include <WiFi.h>

// Pin definitions now in Config.h

DistanceSensor *sensor;
DisplayManager *displayManager;
StateManager *stateManager;

// WiFi reconnection variables
unsigned long lastWiFiCheck = 0;
// WiFi check interval now defined in Config.h

void checkWiFiConnection() {
  if (millis() - lastWiFiCheck < WIFI_CHECK_INTERVAL_MS) {
    return; // Not time to check yet
  }

  lastWiFiCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Attempting reconnection...");
    WiFi.disconnect();
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Wait up to 10 seconds for reconnection
    int timeout = 20;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
      delay(500);
      Serial.print(".");
      timeout--;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Reconnected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nWiFi Reconnection failed. Will retry later.");
    }
  }
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

  // Connect Configured WiFi
  Serial.printf("Connecting to %s...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Wait up to 10s for connection
  int timeout = 20;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(500);
    Serial.print(".");
    timeout--;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection Timeout (continues in background)");
  }

  stateManager = new StateManager(sensor, displayManager);
}

void loop() {
  // Check WiFi connection periodically
  checkWiFiConnection();

  // Update Non-blocking Animations (Every loop)
  displayManager->update();

  // Update Sensor Loop (Fast)
  // Main Loop Timer (50ms = 20Hz)
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < MAIN_LOOP_INTERVAL_MS)
    return;
  lastUpdate = millis();

  // Update Sensor (Now throttled)
  sensor->update();

  stateManager->update();
}
