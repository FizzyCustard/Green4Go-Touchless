#include "WledClient.h"
#include "Config.h"

WledClient::WledClient() {
  _hasSavedState = false;
  _savedState = "";
}

void WledClient::begin() {
  // Nothing specific to init for HTTP
}

void WledClient::sendRequest(String &jsonPayload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WLED] WiFi not connected");
    return;
  }

  HTTPClient http;

  // Validate WLED_IP length to prevent excessive memory usage
  if (strlen(WLED_IP) > MAX_IP_LENGTH) {
    Serial.println("[WLED] WLED_IP too long");
    return;
  }

  String url = "http://" + String(WLED_IP) + "/json/state";

  // Additional safety: check final URL length
  if (url.length() > MAX_URL_LENGTH) {
    Serial.println("[WLED] Final URL too long");
    return;
  }

  Serial.printf("[WLED] Sending to %s: %s\n", url.c_str(), jsonPayload.c_str());

  // Use WiFiClient for HTTP
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(_client, url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(jsonPayload);

  if (httpCode > 0) {
    Serial.printf("[WLED] Success: %d\n", httpCode);
  } else {
    Serial.printf("[WLED] Failed: %s (%d)\n",
                  http.errorToString(httpCode).c_str(), httpCode);
  }

  http.end();
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

  // Logic:
  // Red: < 5 mins OR > 12 mins
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

void WledClient::saveState() {
  if (WiFi.status() != WL_CONNECTED)
    return;

  // Validate WLED_IP length
  if (strlen(WLED_IP) > MAX_IP_LENGTH) {
    Serial.println("[WLED] WLED_IP too long for saveState");
    return;
  }

  HTTPClient http;
  String url = "http://" + String(WLED_IP) + "/json/state";

  // Check final URL length
  if (url.length() > MAX_URL_LENGTH) {
    Serial.println("[WLED] Final URL too long for saveState");
    return;
  }

  // Use WiFiClient for HTTP
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(_client, url);

  int httpCode = http.GET();
  if (httpCode > 0) {
    _savedState = http.getString();
    _hasSavedState = true;
    Serial.println("[WLED] State Saved");
  } else {
    Serial.printf("[WLED] Failed to save state: %s\n",
                  http.errorToString(httpCode).c_str());
    _hasSavedState = false;
  }
  http.end();
}

void WledClient::restoreState() {
  if (!_hasSavedState || _savedState.length() == 0)
    return;

  Serial.println("[WLED] Restoring State...");
  sendRequest(_savedState);
  _hasSavedState = false; // Reset after restore
}
