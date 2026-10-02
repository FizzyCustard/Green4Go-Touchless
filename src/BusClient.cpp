#include "BusClient.h"
#include "Config.h"

BusClient::BusClient() {
  // Constructor
}

void BusClient::begin() {
  // For ESP32 Secure Client
  _secureClient.setInsecure(); // Skip certificate validation for simplicity
}

int BusClient::getNextBusTime() {
  if (WiFi.status() != WL_CONNECTED) {
    // Serial.println("[Bus] WiFi not connected");
    return -1;
  }

  HTTPClient http;
  char url[512]; // Increased buffer size for safety

  // Validate TFL_STOP_ID length to prevent buffer overflow
  if (strlen(TFL_STOP_ID) >
      MAX_LINE_NAME_LENGTH * 2) { // Allow longer for stop IDs
    Serial.println("[Bus] TFL_STOP_ID too long");
    return -2;
  }

  // Safe URL construction with bounds checking
  int result =
      snprintf(url, sizeof(url), "https://api.tfl.gov.uk/StopPoint/%s/Arrivals",
               TFL_STOP_ID);
  if (result >= (int)sizeof(url)) {
    Serial.println("[Bus] URL buffer overflow prevented");
    return -2;
  }

  // Serial.printf("[Bus] Fetching data from %s\r\n", url);

  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(_secureClient, url);
  int httpCode = http.GET();

  // API Error defaults to -2, re-use existing result struct or rename
  int apiResult = -2;

  if (httpCode == HTTP_CODE_OK) {
    Stream &payloadStream = http.getStream();
    apiResult = parseNextArrival(payloadStream);
  } else {
    Serial.printf("[Bus] HTTP failed: %s (%d)\n",
                  http.errorToString(httpCode).c_str(), httpCode);
  }

  http.end();
  return apiResult;
}

int BusClient::parseNextArrival(Stream &stream) {
  int minSeconds = -1;
  String filterLines = String(TFL_FILTER_LINES);

  // Safety: Limit parsing iterations to prevent infinite loops
  const int MAX_ITERATIONS = MAX_JSON_ITERATIONS;
  int iterations = 0;

  // Stream parsing logic ported from TflClient
  // We look for objects in the array.
  // Patterns: "lineName":"X", "timeToStation":Y

  while (stream.available() && iterations < MAX_ITERATIONS) {
    iterations++;

    if (stream.find("\"lineName\":")) {
      // Found lineName key
      if (stream.find("\"")) { // Skip opening quote
        String lineName = stream.readStringUntil('\"');

        // Validate line name length (prevent excessive memory usage)
        if (lineName.length() > MAX_LINE_NAME_LENGTH) {
          continue; // Skip invalid entries
        }

        // Find timeToStation in the same object
        if (stream.find("\"timeToStation\":")) {
          int seconds = stream.parseInt();

          // Validate reasonable time bounds (0 to 2 hours)
          if (seconds < 0 || seconds > MAX_BUS_TIME_SECONDS) {
            continue; // Skip invalid times
          }

          // Simple check: Filter
          bool match = false;
          if (filterLines.length() == 0 ||
              filterLines.indexOf(lineName) != -1) {
            match = true;
          }

          if (match) {
            // Serial.printf("[Bus] Line %s in %d sec\n", lineName.c_str(),
            // seconds);
            if (minSeconds == -1 || seconds < minSeconds) {
              minSeconds = seconds;
            }
          }
        }
      }
    } else {
      break;
    }
  }

  if (minSeconds != -1) {
    return minSeconds / 60; // Return minutes
  }

  return -3; // No valid data found
}
