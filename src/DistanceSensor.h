#ifndef DISTANCE_SENSOR_H
#define DISTANCE_SENSOR_H

#include <Arduino.h>

class DistanceSensor {
public:
  DistanceSensor(int trigPin, int echoPin);
  void begin();
  void update();
  float getSmoothedDistance();
  float getRawDistance(); // Last ping, -1 if it missed

private:
  int _trigPin;
  int _echoPin;
  float _smoothedDist;
  float _lastRaw;

  // One-euro filter state
  float _speed;              // Smoothed rate of change, cm/s
  unsigned long _lastUpdateMs;

  // Last three raw readings for the median filter
  float _raw[3];
  uint8_t _rawCount;
  uint8_t _rawIdx;

  // Missed-echo bridging
  float _lastValid;
  uint8_t _missCount;

  float readDistance();
};

#endif
