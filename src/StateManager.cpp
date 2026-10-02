#include "StateManager.h"
#include "Config.h"

StateManager::StateManager(DistanceSensor *sensor, DisplayManager *display) {
  // Validate input pointers
  if (!sensor || !display) {
    Serial.println("ERROR: StateManager received null pointers");
    _sensor = nullptr;
    _display = nullptr;
    return;
  }

  _sensor = sensor;
  _display = display;

  _busClient = new BusClient();
  _wledClient = new WledClient();
  _networkManager = new NetworkManager(_busClient, _wledClient);

  _busClient->begin();
  _wledClient->begin();
  _networkManager->begin();

  _currentState = BOOT_WARMUP;
  _currentZoneIdx = -1;
  _lastZoneIdx = -1;
  _zoneStartTime = 0;
  _successStartTime = 0;
  _errorDisplayTimer = 0;
  _ignoreInputUntil = 0;

  _timerStartTime = 0;
  _timerRunning = false;
  _timerMenuVisible = false;

  _lastInteractionTime = millis();
  _lastBusUpdate = 0;
  _busEffectStartTime = 0;
  _cachedBusMinutes = -1;

  _unlockStartTime = 0;
  _unlocking = false;
}

StateManager::~StateManager() {
  // Clean up dynamically allocated objects
  if (_busClient) {
    delete _busClient;
    _busClient = nullptr;
  }
  if (_wledClient) {
    delete _wledClient;
    _wledClient = nullptr;
  }
  if (_networkManager) {
    delete _networkManager;
    _networkManager = nullptr;
  }
}

void StateManager::resetSelectionTimer() {
  _zoneStartTime = millis();
  _lastZoneIdx = -1; // Force re-detection
}

void StateManager::resetStandbyTimer() { _lastInteractionTime = millis(); }

void StateManager::checkStandbyTimeout() {
  if (_currentState == STANDBY || _currentState == UNLOCK_SEQUENCE)
    return;

  if (millis() - _lastInteractionTime > STANDBY_TIMEOUT_MS) {
    // Go to Standby
    _currentState = STANDBY;
    _display->clearContentArea();

    // Force immediate update of standby screen
    _lastBusUpdate = 0; // Will trigger bus update loop
  }
}

