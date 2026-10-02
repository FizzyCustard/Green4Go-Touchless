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

  // Returns time to next bus in minutes, or -1 if error/no bus
  int getNextBusTime();

private:
  WiFiClientSecure _secureClient;

  // Stream parser helper
  // Returns minutes to arrival
  int parseNextArrival(Stream &stream);
};

#endif
