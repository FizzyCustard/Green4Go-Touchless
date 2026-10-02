# Interaction Model Mathematics

The touchless interface uses an ultrasonic distance sensor `HC-SR04` (managed via `DistanceSensor.cpp`) to control a virtual cursor mapped to a 240x320 TFT screen (`DisplayManager.cpp`).

## Distance Mapping

```cpp
float itemHeight = 15.0 / numItems; 
int zone = (int)(dist / itemHeight);

// Clamp limits to prevent out-of-bounds array access
if (zone >= numItems) zone = numItems - 1;
if (zone < 0) zone = 0;
```

A menu with 3 items (`numItems = 3`) produces boundaries at:
- Zone 0: `0cm - 5cm`
- Zone 1: `5.01cm - 10cm`
- Zone 2: `10.01cm - 15cm`

## Dwell Time Selection Pattern

Use this exact pattern when checking for user selections in a new state:

```cpp
if (zone != _lastZoneIdx) {
  // Movement detected, reset the dwell timer
  _lastZoneIdx = zone;
  _zoneStartTime = millis();
} else if (zone != -1 && zone < numItems) { 
  // Hand is stable in a valid zone
  if (millis() - _zoneStartTime > DWELL_THRESHOLD) {
    // Dwell threshold reached. User intended to click this item.
    handleSelection(zone);
    
    // Reset timers immediately to prevent double-clicks
    _zoneStartTime = millis();
    resetStandbyTimer(); 
  }
}
```

## Smoothing Context (`DistanceSensor.cpp`)

The distance is smoothed using an Exponential Moving Average (EMA).

```cpp
// EMA Smoothing
_smoothedDist = (EMA_ALPHA * dist) + ((1.0 - EMA_ALPHA) * _smoothedDist);
```

If the hardware reads a timeout (`-1.0`), the system forces the distance towards a safe "far" value (100.0cm) to smoothly slide the cursor out of the active zones rather than teleporting erratically.
