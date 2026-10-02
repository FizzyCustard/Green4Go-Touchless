#ifndef WLED_CLIENT_H
#define WLED_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

class WledClient {
public:
  WledClient();
  void begin();

  void setRainbow();
  void setCoffee();
  void setLoading();
  void updateForBusTime(int minutes);

  void saveState();
  void restoreState();

private:
  WiFiClient _client;
  String _savedState;
  bool _hasSavedState;
  unsigned long _backoffUntil; // Skip calls until this millis() after a failure
  bool _inBackoff;

  bool ready(const char *what, bool ignoreBackoff = false);
  bool buildUrl(String &url);
  void noteResult(int httpCode);
  int sendRequest(String &jsonPayload, bool ignoreBackoff = false);
};

#endif
