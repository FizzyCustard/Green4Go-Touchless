#include "WledClient.h"
#include "Config.h"

WledClient::WledClient() {
  _hasSavedState = false;
  _restorePending = false;
  _backoffStart = 0;
  _inBackoff = false;
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

static const char PRESET_COFFEE[] =
    R"json({"on":true,"bri":200,"transition":7,"seg":[{"id":0,"on":true,"frz":false,"bri":255,"col":[[255,221,209],[0,0,0],[0,0,0]],"fx":0,"pal":0}]})json";

// Blue breathe while a bus fetch is in flight
static const char EFFECT_LOADING[] =
    R"json({"on":true,"seg":[{"id":0,"fx":2,"sx":128,"ix":128,"col":[[0,0,255]]}]})json";

// Fast green breathe: "go now"
static const char EFFECT_LEAVE_NOW[] =
    R"json({"on":true,"seg":[{"id":0,"fx":2,"sx":220,"ix":128,"col":[[0,255,0]]}]})json";

void WledClient::setRainbow() { sendRequest(PRESET_RAINBOW); }

void WledClient::setCoffee() { sendRequest(PRESET_COFFEE); }

void WledClient::setLoading() { sendRequest(EFFECT_LOADING); }

void WledClient::setLeaveNow() { sendRequest(EFFECT_LEAVE_NOW); }

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

  char json[96];
  snprintf(json, sizeof(json),
           R"json({"on":true,"seg":[{"id":0,"fx":0,"col":[[%s]]}]})json",
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
