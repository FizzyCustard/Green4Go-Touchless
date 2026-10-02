#include "DistanceSensor.h"
#include "Config.h"
DistanceSensor::DistanceSensor(int trigPin, int echoPin) {
  // Validate pin numbers
  if (trigPin < 0 || trigPin > MAX_ESP32_PIN || echoPin < 0 ||
      echoPin > MAX_ESP32_PIN) {
    Serial.println("ERROR: Invalid pin numbers for DistanceSensor");
    _trigPin = -1;
    _echoPin = -1;
  } else {
    _trigPin = trigPin;
    _echoPin = echoPin;
  }
  _smoothedDist = -1.0;
  _lastZoneIdx = -1;
}

void DistanceSensor::begin() {
  // Validate pins before use
  if (_trigPin < 0 || _echoPin < 0) {
    Serial.println("ERROR: Cannot initialize DistanceSensor - invalid pins");
    return;
  }
  pinMode(_trigPin, OUTPUT);
  pinMode(_echoPin, INPUT);
}

void DistanceSensor::update() {
  float dist = readDistance();

  if (dist < 0) {
    // Timeout/Infinity. Start moving smoothed towards a "far" value.
    // Let's say 100cm is "far enough" to be outside interaction.
    dist = 100.0;
  }

  // EMA Smoothing
  if (_smoothedDist < 0) {
    _smoothedDist = dist;
  } else {
    _smoothedDist = (EMA_ALPHA * dist) + ((1.0 - EMA_ALPHA) * _smoothedDist);
  }

  // Debug (Viewable if USB connected)
  // Raw might be -1.0 (Timeout)
  if (Serial) {
    Serial.printf("Sensor - Raw: %.2f, Smoothed: %.2f\n",
                  (dist == 100.0 && readDistance() < 0) ? -1.0 : dist,
                  _smoothedDist);
  }
}

float DistanceSensor::readDistance() {
  // Validate pins before use
  if (_trigPin < 0 || _echoPin < 0) {
    return -1.0; // Invalid configuration
  }

  digitalWrite(_trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(_trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(_trigPin, LOW);

  long duration = pulseIn(_echoPin, HIGH, 30000); // 30ms timeout
  if (duration == 0)
    return -1.0;

  float distance = duration * 0.034 / 2;

  // Validate reasonable distance range (0.2cm to 400cm for typical ultrasonic
  // sensors)
  if (distance < MIN_DISTANCE_CM || distance > MAX_DISTANCE_CM) {
    return -1.0; // Out of reasonable range
  }

  return distance;
}

float DistanceSensor::getSmoothedDistance() { return _smoothedDist; }

// Logic to determine which "slot" or "zone" is active based on total items
// Usable range is 0 to 15cm.
// Anything > 15cm is "Base" (Index -1 or totalItems)
int DistanceSensor::getActiveZone() {
  // This method might need to be context-aware (how many items?)
  // But for raw zone logic consistent with previous code, let's just expose the
  // distance and let StateManager assume the mapping? Or we provide a helper:
  return -1; // Placeholder, usage will be in StateManager mostly via
             // getSmoothedDistance
}
