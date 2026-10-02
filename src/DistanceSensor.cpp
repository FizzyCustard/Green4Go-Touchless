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
  _rawCount = 0;
  _rawIdx = 0;
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

static float median3(float a, float b, float c) {
  if (a > b) {
    float t = a;
    a = b;
    b = t;
  }
  if (b > c)
    b = c;
  return (a > b) ? a : b;
}

void DistanceSensor::update() {
  float raw = readDistance();

  // Timeout/no echo: treat as "far" so the cursor slides out of the zones
  float dist = (raw < 0) ? SENSOR_FAR_CM : raw;

  // Median of the last 3 readings drops single-sample spikes (stray echoes)
  // before they reach the EMA, which would otherwise smear them out
  _raw[_rawIdx] = dist;
  _rawIdx = (_rawIdx + 1) % 3;
  if (_rawCount < 3)
    _rawCount++;
  float filtered =
      (_rawCount < 3) ? dist : median3(_raw[0], _raw[1], _raw[2]);

  // EMA Smoothing
  if (_smoothedDist < 0) {
    _smoothedDist = filtered;
  } else {
    _smoothedDist =
        (EMA_ALPHA * filtered) + ((1.0 - EMA_ALPHA) * _smoothedDist);
  }

#if DEBUG_SENSOR
  Serial.printf("Sensor - Raw: %.2f, Smoothed: %.2f\n", raw, _smoothedDist);
#endif
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

  // Capped at ~2m: interaction tops out at 30cm, and a shorter wait keeps
  // the main loop (and animations) moving when nothing is in front
  long duration = pulseIn(_echoPin, HIGH, SENSOR_ECHO_TIMEOUT_US);
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
