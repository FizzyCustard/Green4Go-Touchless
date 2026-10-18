#include "Config.h"
#include "DisplayManager.h"
#include "DistanceSensor.h"
#include "StateManager.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <lwip/sockets.h>
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

// The same update pushed over HTTP, so every connection starts from the
// sender (espota needs the device to connect back, which firewalls block):
//   curl --digest -u admin:PASSWORD --data-binary @firmware.bin
//        -H "Content-Type: application/octet-stream"
//        http://green4go.local/update
// Digest auth, so the password never crosses the network in the clear.
// The body is the raw .bin: the library hands a raw body and a multipart
// form to the same callback through different structs (reading the wrong
// one crashes), so forms are refused outright.
static WebServer otaWeb(OTA_WEB_PORT);
static bool webUploadAuthed = false;

static bool isFormUpload() {
  return otaWeb.header("Content-Type").startsWith("multipart/");
}

void setupWebOta() {
  const char *headers[] = {"Content-Type"};
  otaWeb.collectHeaders(headers, 1);
  otaWeb.on(
      "/update", HTTP_POST,
      // Runs once the whole request has been received
      []() {
        if (isFormUpload()) {
          otaWeb.send(415, "text/plain",
                      "Send the raw .bin with --data-binary\n");
          return;
        }
        if (!webUploadAuthed) {
          otaWeb.requestAuthentication(DIGEST_AUTH, OTA_HOSTNAME);
          return;
        }
        webUploadAuthed = false;
        if (Update.hasError() || !Update.isFinished()) {
          otaWeb.send(500, "text/plain",
                      String("Update failed: ") + Update.errorString() + "\n");
          if (otaInProgress) {
            otaInProgress = false;
            displayManager->drawOtaMessage("Update failed", C_LIGHT_RED);
            delay(2000);
          }
          return;
        }
        otaWeb.send(200, "text/plain", "OK, restarting\n");
        Serial.println("OTA (HTTP): done, rebooting");
        otaInProgress = false;
        displayManager->drawOtaProgress(100);
        displayManager->drawOtaMessage("Restarting...", C_GREEN);
        delay(500); // Let the response reach the sender
        ESP.restart();
      },
      // Runs for each chunk of the body
      []() {
        if (isFormUpload())
          return; // Only otaWeb.upload() is valid here, and we don't use it
        HTTPRaw &raw = otaWeb.raw();
        if (raw.status == RAW_START) {
          // Headers are in by now: refuse to touch flash without the password
          webUploadAuthed = otaWeb.authenticate(OTA_WEB_USER, OTA_PASSWORD);
          // curl's first digest request is empty, just to get a challenge
          if (!webUploadAuthed || otaWeb.clientContentLength() <= 0)
            return;
          Serial.println("OTA (HTTP): starting");
          otaInProgress = true;
          displayManager->drawOtaProgress(0);
          Update.begin(otaWeb.clientContentLength());
        } else if (!webUploadAuthed || !otaInProgress || Update.hasError()) {
          return;
        } else if (raw.status == RAW_WRITE) {
          Update.write(raw.buf, raw.currentSize);
          displayManager->drawOtaProgress((unsigned int)(
              (uint64_t)raw.totalSize * 100 / otaWeb.clientContentLength()));
        } else if (raw.status == RAW_END) {
          Update.end(); // Checks the image before it can boot
        } else if (raw.status == RAW_ABORTED) {
          // The sender went away; no reply handler runs after this
          Update.abort();
          webUploadAuthed = false;
          otaInProgress = false;
          Serial.println("OTA (HTTP): upload cut off");
          displayManager->drawOtaMessage("Update failed", C_LIGHT_RED);
          delay(2000);
        }
      });
  otaWeb.begin();
}

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
  setupWebOta();
  otaReady = true;
  Serial.printf("OTA ready at %s.local\n", OTA_HOSTNAME);
}
#endif

#if SENSOR_STREAM_PORT
// Streams every sensor tick to one TCP client for tuning, e.g.
// `nc green4go.local 2323`. Lines are "millis raw smoothed" (raw -1 = miss).
// Read-only, and costs nothing while nobody is connected.
static WiFiServer streamServer(SENSOR_STREAM_PORT);
static WiFiClient streamClient;
static bool streamStarted = false;

void streamSensor() {
  if (!streamStarted)
    return;
  if (streamServer.hasClient()) {
    if (streamClient)
      streamClient.stop(); // Newest connection wins
    streamClient = streamServer.accept();
    streamClient.setNoDelay(true);
    streamClient.println("# millis raw_cm smoothed_cm");
  }
  if (!streamClient || !streamClient.connected())
    return;
  // Never block the UI on a slow reader: a non-blocking send drops the line
  // if the socket buffer is full (WiFiClient::write would wait, and its
  // availableForWrite() always reports 0)
  char line[40];
  int n = snprintf(line, sizeof(line), "%lu %.1f %.1f\n", millis(),
                   sensor->getRawDistance(), sensor->getSmoothedDistance());
  send(streamClient.fd(), line, n, MSG_DONTWAIT);
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
#if SENSOR_STREAM_PORT
      if (!streamStarted) {
        streamServer.begin();
        streamStarted = true;
        Serial.printf("Sensor stream on port %d\n", SENSOR_STREAM_PORT);
      }
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
  if (otaReady) {
    ArduinoOTA.handle();
    otaWeb.handleClient();
  }
#endif

  // Sensor + state machine at a steady 20Hz (the HC-SR04 needs ~50ms
  // between pings so echoes don't overlap)
  static unsigned long lastTick = 0;
  if (millis() - lastTick >= MAIN_LOOP_INTERVAL_MS) {
    lastTick = millis();
    sensor->update();
#if SENSOR_STREAM_PORT
    streamSensor();
#endif
    stateManager->update();
  }

  // Animations run at their own frame rate between ticks
  displayManager->update();

  delay(1); // Yield instead of spinning flat out
}
