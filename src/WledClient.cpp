#include "WledClient.h"
#include "Config.h"

WledClient::WledClient() {
  _hasSavedState = false;
  _restorePending = false;
  _backoffStart = 0;
  _inBackoff = false;
  _status[0] = '\0';
}

void WledClient::begin() {
  // Reserve once up front, before TLS fragments the heap
  _savedState.reserve(WLED_STATE_MAX_BYTES);
}

// WLED lives on the LAN, so a failed call means it's off or unplugged. Back
// off for a while rather than making every request wait out a timeout, which
// would also delay any bus fetch queued behind it.
bool WledClient::ready(const char *what, bool ignoreBackoff) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[WLED] %s skipped: WiFi not connected\n", what);
    return false;
  }
  if (_inBackoff && millis() - _backoffStart >= WLED_BACKOFF_MS)
    _inBackoff = false;
  if (_inBackoff && !ignoreBackoff) {
    Serial.printf("[WLED] %s skipped: WLED unreachable, backing off\n", what);
    return false;
  }
  return true;
}

void WledClient::noteResult(int httpCode) {
  if (httpCode > 0) {
    _inBackoff = false;
    return;
  }
  _inBackoff = true;
  _backoffStart = millis();
}

bool WledClient::buildUrl(String &url) {
  if (strlen(WLED_IP) > MAX_IP_LENGTH) {
    Serial.println("[WLED] WLED_IP too long");
    return false;
  }
  url = "http://" WLED_IP "/json/state";
  return url.length() <= MAX_URL_LENGTH;
}

