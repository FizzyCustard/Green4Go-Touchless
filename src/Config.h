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
#define WLED_STATE_MAX_BYTES 8192 // Largest WLED state we'll save and restore

// --- Bus status colours (screen and lights) ---
// Red: <= RED_MAX (too soon to catch) or > ORANGE_MAX (long wait)
// Green: up to GREEN_MAX. Orange: up to ORANGE_MAX.
#define BUS_RED_MAX_MIN 5
#define BUS_GREEN_MAX_MIN 9
#define BUS_ORANGE_MAX_MIN 12

// --- Standby screen title ---
#define TITLE_LINE_1 "COFFEE ZONE"
#define TITLE_LINE_2 "CONTROLLER"

// --- Hardware Pin Definitions ---
// Display pins for ESP32-S3
#define TFT_SCK 39
#define TFT_MOSI 38
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
#define BUS_ERROR_RETRY_MS 15000       // First retry after a failed fetch...
#define BUS_ERROR_MAX_RETRY_MS 300000UL // ...doubling up to 5 min while it keeps failing
#define BUS_SCREEN_REFRESH_MS 30000    // Refresh while the Bus Times screen is open
#define BUS_STALE_MS 180000            // Keep counting down old data this long if a refresh fails
#define BUS_FRESH_MS 60000             // Data younger than this is shown instantly on the Bus screen

// --- Bus status codes (negative values where a time would otherwise be) ---
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
#define TLS_HANDSHAKE_TIMEOUT_S 10    // TfL TLS handshake (library default is 120s)
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
// The rows start a few cm out: the HC-SR04 is unreliable below ~4cm, so
// the top row used to lose most of its band. A hand closer than the start
// still counts as the top row.
#define MIN_INTERACTION_DISTANCE 4.0    // Top row starts here...
#define MAX_INTERACTION_DISTANCE 22.0   // ...bottom row ends here (3 x 6cm)
#define BASE_ACTIVE_MIN_DISTANCE 22.0   // Min distance for base zone active
#define BASE_ACTIVE_MAX_DISTANCE 32.0   // Max distance for base zone active
#define UNLOCK_HYSTERESIS_CM 2.0        // Hysteresis for unlock cancel
#define ZONE_HYSTERESIS_CM 1.25         // Sticky band around menu zone edges
#define DWELL_GRACE_MS 250              // Slip off a row and back this fast: dwell resumes
#define REARM_DISTANCE_CM 2.0           // Movement needed to re-arm after a selection
#define TIMER_MENU_CLOSE_DISTANCE 37.0  // Hand beyond this closes the timer menu
#define SENSOR_FAR_CM 100.0             // Value used when no echo comes back
#define SENSOR_ECHO_TIMEOUT_US 12000    // ~2m max range
// One-euro filter: smooths hard while the hand is still (steady cursor
// during a dwell) and lightly while it moves (cursor keeps up)
#define SENSOR_MIN_CUTOFF_HZ 1.0        // Smoothing when still (lower = steadier)
#define SENSOR_BETA 0.2                 // How fast smoothing eases off with speed (per cm/s)
#define SENSOR_DCUTOFF_HZ 2.0           // Smoothing of the speed estimate
#define SPEED_OF_SOUND_CM_PER_US 0.034
#define MOVE_THRESHOLD_CM 1.0           // Change that counts as the hand moving
#define STATIC_OBJECT_MS 60000UL        // Still this long = an object, not a hand
#define SENSOR_MAX_MISSES 3             // Missed echoes bridged before reading "far" (150ms)

// --- Validation Constants ---
#define MAX_LINE_NAME_LENGTH 20         // Max bus line name length
#define MAX_BUS_TIME_SECONDS 7200       // Max reasonable bus time (2 hours)
#define MAX_IP_LENGTH 64                // Max IP address string length
#define MAX_URL_LENGTH 128              // Max URL string length
#define MAX_JSON_ITERATIONS 200         // Max arrivals parsed (busy stops return 50+)
#define MIN_DISTANCE_CM 0.2             // Min valid sensor distance
#define MAX_DISTANCE_CM 400.0           // Max valid sensor distance
#define MAX_ESP32_PIN 48                // Max valid ESP32 pin number