void StateManager::update() {
  // Validate components before use
  if (!_sensor || !_display) {
    return; // Cannot update with invalid components
  }

  float dist = _sensor->getSmoothedDistance();
  bool baseActive = (dist > BASE_ACTIVE_MIN_DISTANCE && dist < BASE_ACTIVE_MAX_DISTANCE);

  // Only update base zone when NOT in standby to prevent screen flicker
  if (_currentState != STANDBY) {
    _display->updateBaseZone(baseActive);
  }

  checkStandbyTimeout();

  // --- State Machine ---
  switch (_currentState) {
  case BOOT_WARMUP:
    updateWarmup();
    break;

  case STANDBY:
    updateStandby();
    break;

  case UNLOCK_SEQUENCE:
    updateUnlock();
    break;

  case MAIN_MENU:
  case LIGHT_SETTINGS:
  case BUS_TIMES:
    // Standard Zone Logic
    // But first, specifics for screens
    break;

  case SHOWING_SUCCESS:
    if (millis() - _successStartTime > SUCCESS_DISPLAY_TIME_MS) {
      _currentState = MAIN_MENU;
      _display->clearContentArea();
      resetSelectionTimer();
      _ignoreInputUntil = millis() + IGNORE_INPUT_TIME_MS;
    }
    // No zone logic during success screen
    break;

  case SHOWING_API_ERROR:
    if (millis() - _errorDisplayTimer > ERROR_DISPLAY_TIME_MS) {
      _currentState = MAIN_MENU;
      _display->clearContentArea();
      resetSelectionTimer();
      _ignoreInputUntil = millis() + IGNORE_INPUT_TIME_MS;
      _busEffectStartTime = 0;
    }
    break;

  case TIMER_SCREEN:
    // ... (Timer logic)
    break;
  }

  // --- Timer Specifics ---
  if (_currentState == TIMER_SCREEN) {
    if (baseActive && !_timerMenuVisible) {
      _display->animateTimerMenu(true);
      _timerMenuVisible = true;
      resetSelectionTimer();
      resetStandbyTimer();
    } else if (!baseActive && _timerMenuVisible && dist > 30.0) {
      _display->animateTimerMenu(false);
      _timerMenuVisible = false;
      resetSelectionTimer();
    }

    if (_timerRunning) {
      unsigned long elapsed = millis() - _timerStartTime;
      int timerY = _timerMenuVisible ? 40 : 120;
      _display->drawTimerValue(elapsed, timerY);
    }
  }

  // --- Zone Logic for Interactive Screens ---
  if (_currentState != STANDBY && _currentState != UNLOCK_SEQUENCE &&
      _currentState != SHOWING_SUCCESS && _currentState != SHOWING_API_ERROR) {
    int numItems = 0;
    switch (_currentState) {
    case MAIN_MENU:
      numItems = 3; // Timer, Bus, Lights
      break;
    case TIMER_SCREEN:
      numItems = _timerMenuVisible ? 3 : 0;
      break;
    case LIGHT_SETTINGS:
      numItems = 3;
      break;
    case BUS_TIMES:
      numItems = 1; // 1 Unified Zone for Back
      break;
    }

    int zone = -1;
    if (numItems > 0 && dist >= 0 && dist <= 15.0) {
      zone = calculateZone(dist, numItems);

      // Timer Screen Remapping
      if (_currentState == TIMER_SCREEN && _timerMenuVisible) {
        zone -= 1; // Shift 0->-1, 1->0, 2->1
      }
    }

    // Interaction Tracking
    if (dist < 25.0) {
      resetStandbyTimer();
    }

    // Selection Logic
    if (millis() < _ignoreInputUntil) {
      _lastZoneIdx = -1;
      _zoneStartTime = millis();
    } else if (zone != _lastZoneIdx) {
      _lastZoneIdx = zone;
      _zoneStartTime = millis();
    } else if (zone != -1 && zone < numItems) { // Guard against invalid zones
      if (millis() - _zoneStartTime > DWELL_THRESHOLD_MS) {
        handleSelection(zone);
        _zoneStartTime = millis();
        resetStandbyTimer();
      }
    }

    // Update Display
    switch (_currentState) {
    case MAIN_MENU:
      _display->drawMainMenu(zone);
      break;
    case TIMER_SCREEN:
      _display->drawTimerScreen(zone, _timerMenuVisible);
      break;
    case LIGHT_SETTINGS:
      _display->drawLightSettings(zone);
      break;
    case BUS_TIMES:
      // Check for incoming async bus data
      if (_networkManager->isBusDataReady()) {
        _cachedBusMinutes = _networkManager->getLatestBusMinutes();
        _networkManager->clearBusDataReady();

        if (_cachedBusMinutes == -2) {
          _currentState = SHOWING_API_ERROR;
          _errorDisplayTimer = millis();
          _display->drawErrorScreen("API Error");
          break; // Skip drawing bus screen
        }

        _networkManager->requestWledBusTime(_cachedBusMinutes);
      }
      _display->drawBusScreen(_cachedBusMinutes, zone);
      break;
    }
    _currentZoneIdx = zone;
  }

  // Check Bus Effect Timeout (10s)
  if (_currentState == BUS_TIMES && _busEffectStartTime > 0) {
    if (millis() - _busEffectStartTime > BUS_EFFECT_TIMEOUT_MS) {
      _networkManager->requestWledRestoreState();
      _busEffectStartTime = 0;
    }
  }
}

void StateManager::updateStandby() {
  float dist = _sensor->getSmoothedDistance();

  // Unlock Trigger
  if (dist < UNLOCK_DISTANCE_CM) {
    _currentState = UNLOCK_SEQUENCE;
    _unlockStartTime = millis();
    _unlocking = true;
    _display->clearContentArea();
    return;
  }

  // Bus Update Loop (Periodically)
  if (millis() - _lastBusUpdate > BUS_UPDATE_INTERVAL_MS ||
      _lastBusUpdate == 0) {
    _lastBusUpdate = millis();
    // Request async fetch
    _networkManager->requestBusUpdate();
  }

  // Apply async fetch result
  if (_networkManager->isBusDataReady()) {
    _cachedBusMinutes = _networkManager->getLatestBusMinutes();
    _networkManager->clearBusDataReady();
    _display->drawStandbyScreen(_cachedBusMinutes);
  } else {
    // Just draw with whatever we had locally
    _display->drawStandbyScreen(_cachedBusMinutes);
  }
}

void StateManager::updateUnlock() {
  float dist = _sensor->getSmoothedDistance();

  if (dist > UNLOCK_DISTANCE_CM + UNLOCK_HYSTERESIS_CM) { // Hysteresis
    // Cancel
    _currentState = STANDBY;
    _display->drawStandbyScreen(_cachedBusMinutes);
    return;
  }

  // Progress
  unsigned long elapsed = millis() - _unlockStartTime;
  float progress = (float)elapsed / UNLOCK_TIME_MS;

  _display->drawUnlockVisual(progress);

  if (progress >= 1.0) {
    // Unlocked!
    _currentState = MAIN_MENU;
    _display->clearContentArea();
    resetStandbyTimer();
    resetSelectionTimer();
  }
}

