#ifndef BUS_CLIENT_H
#define BUS_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

class BodyStream;

class BusClient {
public:
  BusClient();
  void begin();

  // Returns seconds until the next matching bus, or a negative BUS_* status
  // code. The route of that bus is copied into lineOut, and the seconds
  // until the one after it into secondOut (-1 if there isn't one).
  int getNextBusSeconds(char *lineOut, size_t lineLen, int &secondOut);

private:
  WiFiClientSecure _secureClient;

  int parseNextArrival(BodyStream &stream, char *lineOut, size_t lineLen,
                       int &secondOut);
  static bool lineMatchesFilter(const char *lineName);
};

#endif
