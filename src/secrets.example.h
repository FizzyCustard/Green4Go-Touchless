#ifndef SECRETS_H
#define SECRETS_H

// Personal settings template.
// Copy this file to src/secrets.h and fill in your own details.
// src/secrets.h is gitignored so your details stay out of the repo.

// WiFi Credentials
static const char *WIFI_SSID = "your-wifi-name";
static const char *WIFI_PASS = "your-wifi-password";

// --- TfL Bus API ---
// Find your stop ID at https://tfl.gov.uk/ (e.g. "490000000X")
#define TFL_STOP_ID "YOUR_STOP_ID"
// Comma separated list of lines to filter, e.g. "36,185", or "" for all.
#define TFL_FILTER_LINES ""

// --- WLED Integration ---
// IP address of your WLED instance
#define WLED_IP "192.168.1.100"

#endif // SECRETS_H
