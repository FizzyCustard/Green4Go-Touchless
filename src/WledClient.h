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
  void sendRequest(String &jsonPayload);
};

#endif