// --- Leave Now Alert ---
// Pulses the lights green while a bus is between LEAVE_WINDOW_MAX_MIN and
// LEAVE_WINDOW_MIN_MIN away (both inclusive): the time to head out for it.
// Only active during the hours below (local time).
#define LEAVE_WINDOW_MAX_MIN 8 // Alert starts with the bus 8:00 away...
#define LEAVE_WINDOW_MIN_MIN 6 // ...and ends after 6:00 (5 min walk + buffer)
#define LEAVE_ALERT_DAYS 0b0111110 // Bit per weekday, bit 0 = Sunday (Mon-Fri)
#define LEAVE_ALERT_START_HHMM 700 // 07:00
#define LEAVE_ALERT_END_HHMM 930   // 09:30
#define LEAVE_REFRESH_MS 20000     // Refresh this often when a bus is getting close
#define LEAVE_REFRESH_LEAD_MIN 4   // ...i.e. within the window plus this lead
#define LEAVE_PULSE_MS 3000        // The green pulse plays this long, then the lights go back
#define TIMEZONE "GMT0BST,M3.5.0/1,M10.5.0" // UK time incl. BST

// --- Bus status on the coffee light ---
// Whenever the lights are coffee white (any day, any time), the first few
// LEDs show the next bus you can still catch: red while it's a long way off,
// fading to orange as it gets close, green inside the leave window, then red
// again once it's gone. The rest of the strip stays white. While coffee white
// is on, this replaces the green leave-now pulse.
#define STATUS_LEDS 3            // LEDs at the start of the strip (LED 0 up)
#define STATUS_BRIGHTNESS 100    // % of full colour, independent of the white
#define STATUS_RED_MIN 15        // Fully red at or beyond this many minutes...
#define STATUS_ORANGE_MIN 10     // ...orange here, green from LEAVE_WINDOW_MAX_MIN
#define STATUS_TICK_MS 10000     // Check the lights and update the colour this often
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"
#define NTP_VALID_EPOCH 1700000000L // Clock before this = NTP not synced yet

// --- Display Sleep ---
#define DISPLAY_SLEEP_MS 900000UL  // Backlight off after 15 min idle in standby
#define BACKLIGHT_FADE_MS 400      // Fade time for sleep/wake

// --- OTA Updates ---
// Set OTA_PASSWORD in secrets.h to enable over-the-air updates
#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "green4go" // Reachable as green4go.local
#endif
#define OTA_WEB_PORT 80     // HTTP upload: POST /update
#define OTA_WEB_USER "admin" // ...with this user and OTA_PASSWORD

// --- Network task ---
#define NET_QUEUE_LENGTH 10
#define NET_TASK_STACK_BYTES 16384

// --- Health monitor ---
// Logs heap/stack once a minute and restarts cleanly (only while idle in
// standby) if the device gets into a state it can't recover from on its own
#define HEALTH_CHECK_INTERVAL_MS 60000UL
#define HEALTH_MIN_HEAP_BLOCK 24000     // TLS needs ~17KB contiguous blocks
#define HEALTH_LOW_HEAP_RESTART_MS 300000UL // Low that long -> restart
#define HEALTH_NO_DATA_RESTART_MS 7200000UL // No good TfL data for 2h on WiFi
#define HEALTH_NET_STUCK_MS 120000UL    // One network command running this long

// --- Debug ---
#define DEBUG_SENSOR 0 // 1 = print raw/smoothed distance every tick
// Raw/smoothed distance every tick to whoever connects to this TCP port,
// e.g. `nc green4go.local 2323`. 0 = off.
#define SENSOR_STREAM_PORT 2323

#endif