// Returns the HTTP code, or 0 if the request was skipped
int WledClient::sendRequest(const char *json, bool ignoreBackoff) {
  // Anything else sent may have painted over the status LEDs
  _status[0] = '\0';

  String url;
  if (!ready("Send", ignoreBackoff) || !buildUrl(url))
    return 0;

  size_t len = strlen(json);
  Serial.printf("[WLED] POST %u bytes\n", (unsigned)len);

  HTTPClient http;
  // A fresh connection each time: a kept-alive socket goes stale when WLED
  // reboots or WiFi drops, and the next command would vanish into it
  http.setReuse(false);
  http.setConnectTimeout(WLED_TIMEOUT_MS);
  http.setTimeout(WLED_TIMEOUT_MS);
  http.begin(_client, url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST((uint8_t *)json, len);
  noteResult(httpCode);

  if (httpCode > 0) {
    Serial.printf("[WLED] Success: %d\n", httpCode);
  } else {
    Serial.printf("[WLED] Failed: %s (%d)\n",
                  http.errorToString(httpCode).c_str(), httpCode);
  }

  http.end();
  return httpCode;
}

// Presets only touch segment 0, so any other segments the user has set up
// on their controller are left alone
static const char PRESET_RAINBOW[] =
    R"json({"on":true,"bri":200,"transition":7,"seg":[{"id":0,"on":true,"frz":false,"bri":255,"col":[[188,71,255],[0,0,0],[0,0,0]],"fx":9,"sx":128,"ix":128,"pal":0}]})json";

// Coffee white. The status LEDs only show while segment 0 is this colour.
#define COFFEE_R 255
#define COFFEE_G 221
#define COFFEE_B 209
#define COFFEE_HEX "FFDDD1"

static const char PRESET_COFFEE[] =
    R"json({"on":true,"bri":200,"transition":7,"seg":[{"id":0,"on":true,"frz":false,"bri":255,"col":[[255,221,209],[0,0,0],[0,0,0]],"fx":0,"pal":0}]})json";

// Blue breathe while a bus fetch is in flight
static const char EFFECT_LOADING[] =
    R"json({"on":true,"seg":[{"id":0,"on":true,"frz":false,"fx":2,"sx":128,"ix":128,"col":[[0,0,255]]}]})json";

// Fast green breathe: "go now"
static const char EFFECT_LEAVE_NOW[] =
    R"json({"on":true,"seg":[{"id":0,"on":true,"frz":false,"fx":2,"sx":220,"ix":128,"col":[[0,255,0]]}]})json";

void WledClient::setRainbow() { sendRequest(PRESET_RAINBOW); }

void WledClient::setCoffee() { sendRequest(PRESET_COFFEE); }

void WledClient::setLoading() { sendRequest(EFFECT_LOADING); }

void WledClient::setLeaveNow() { sendRequest(EFFECT_LEAVE_NOW); }

// --- Bus status on the coffee light ---

// Reads 4 bytes and checks for "true" (a JSON false reads as "fals")
static bool readTrue(Stream &s) {
  char b[4];
  return s.readBytes(b, 4) == 4 && memcmp(b, "true", 4) == 0;
}

// Reads the strip's state and reports whether segment 0 is showing coffee
// white: on, solid, coffee colour. Also returns the segment's length and
// whether it's frozen (drawing individual LEDs freezes it). Relies on the
// key order WLED writes: top-level "on" first, then per segment "len",
// "on", "frz", "col", "fx". Anything unexpected reads as "not coffee".
bool WledClient::readCoffee(int &len, bool &frozen) {
  String url;
  if (!ready("Status") || !buildUrl(url))
    return false;

  HTTPClient http;
  http.setReuse(false);
  http.useHTTP10(true); // Plain body, no chunking
  http.setConnectTimeout(WLED_TIMEOUT_MS);
  http.setTimeout(WLED_TIMEOUT_MS);
  http.begin(_client, url);

  int httpCode = http.GET();
  noteResult(httpCode);
  bool coffee = false;
  if (httpCode == HTTP_CODE_OK) {
    Stream &s = http.getStream();
    s.setTimeout(WLED_TIMEOUT_MS);
    int r = 0, g = 0, b = 0;
    if (s.find("\"on\":") && readTrue(s) && s.find("\"seg\":[") &&
        s.find("\"len\":")) {
      len = s.parseInt();
      if (s.find("\"on\":") && readTrue(s) && s.find("\"frz\":")) {
        frozen = readTrue(s);
        if (s.find("\"col\":[[")) {
          r = s.parseInt();
          g = s.parseInt();
          b = s.parseInt();
          coffee = r == COFFEE_R && g == COFFEE_G && b == COFFEE_B &&
                   s.find("\"fx\":") && s.parseInt() == 0 && len > 0;
        }
      }
    }
  } else {
    Serial.printf("[WLED] Status check failed: %s\n",
                  http.errorToString(httpCode).c_str());
  }
  http.end();
  return coffee;
}

// Status colour for a bus `seconds` away, scaled by STATUS_BRIGHTNESS
static void statusColour(int seconds, char *hex, size_t size) {
  static const int RED[3] = {255, 0, 0};
  static const int ORANGE[3] = {255, 110, 0};
  static const int GREEN[3] = {0, 255, 0};
  const int *from = RED, *to = RED;
  float t = 0; // How far from `from` to `to`
  float m = seconds / 60.0f;

  if (m < LEAVE_WINDOW_MIN_MIN) {
    // Past the window: too late for this one
  } else if (m <= LEAVE_WINDOW_MAX_MIN) {
    from = to = GREEN;
  } else if (m <= STATUS_ORANGE_MIN) {
    from = GREEN;
    to = ORANGE;
    t = (m - LEAVE_WINDOW_MAX_MIN) / (STATUS_ORANGE_MIN - LEAVE_WINDOW_MAX_MIN);
  } else if (m <= STATUS_RED_MIN) {
    from = ORANGE;
    to = RED;
    t = (m - STATUS_ORANGE_MIN) / (STATUS_RED_MIN - STATUS_ORANGE_MIN);
  }

  int c[3];
  for (int i = 0; i < 3; i++)
    c[i] = (int)((from[i] + (to[i] - from[i]) * t) * STATUS_BRIGHTNESS / 100 +
                 0.5f);
  snprintf(hex, size, "%02X%02X%02X", c[0], c[1], c[2]);
}

bool WledClient::updateCoffeeStatus(int seconds) {
  int len = 0;
  bool frozen = false;
  if (!readCoffee(len, frozen)) {
    _status[0] = '\0';
    return false;
  }

  char json[sizeof(_status)];
  bool show = seconds >= 0;
  if (show) {
    char hex[7];
    statusColour(seconds, hex, sizeof(hex));
    int n = len < STATUS_LEDS ? len : STATUS_LEDS;
    snprintf(json, sizeof(json),
             R"json({"seg":[{"id":0,"i":[0,%d,"%s",%d,%d,")json" COFFEE_HEX
             R"json("]}]})json",
             n, hex, n, len);
  } else {
    // Unfreezing hands the strip back to the plain white effect
    snprintf(json, sizeof(json), R"json({"seg":[{"id":0,"frz":false}]})json");
  }

  // Already showing? Drawing freezes the segment, so a thawed one means
  // something (like reselecting coffee) has repainted it since
  if (strcmp(json, _status) == 0 && frozen == show)
    return true;
  if (sendRequest(json) > 0) {
    strncpy(_status, json, sizeof(_status) - 1);
    _status[sizeof(_status) - 1] = '\0';
  }
  return true;
}

