#ifndef DISTANCE_SENSOR_H
#define DISTANCE_SENSOR_H

#include <Arduino.h>

class DistanceSensor {
public:
  DistanceSensor(int trigPin, int echoPin);
  void begin();
  void update();
  float getSmoothedDistance();
  int getActiveZone();

private:
  int _trigPin;
  int _echoPin;
  float _smoothedDist;
  int _lastZoneIdx;

  // Config
  const float EMA_ALPHA = 0.35;
  const float ZONE_SIZE =
      4.0; /* Adjusted logic needed for v2 spec: <15cm usable */
  const float HYSTERESIS_BUFFER = 0.5;

  // v2 Spec:
  // Usable Zone: < 15cm
  // Base Zone: > 15cm (Active when < 25cm)
  // We need to map distance to normalized zones for the menu.
  // If we have N menu items in 0-15cm, each item is (15 / N) cm.

  // However, the original code used fixed 4cm zones.
  // For general usage, let's keep getActiveZone flexible or map it specifically
  // to the current Screen's requirements in StateManager?
  // Actually, Spec v2 says: "Navigation: Menu options... split the 15cm Usable
  // Zone proportionally." So Zone calculation depends on the NUMBER OF ITEMS.
  // We should probably provide a method: `int getSelectionIndex(int
  // totalItems)`

  float readDistance();
};

#endif
