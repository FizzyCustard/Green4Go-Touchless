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
  _lastRaw = -1.0;
  _speed = 0;
  _lastUpdateMs = 0;
  _rawCount = 0;
  _rawIdx = 0;
  _lastValid = -1.0;
  _missCount = 0;
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

// Weight of a new sample for a low-pass filter at cutoffHz
static float lowPassAlpha(float cutoffHz, float dt) {
  float tau = 1.0f / (2.0f * PI * cutoffHz);
  return 1.0f / (1.0f + tau / dt);
}

void DistanceSensor::update() {
  float raw = readDistance();
  _lastRaw = raw;

  // HC-SR04s drop the odd echo even with a hand held still. Bridge short
  // runs of misses with the last good reading; only a sustained miss means
  // the hand has really gone, and then we treat it as "far" so the cursor
  // slides out of the zones
  float dist;
  if (raw >= 0) {
    _missCount = 0;
    _lastValid = raw;
    dist = raw;
  } else if (_missCount < SENSOR_MAX_MISSES && _lastValid >= 0) {
    _missCount++;
    dist = _lastValid;
  } else {
    dist = SENSOR_FAR_CM;
  }

  // Median of the last 3 readings drops single-sample spikes (stray echoes)
  // before they reach the EMA, which would otherwise smear them out
  _raw[_rawIdx] = dist;
  _rawIdx = (_rawIdx + 1) % 3;
  if (_rawCount < 3)
    _rawCount++;
  float filtered =
      (_rawCount < 3) ? dist : median3(_raw[0], _raw[1], _raw[2]);

  // One-euro filter: the cutoff rises with the hand's speed, so a still
  // hand gets heavy smoothing and a moving one barely any lag
  unsigned long now = millis();
  if (_smoothedDist < 0) {
    _smoothedDist = filtered;
    _speed = 0;
  } else {
    float dt = (now - _lastUpdateMs) / 1000.0f;
    if (dt <= 0)
      dt = MAIN_LOOP_INTERVAL_MS / 1000.0f;
    float rawSpeed = (filtered - _smoothedDist) / dt;
    _speed += lowPassAlpha(SENSOR_DCUTOFF_HZ, dt) * (rawSpeed - _speed);
    float cutoff = SENSOR_MIN_CUTOFF_HZ + SENSOR_BETA * fabsf(_speed);
    _smoothedDist += lowPassAlpha(cutoff, dt) * (filtered - _smoothedDist);
  }
  _lastUpdateMs = now;

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

  float distance = duration * SPEED_OF_SOUND_CM_PER_US / 2; // There and back

  // Validate reasonable distance range (0.2cm to 400cm for typical ultrasonic
  // sensors)
  if (distance < MIN_DISTANCE_CM || distance > MAX_DISTANCE_CM) {
    return -1.0; // Out of reasonable range
  }

  return distance;
}

float DistanceSensor::getSmoothedDistance() { return _smoothedDist; }

float DistanceSensor::getRawDistance() { return _lastRaw; }
