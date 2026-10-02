#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Personal Settings ---
// WiFi, TfL stop and WLED IP live in secrets.h (gitignored).
// Copy secrets.example.h to secrets.h and fill in your details.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing src/secrets.h - copy src/secrets.example.h to src/secrets.h and fill in your details"
#endif

// --- TfL Bus API ---
// API URL Template
// Returns JSON array of arrival predictions
#define TFL_API_URL_TEMPLATE "https://api.tfl.gov.uk/StopPoint/%s/Arrivals"

// --- WLED Integration ---
#define WLED_DEFAULT_PRESET 1
#define WLED_RAINBOW_PRESET 2 // Example preset ID for rainbow
#define WLED_SOLID_PRESET 3   // Example preset ID for solid

// --- Hardware Pin Definitions ---
// Display pins for ESP32-S3
#define TFT_SCK 39
#define TFT_MOSI 38
#define TFT_MISO GFX_NOT_DEFINED
#define TFT_DC 42
#define TFT_CS 45
#define TFT_RST 0
#define TFT_BL 1

// Ultrasonic sensor pins
#define US_TRIG 17
#define US_ECHO 18

// --- Interaction ---
#define UNLOCK_DISTANCE_CM 3
#define STANDBY_TIMEOUT_MS 30000
#define BUS_UPDATE_INTERVAL_MS 60000

// --- Timing Constants ---
#define DWELL_THRESHOLD_MS 1000       // Time to dwell for selection
#define SUCCESS_DISPLAY_TIME_MS 1000  // How long to show success screen
#define ERROR_DISPLAY_TIME_MS 3000    // How long to show error screen
#define IGNORE_INPUT_TIME_MS 400      // Ignore input after selection
#define BUS_EFFECT_TIMEOUT_MS 10000   // WLED bus effect timeout
#define WIFI_CHECK_INTERVAL_MS 30000  // WiFi reconnection check interval
#define HTTP_TIMEOUT_MS 3000          // HTTP request timeout
#define UNLOCK_TIME_MS 1000           // Time required to unlock
#define MAIN_LOOP_INTERVAL_MS 50      // Main loop timing (20Hz)

// --- Distance/Zone Constants ---
#define MAX_INTERACTION_DISTANCE 15.0   // Maximum distance for interaction
#define BASE_ACTIVE_MIN_DISTANCE 15.0   // Min distance for base zone active
#define BASE_ACTIVE_MAX_DISTANCE 25.0   // Max distance for base zone active
#define UNLOCK_HYSTERESIS_CM 2.0        // Hysteresis for unlock cancel

// --- Validation Constants ---
#define MAX_LINE_NAME_LENGTH 20         // Max bus line name length
#define MAX_BUS_TIME_SECONDS 7200       // Max reasonable bus time (2 hours)
#define MAX_IP_LENGTH 64                // Max IP address string length
#define MAX_URL_LENGTH 128              // Max URL string length
#define MAX_JSON_ITERATIONS 50          // Max parsing iterations
#define MIN_DISTANCE_CM 0.2             // Min valid sensor distance
#define MAX_DISTANCE_CM 400.0           // Max valid sensor distance
#define MAX_ESP32_PIN 48                // Max valid ESP32 pin number

#endif
