#ifndef WLED_CLIENT_H
#define WLED_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

// Talks to a WLED controller over its JSON API. All calls block for at most
// WLED_TIMEOUT_MS and run on the network task, never the UI loop.
class WledClient {
public:
  WledClient();
  void begin();

  void setRainbow();
  void setCoffee();
  void setLoading();
  void updateForBusTime(int minutes);
  void setLeaveNow();

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

  bool ready(const char *what, bool ignoreBackoff = false);
  bool buildUrl(String &url);
  void noteResult(int httpCode);
  int sendRequest(const char *json, bool ignoreBackoff = false);
};

#endif
