#include "WledClient.h"
#include "Config.h"

WledClient::WledClient() {
  _hasSavedState = false;
  _savedState = "";
  _backoffUntil = 0;
  _inBackoff = false;
}

void WledClient::begin() {
  // Nothing specific to init for HTTP
}

// WLED lives on the LAN, so a failed call means it's off or unplugged. Back
// off for a while rather than making every request wait out a timeout, which
// would also delay any bus fetch queued behind it.
bool WledClient::ready(const char *what, bool ignoreBackoff) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[WLED] %s skipped: WiFi not connected\n", what);
    return false;
  }
  if (!ignoreBackoff && _inBackoff && (long)(millis() - _backoffUntil) < 0) {
    Serial.printf("[WLED] %s skipped: WLED unreachable, backing off\n", what);
    return false;
  }
  _inBackoff = false;
  return true;
}

void WledClient::noteResult(int httpCode) {
  if (httpCode > 0)
    return;
  _inBackoff = true;
  _backoffUntil = millis() + WLED_BACKOFF_MS;
}

bool WledClient::buildUrl(String &url) {
  // Validate WLED_IP length to prevent excessive memory usage
  if (strlen(WLED_IP) > MAX_IP_LENGTH) {
    Serial.println("[WLED] WLED_IP too long");
    return false;
  }
  url = "http://" + String(WLED_IP) + "/json/state";
  return url.length() <= MAX_URL_LENGTH;
}

// Returns the HTTP code, or 0 if the request was skipped
int WledClient::sendRequest(String &jsonPayload, bool ignoreBackoff) {
  String url;
  if (!ready("Send", ignoreBackoff) || !buildUrl(url))
    return 0;

  Serial.printf("[WLED] Sending to %s: %s\n", url.c_str(), jsonPayload.c_str());

  HTTPClient http;
  http.setConnectTimeout(WLED_TIMEOUT_MS);
  http.setTimeout(WLED_TIMEOUT_MS);
  http.begin(_client, url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(jsonPayload);
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

// WLED JSON Presets
// Removed start/stop to apply to full configured segment
const char *PRESET_RAINBOW =
    R"json({"on":true,"bri":200,"transition":7,"mainseg":0,"seg":[{"id":0,"grp":1,"spc":0,"of":0,"on":true,"frz":false,"bri":255,"cct":127,"set":0,"col":[[188,71,255],[0,0,0],[0,0,0]],"fx":9,"sx":128,"ix":128,"pal":0,"c1":128,"c2":128,"c3":16,"sel":true,"rev":false,"mi":false,"o1":false,"o2":false,"o3":false,"si":0,"m12":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0}]})json";

const char *PRESET_COFFEE =
    R"json({"on":true,"bri":200,"transition":7,"mainseg":0,"seg":[{"id":0,"grp":1,"spc":0,"of":0,"on":true,"frz":false,"bri":255,"cct":127,"set":0,"n":"","col":[[255,221,209],[0,0,0],[0,0,0]],"fx":0,"sx":128,"ix":128,"pal":0,"c1":128,"c2":128,"c3":16,"sel":true,"rev":false,"mi":false,"o1":false,"o2":false,"o3":false,"si":0,"m12":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0},{"stop":0}]})json";

void WledClient::setRainbow() {
  String json = String(PRESET_RAINBOW);
  sendRequest(json);
}

void WledClient::setCoffee() {
  String json = String(PRESET_COFFEE);
  sendRequest(json);
}

void WledClient::setLoading() {
  // Blue Breathing
  // fx: 2 (Breathe), sx: 128 (Speed), ix: 128 (Intensity)
  // col: [[0, 0, 255]] (Blue)
  String json = "{\"seg\":[{\"fx\":2,\"sx\":128,\"ix\":128,\"col\":[[0,0,255]]}"
                "],\"on\":true}";
  Serial.println("[WLED] Setting Loading Animation");
  sendRequest(json);
}

void WledClient::updateForBusTime(int minutes) {
  if (minutes < 0)
    return;

  // Logic (must match DisplayManager::drawBusScreen):
  // Red: <= 5 mins OR > 12 mins
  // Green: 6 - 9 mins
  // Orange: 10 - 12 mins

  String colorStr;
  if (minutes <= 5) {
    colorStr = "[[255,0,0]]"; // Red
  } else if (minutes <= 9) {
    colorStr = "[[0,255,0]]"; // Green
  } else if (minutes <= 12) {
    colorStr = "[[255,165,0]]"; // Orange
  } else {
    colorStr = "[[255,0,0]]"; // Red
  }

  // fx: 0 is Solid (Static).
  // No start/stop specified -> applies to configured segment(s)
  String json = "{\"seg\":[{\"fx\":0,\"col\":" + colorStr + "}],\"on\":true}";

  Serial.printf("[WLED] Bus Update (%d mins), Sending: %s\n", minutes,
                json.c_str());
  sendRequest(json);
}

void WledClient::setLeaveNow() {
  // Fast green breathe: "go now"
  String json = "{\"seg\":[{\"fx\":2,\"sx\":220,\"ix\":128,\"col\":[[0,255,0]]}"
                "],\"on\":true}";
  Serial.println("[WLED] Leave now!");
  sendRequest(json);
}

void WledClient::saveState() {
  if (_hasSavedState) {
    // A previous restore failed; the state we hold is the user's real one,
    // whereas the strip is probably still showing the bus colour
    Serial.println("[WLED] Keeping earlier unrestored state");
    return;
  }
  String url;
  if (!ready("Save") || !buildUrl(url))
    return;

  HTTPClient http;
  http.setConnectTimeout(WLED_TIMEOUT_MS);
  http.setTimeout(WLED_TIMEOUT_MS);
  http.begin(_client, url);

  int httpCode = http.GET();
  noteResult(httpCode);
  if (httpCode == HTTP_CODE_OK) {
    _savedState = http.getString();
    _hasSavedState = _savedState.length() > 0;
    Serial.println("[WLED] State Saved");
  } else {
    Serial.printf("[WLED] Failed to save state: %s\n",
                  http.errorToString(httpCode).c_str());
  }
  http.end();
}

// The user picked new lights themselves, so there's nothing to go back to
void WledClient::discardSavedState() {
  _hasSavedState = false;
  _savedState = "";
}

void WledClient::restoreState() {
  if (!_hasSavedState || _savedState.length() == 0)
    return;

  // Always try the restore, even during backoff: one blip mid-effect must
  // not leave the lights stuck on the bus colour. Keep the saved state if it
  // fails so a later restore can still put things back.
  Serial.println("[WLED] Restoring State...");
  if (sendRequest(_savedState, true) > 0)
    _hasSavedState = false;
}
