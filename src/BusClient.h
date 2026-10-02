#ifndef BUS_CLIENT_H
#define BUS_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

class BusClient {
public:
  BusClient();
  void begin();

  // Returns seconds until the next matching bus, or a negative BUS_* status
  // code. The route of that bus is copied into lineOut.
  int getNextBusSeconds(char *lineOut, size_t lineLen);

private:
  WiFiClientSecure _secureClient;

  int parseNextArrival(Stream &stream, char *lineOut, size_t lineLen);
  static bool lineMatchesFilter(const String &lineName);
};

#endif
