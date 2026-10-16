#ifndef WLED_CLIENT_H
#define WLED_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

// Talks to a WLED controller over its JSON API. Every call is bounded by
// WLED_TIMEOUT_MS-based limits (a save reads the body within 2x that) and
// runs on the network task, never the UI loop.
class WledClient {
public:
  WledClient();
  void begin();

  void setRainbow();
  void setCoffee();
  void setLoading();
  void updateForBusTime(int minutes);
  void setLeaveNow();

  // If the lights are coffee white, colours the status LEDs for a bus this
  // many seconds away (-1 for none: plain white). Returns whether they were.
  bool updateCoffeeStatus(int seconds);

  // Effects save the user's state first and restore it afterwards
  void saveState();
  void restoreState();
  void discardSavedState();
  bool restorePending() const { return _restorePending; }

private:
  WiFiClient _client;
  String _savedState;
  bool _hasSavedState;
  bool _restorePending; // A restore failed and should be retried
  unsigned long _backoffStart;
  bool _inBackoff;
  char _status[96]; // Last status sent ("" = send again on the next update)

  bool readCoffee(int &len, bool &frozen);
  bool ready(const char *what, bool ignoreBackoff = false);
  bool buildUrl(String &url);
  void noteResult(int httpCode);
  int sendRequest(const char *json, bool ignoreBackoff = false);
};

#endif
