# Interaction Model

An HC-SR04 (`DistanceSensor`) gives one smoothed distance per 50ms tick. `StateManager` maps it to rows on the 240x320 ST7789 (`DisplayManager`): 4-22cm over the three 80px content rows (y 0-240), 22-32cm over the 80px base strip (y 240-320).

## Zones with hysteresis (`StateManager::calculateZone`)

```cpp
int StateManager::calculateZone(float dist, int numZones, int prevZone) {
  if (numZones <= 0 || dist < 0)
    return ROW_NONE;
  float zoneSize =
      (MAX_INTERACTION_DISTANCE - MIN_INTERACTION_DISTANCE) / numZones; // 18 / 3 = 6cm
  float pos = dist - MIN_INTERACTION_DISTANCE;

  // Stay in the current zone until ZONE_HYSTERESIS_CM past its edge
  if (prevZone >= 0 && prevZone < numZones) {
    float lo = prevZone * zoneSize - ZONE_HYSTERESIS_CM;
    float hi = (prevZone + 1) * zoneSize + ZONE_HYSTERESIS_CM;
    if ((prevZone == 0 || pos >= lo) && pos <= hi) // zone 0 has no inner edge
      return prevZone;
  }
  if (dist > MAX_INTERACTION_DISTANCE)
    return ROW_NONE;
  int zone = (int)(pos / zoneSize);
  if (zone >= numZones)
    zone = numZones - 1;
  if (zone < 0)
    zone = 0;
  return zone;
}
```

Fresh entry boundaries: zone 0 = up to 10cm, zone 1 = 10-16cm, zone 2 = 16-22cm. Once in zone 1 the hand stays there from 8.75cm to 17.25cm.

Constants: `ROW_NONE = -1`, `ROW_UNSET = -2` (disarmed, no reading yet on this screen).

## Per-tick pattern (`StateManager::updateInteractive`)

```cpp
int numZones = 3;   // always three; 0 = nothing selectable (timer menu hidden/animating)
int firstRow = 0;   // 1 on Timer and Bus Times (top row is info only)

int zone = calculateZone(dist, numZones, _rawZone);
_rawZone = zone;                                  // kept across screens
int row = (zone >= firstRow) ? zone : ROW_NONE;

if (handPresent(dist))
  resetStandbyTimer();

// Re-arm once the hand leaves, or genuinely moves to a different row
if (!_armed) {
  bool arm = false;
  if (_rearmRow == ROW_UNSET)
    _rearmRow = row;                              // first reading after disarm
  else if (row == ROW_NONE && _rearmRow != ROW_NONE)
    arm = true;                                   // hand left the rows
  else if (row != _rearmRow && fabsf(dist - _disarmDist) > REARM_DISTANCE_CM)
    arm = true;                                   // real move to another row
  if (arm) {
    _armed = true;
    _zoneStartTime = now;                         // dwell counts from arming
    _lastZoneIdx = row;
    _graceRow = ROW_NONE;                         // no resuming older dwells
  }
}

// Dwell: restart the clock when the row changes, unless the hand is back
// on the row it left less than DWELL_GRACE_MS ago (then resume it)
if (row != _lastZoneIdx) {
  unsigned long start = now;
  if (row >= 0 && row == _graceRow && now - _graceLeftAt <= DWELL_GRACE_MS)
    start = _graceStart + (now - _graceLeftAt);
  _graceRow = _lastZoneIdx;
  _graceStart = _zoneStartTime;
  _graceLeftAt = now;
  _lastZoneIdx = row;
  _zoneStartTime = start;
}
float progress = 0;
if (_armed && row >= 0)
  progress = (now - _zoneStartTime) / (float)DWELL_THRESHOLD_MS;

// ... draw the current screen with `row` as the hover state ...
_display->drawDwellProgress(row, progress);

if (progress >= 1.0f)
  handleSelection(row);   // runs the action, then disarm()
```

`disarm()` sets `_armed = false`, `_rearmRow = ROW_UNSET`, `_disarmDist = current smoothed distance`. `goToState()` calls it too, and resets `_lastZoneIdx` to `ROW_NONE` but not `_rawZone`.

Arming restarts `_zoneStartTime`, so time spent on a row while disarmed never counts towards the dwell.

## Timer menu (`StateManager::updateTimerMenu`)

```cpp
bool baseActive = dist > BASE_ACTIVE_MIN_DISTANCE && dist < BASE_ACTIVE_MAX_DISTANCE;
if (!_timerMenuArmed && dist > TIMER_MENU_CLOSE_DISTANCE)
  _timerMenuArmed = true;                     // hand must leave once first
if (!_timerMenuVisible && _timerMenuArmed && baseActive)  -> open, return true
if (_timerMenuVisible && dist > TIMER_MENU_CLOSE_DISTANCE) -> close, return true
```
On the Timer screen `numZones = (menuVisible && !toggledThisTick && !display->isAnimating()) ? 3 : 0`.

## Presence

```cpp
void trackMovement(float dist) {     // every tick
  if (_moveRefDist < 0 || fabsf(dist - _moveRefDist) > MOVE_THRESHOLD_CM) {
    _moveRefDist = dist;
    _lastMoveAt = millis();
  }
}
bool handPresent(float dist) {
  return dist >= 0 && dist < BASE_ACTIVE_MAX_DISTANCE &&
         millis() - _lastMoveAt < STATIC_OBJECT_MS;   // 60s
}
```
Used for: unlock trigger (`dist < UNLOCK_DISTANCE_CM && handPresent`), standby timer reset, display sleep.

## Sensor smoothing (`DistanceSensor::update`)

```cpp
float raw = readDistance();                 // -1 on timeout / out of range
if (raw >= 0)                         { _missCount = 0; _lastValid = raw; dist = raw; }
else if (_missCount < SENSOR_MAX_MISSES && _lastValid >= 0) { _missCount++; dist = _lastValid; }
else                                  dist = SENSOR_FAR_CM;   // 100cm: hand gone

filtered = median of last 3 dist values (dist itself until 3 collected);
// One-euro filter: cutoff rises with speed
float rawSpeed = (filtered - _smoothedDist) / dt;
_speed += lowPassAlpha(SENSOR_DCUTOFF_HZ, dt) * (rawSpeed - _speed);
float cutoff = SENSOR_MIN_CUTOFF_HZ + SENSOR_BETA * fabsf(_speed);
_smoothedDist += lowPassAlpha(cutoff, dt) * (filtered - _smoothedDist);
```
`getSmoothedDistance()` returns -1 before the first reading.
