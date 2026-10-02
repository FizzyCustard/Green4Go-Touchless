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
#define TIMER_AWAKE_MAX_MS 3600000UL  // A running timer keeps the screen awake for up to 1 hour
#define BUS_UPDATE_INTERVAL_MS 60000   // Standby refresh when the last fetch worked
#define BUS_ERROR_RETRY_MS 15000       // Standby refresh when the last fetch failed
#define BUS_SCREEN_REFRESH_MS 30000    // Refresh while the Bus Times screen is open
#define BUS_STALE_MS 180000            // Keep counting down old data this long if a refresh fails
#define BUS_FRESH_MS 60000             // Data younger than this is shown instantly on the Bus screen

// --- Bus status codes (negative "minutes" values) ---
#define BUS_LOADING -1   // No data yet / fetch in flight
#define BUS_API_ERROR -2 // HTTP or parse failure
#define BUS_NO_BUSES -3  // API worked but no matching arrivals
#define BUS_OFFLINE -4   // WiFi not connected

// --- Timing Constants ---
#define DWELL_THRESHOLD_MS 1000       // Time to dwell for selection
#define SUCCESS_DISPLAY_TIME_MS 1000  // How long to show success screen
#define ERROR_DISPLAY_TIME_MS 3000    // How long to show error screen
#define BUS_EFFECT_TIMEOUT_MS 10000   // WLED bus effect timeout
#define WIFI_CHECK_INTERVAL_MS 30000  // WiFi reconnection check interval
#define HTTP_TIMEOUT_MS 3000          // HTTP read timeout
#define HTTP_CONNECT_TIMEOUT_MS 4000  // TfL connect timeout (TLS over the internet)
#define BUS_PARSE_DEADLINE_MS 8000    // Hard cap on time spent parsing one response
#define WLED_TIMEOUT_MS 1000          // WLED is on the LAN, so fail fast
#define WLED_BACKOFF_MS 30000         // Skip WLED calls for this long after a failure
#define UNLOCK_TIME_MS 1000           // Time required to unlock
#define MAIN_LOOP_INTERVAL_MS 50      // Sensor + state machine tick (20Hz)
#define FRAME_INTERVAL_MS 16          // Animation frame cap (~60fps)
#define TIMER_MENU_ANIM_MS 300        // Timer menu slide duration
#define SPLASH_MIN_MS 2000            // Splash stays up at least this long
#define SPLASH_MAX_MS 10000           // ...and never longer than this
#define BUS_BOOT_RETRY_MS 5000        // Splash retry interval for the first fetch

// --- Distance/Zone Constants ---
#define MAX_INTERACTION_DISTANCE 15.0   // Maximum distance for interaction
#define BASE_ACTIVE_MIN_DISTANCE 15.0   // Min distance for base zone active
#define BASE_ACTIVE_MAX_DISTANCE 25.0   // Max distance for base zone active
#define UNLOCK_HYSTERESIS_CM 2.0        // Hysteresis for unlock cancel
#define ZONE_HYSTERESIS_CM 0.5          // Sticky band around menu zone edges
#define TIMER_MENU_CLOSE_DISTANCE 30.0  // Hand beyond this closes the timer menu
#define SENSOR_FAR_CM 100.0             // Value used when no echo comes back
#define SENSOR_ECHO_TIMEOUT_US 12000    // ~2m max range; was 30ms (5m)

// --- Validation Constants ---
#define MAX_LINE_NAME_LENGTH 20         // Max bus line name length
#define MAX_BUS_TIME_SECONDS 7200       // Max reasonable bus time (2 hours)
#define MAX_IP_LENGTH 64                // Max IP address string length
#define MAX_URL_LENGTH 128              // Max URL string length
#define MAX_JSON_ITERATIONS 200         // Max arrivals parsed (busy stops return 50+)
#define MIN_DISTANCE_CM 0.2             // Min valid sensor distance
#define MAX_DISTANCE_CM 400.0           // Max valid sensor distance
#define MAX_ESP32_PIN 48                // Max valid ESP32 pin number

// --- Debug ---
#define DEBUG_SENSOR 0 // 1 = print raw/smoothed distance every tick

#endif