void WledClient::updateForBusTime(int minutes) {
  if (minutes < 0)
    return;

  // Same thresholds as the Bus screen (see Config.h)
  const char *color;
  if (minutes <= BUS_RED_MAX_MIN || minutes > BUS_ORANGE_MAX_MIN)
    color = "255,0,0"; // Red: too soon to catch, or a long wait
  else if (minutes <= BUS_GREEN_MAX_MIN)
    color = "0,255,0"; // Green: comfortable
  else
    color = "255,165,0"; // Orange: getting long

  char json[112];
  snprintf(json, sizeof(json),
           R"json({"on":true,"seg":[{"id":0,"on":true,"frz":false,"fx":0,"col":[[%s]]}]})json",
           color);
  Serial.printf("[WLED] Bus colour for %d min\n", minutes);
  sendRequest(json);
}

void WledClient::saveState() {
  // A new effect owns the strip now; any pending restore happens when it ends
  _restorePending = false;

  if (_hasSavedState) {
    // A previous restore failed; the state we hold is the user's real one,
    // whereas the strip is probably still showing an effect
    Serial.println("[WLED] Keeping earlier unrestored state");
    return;
  }
  String url;
  if (!ready("Save") || !buildUrl(url))
    return;

  HTTPClient http;
  http.setReuse(false);
  http.useHTTP10(true); // Plain body, no chunking
  http.setConnectTimeout(WLED_TIMEOUT_MS);
  http.setTimeout(WLED_TIMEOUT_MS);
  http.begin(_client, url);

  int httpCode = http.GET();
  noteResult(httpCode);
  if (httpCode == HTTP_CODE_OK) {
    // Read into the pre-reserved buffer, refusing anything oversized so a
    // misbehaving controller can't eat the heap the TLS fetches need
    int size = http.getSize();
    WiFiClient &stream = http.getStream();
    _savedState = "";
    bool tooBig = size > WLED_STATE_MAX_BYTES;
    unsigned long start = millis();
    while (!tooBig && (size < 0 || (int)_savedState.length() < size) &&
           millis() - start < WLED_TIMEOUT_MS * 2) {
      int c = stream.read();
      if (c < 0) {
        if (!stream.connected())
          break;
        delay(1);
        continue;
      }
      if (_savedState.length() >= WLED_STATE_MAX_BYTES) {
        tooBig = true;
        break;
      }
      _savedState += (char)c;
    }

    _hasSavedState = !tooBig && _savedState.length() > 0 &&
                     _savedState.charAt(0) == '{';
    if (_hasSavedState) {
      Serial.printf("[WLED] State saved (%u bytes)\n",
                    (unsigned)_savedState.length());
    } else {
      Serial.println("[WLED] State not saved: empty, invalid or too large");
      _savedState = "";
    }
  } else {
    Serial.printf("[WLED] Failed to save state: %s\n",
                  http.errorToString(httpCode).c_str());
  }
  http.end();
}

// The user picked new lights themselves, so there's nothing to go back to
void WledClient::discardSavedState() {
  _hasSavedState = false;
  _restorePending = false;
  _savedState = "";
}

void WledClient::restoreState() {
  if (!_hasSavedState || _savedState.length() == 0) {
    _restorePending = false;
    return;
  }

  // Always try the restore, even during backoff: one blip must not leave the
  // lights stuck on an effect. If it fails, keep the state and flag it so the
  // network task retries until it lands.
  Serial.println("[WLED] Restoring State...");
  if (sendRequest(_savedState.c_str(), true) > 0) {
    _hasSavedState = false;
    _restorePending = false;
  } else {
    _restorePending = true;
  }
}