int StateManager::calculateZone(float dist, int numItems) {
  // Validate inputs
  if (numItems <= 0) {
    return -1; // Invalid number of items
  }

  if (dist < 0 || dist > MAX_INTERACTION_DISTANCE) {
    return -1; // Distance out of interaction range
  }

  float itemHeight = MAX_INTERACTION_DISTANCE / numItems;
  int zone = (int)(dist / itemHeight);

  // Clamp to valid range
  if (zone >= numItems)
    zone = numItems - 1;
  if (zone < 0)
    zone = 0;

  return zone;
}

void StateManager::handleSelection(int index) {
  switch (_currentState) {
  case MAIN_MENU:
    actionMainMenu(index);
    break;
  case TIMER_SCREEN:
    actionTimer(index);
    break;
  case LIGHT_SETTINGS:
    actionLights(index);
    break;
  case BUS_TIMES:
    actionBusTimes(index);
    break;
  case STANDBY:
  case UNLOCK_SEQUENCE:
    // Should not happen as zones are inactive
    break;
  case SHOWING_SUCCESS:
  case SHOWING_API_ERROR:
    break;
  }
  _ignoreInputUntil = millis() + IGNORE_INPUT_TIME_MS;
}

void StateManager::actionMainMenu(int index) {
  resetSelectionTimer();

  if (index == 0) { // Timer
    _currentState = TIMER_SCREEN;
    _display->clearContentArea();
    _timerStartTime = millis();
    _timerRunning = true;
    _timerMenuVisible = false;
  } else if (index == 1) { // Bus Times
    _currentState = BUS_TIMES;
    _display->clearContentArea();

    // Save WLED state before apply effect
    _networkManager->requestWledSaveState();
    _busEffectStartTime = millis();

    // Show Loading Animation
    _networkManager->requestWledLoading();

    // Fetch asynchronously
    _networkManager->requestBusUpdate();
    _cachedBusMinutes = -1; // Loading state
  } else if (index == 2) {  // Lights
    _currentState = LIGHT_SETTINGS;
    _display->clearContentArea();
  }
}

void StateManager::actionTimer(int index) {
  if (index == 0) { // Reset
    _timerStartTime = millis();
    resetSelectionTimer();
  } else if (index == 1) { // Back
    _currentState = MAIN_MENU;
    _display->clearContentArea();
    _timerRunning = false;
    resetSelectionTimer();
  }
}

void StateManager::actionLights(int index) {
  resetSelectionTimer();
  if (index == 0) { // Rainbow
    _networkManager->requestWledRainbow();
    _display->drawSuccessScreen("Rainbow!");
  } else if (index == 1) { // Solid (Now Coffee)
    _networkManager->requestWledCoffee();
    _display->drawSuccessScreen("Coffee");
  } else if (index == 2) { // Back
    _currentState = MAIN_MENU;
    _display->clearContentArea();
    return;
  }

  // Go to non-blocking success state
  _currentState = SHOWING_SUCCESS;
  _successStartTime = millis();
}

void StateManager::actionBusTimes(int index) {
  resetSelectionTimer();
  // Any zone interaction acts as Back when numItems = 1
  if (index == 0) {
    _currentState = MAIN_MENU;
    _display->clearContentArea();
    _busEffectStartTime = 0;
  }
}

void StateManager::updateWarmup() {
  _display->drawBootScreen();

  // 1. Minimum Duration Check (2 seconds)
  if (millis() < 2000) {
    return;
  }

  // 2. Data Availability Check
  // We need valid bus times before showing the standby screen.
  // _cachedBusMinutes is initialized to -1.
  if (_cachedBusMinutes < 0) {
    // Retry fetching every 5 seconds (or immediately if first run after 2s)
    if (millis() - _lastBusUpdate > 5000) {
      _networkManager->requestBusUpdate();
      _lastBusUpdate = millis();
    }

    if (_networkManager->isBusDataReady()) {
      _cachedBusMinutes = _networkManager->getLatestBusMinutes();
      _networkManager->clearBusDataReady();
    }

    // If still invalid, keep waiting (splash screen remains)
    if (_cachedBusMinutes < 0) {
      return;
    }
  }

  // If we get here: Minimum time passed AND we have valid data
  _currentState = STANDBY;
  _display->clearContentArea();
  // _lastBusUpdate was just updated if we fetched, so Standby won't refetch
  // immediately.
}
