#ifndef SECRETS_H
#define SECRETS_H

// Personal settings template.
// Copy this file to src/secrets.h and fill in your own details.
// src/secrets.h is gitignored so your details stay out of the repo.

// WiFi Credentials
#define WIFI_SSID "your-wifi-name"
#define WIFI_PASS "your-wifi-password"

// --- TfL Bus API ---
// Find your stop ID at https://tfl.gov.uk/ (e.g. "490000000X")
#define TFL_STOP_ID "YOUR_STOP_ID"
// Comma separated list of lines to filter, e.g. "36,185", or "" for all.
#define TFL_FILTER_LINES ""
// Optional: a free TfL API key (https://api-portal.tfl.gov.uk) gives a much
// higher rate limit than anonymous use. Not needed for one device.
// #define TFL_APP_KEY "your-tfl-app-key"

// --- WLED Integration ---
// IP address of your WLED instance
#define WLED_IP "192.168.1.100"

// --- OTA Updates (optional) ---
// Uncomment and set a long random password (12+ characters) to enable
// over-the-air uploads. Anyone on your network with this password can flash
// the device. Left commented out, OTA is disabled.
// #define OTA_PASSWORD "use-a-long-random-password"

#endif // SECRETS_H
