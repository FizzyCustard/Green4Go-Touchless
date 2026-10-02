#include "StateManager.h"
#include <WiFi.h>

#define ROW_NONE -1
#define ROW_UNSET -2 // Disarmed, waiting for the first reading on a screen

StateManager::StateManager(DistanceSensor *sensor, DisplayManager *display) {
  _busClient = nullptr;
  _wledClient = nullptr;
  _networkManager = nullptr;

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
  _stateEnteredAt = millis();

  _rawZone = ROW_NONE;
  _lastZoneIdx = ROW_NONE;
  _zoneStartTime = 0;
  _armed = false;
  _rearmRow = ROW_UNSET;

  _lastInteractionTime = millis();
  _busEffectStartTime = 0;

  _busSeconds = BUS_LOADING;
  _busAt = 0;
  _busLine[0] = '\0';
  _lastBusResultOk = false;
  _busEverRequested = false;
  _lastBusRequest = 0;

  _timerStartTime = 0;
  _timerRunning = false;
  _timerMenuVisible = false;
  _timerMenuArmed = false;
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

// --- State plumbing ---

bool StateManager::isInteractive(AppState s) {
  return s == MAIN_MENU || s == TIMER_SCREEN || s == BUS_TIMES ||
         s == LIGHT_SETTINGS || s == SHOWING_SUCCESS || s == SHOWING_API_ERROR;
}

// Every screen change goes through here so cleanup can't be forgotten
void StateManager::goToState(AppState s) {
  AppState old = _currentState;
  _currentState = s;
  _stateEnteredAt = millis();

  // The hand is probably still where it was when it triggered this change,
  // so it must move before anything on the new screen can be selected
  disarm();
  _rawZone = ROW_NONE;
  _lastZoneIdx = ROW_NONE;
  resetStandbyTimer();

  if (old == BUS_TIMES && s != BUS_TIMES)
    endBusEffect(); // Always give the lights back
  if (old == TIMER_SCREEN && s != TIMER_SCREEN)
    _timerRunning = false;
}

void StateManager::disarm() {
  _armed = false;
  _rearmRow = ROW_UNSET;
}

void StateManager::resetStandbyTimer() { _lastInteractionTime = millis(); }

void StateManager::checkStandbyTimeout() {
  if (!isInteractive(_currentState))
    return;

  // A running timer keeps the screen awake (it's the thing being watched)
  unsigned long limit = STANDBY_TIMEOUT_MS;
  if (_currentState == TIMER_SCREEN && _timerRunning)
    limit = TIMER_AWAKE_MAX_MS;

  if (millis() - _lastInteractionTime > limit) {
    goToState(STANDBY);
    // Refresh straight away if what we have is getting old
    if (millis() - _busAt > BUS_FRESH_MS)
      requestBus();
  }
}

// --- Main tick ---

void StateManager::update() {
  // Validate components before use
  if (!_sensor || !_display) {
    return;
  }

  float dist = _sensor->getSmoothedDistance();

  pollBusResult();

  if (isInteractive(_currentState)) {
    bool baseActive =
        (dist > BASE_ACTIVE_MIN_DISTANCE && dist < BASE_ACTIVE_MAX_DISTANCE);
    _display->updateBaseZone(baseActive);
  }

  checkStandbyTimeout();

  switch (_currentState) {
  case BOOT_WARMUP:
    updateWarmup();
    break;

  case STANDBY:
    updateStandby(dist);
    break;

  case UNLOCK_SEQUENCE:
    updateUnlock(dist);
    break;

  case MAIN_MENU:
  case TIMER_SCREEN:
  case BUS_TIMES:
  case LIGHT_SETTINGS:
    updateInteractive(dist);
    break;

  case SHOWING_SUCCESS:
    if (millis() - _stateEnteredAt > SUCCESS_DISPLAY_TIME_MS)
      goToState(MAIN_MENU);
    break;

  case SHOWING_API_ERROR:
    if (millis() - _stateEnteredAt > ERROR_DISPLAY_TIME_MS)
      goToState(MAIN_MENU);
    break;
  }

  // Hand the lights back after the bus colour has been shown for a while
  if (_busEffectStartTime > 0 &&
      millis() - _busEffectStartTime > BUS_EFFECT_TIMEOUT_MS) {
    endBusEffect();
  }
}

// --- Zones & selection ---

// Splits the usable range into numZones bands. Once the hand is in a zone it
// stays there until it moves ZONE_HYSTERESIS_CM past the edge, so jitter on
// a boundary doesn't flicker the highlight or restart the dwell.
int StateManager::calculateZone(float dist, int numZones, int prevZone) {
  if (numZones <= 0 || dist < 0)
    return ROW_NONE;

  float zoneSize = MAX_INTERACTION_DISTANCE / numZones;

  if (prevZone >= 0 && prevZone < numZones) {
    float lo = prevZone * zoneSize - ZONE_HYSTERESIS_CM;
    float hi = (prevZone + 1) * zoneSize + ZONE_HYSTERESIS_CM;
    if (dist >= lo && dist <= hi)
      return prevZone;
  }

  if (dist > MAX_INTERACTION_DISTANCE)
    return ROW_NONE;

  int zone = (int)(dist / zoneSize);
  if (zone >= numZones)
    zone = numZones - 1;
  if (zone < 0)
    zone = 0;
  return zone;
}

// Opens the timer menu when the hand arrives in the base zone, closes it
// when the hand leaves. Returns true if it changed this tick.
bool StateManager::updateTimerMenu(float dist) {
  bool baseActive =
      (dist > BASE_ACTIVE_MIN_DISTANCE && dist < BASE_ACTIVE_MAX_DISTANCE);

  // The hand that picked "Timer" passes through the base zone on its way
  // out; don't treat that as a request to open the menu
  if (!_timerMenuArmed && dist > TIMER_MENU_CLOSE_DISTANCE)
    _timerMenuArmed = true;

  if (!_timerMenuVisible && _timerMenuArmed && baseActive) {
    _timerMenuVisible = true;
    return true;
  }
  if (_timerMenuVisible && dist > TIMER_MENU_CLOSE_DISTANCE) {
    _timerMenuVisible = false;
    return true;
  }
  return false;
}

void StateManager::updateInteractive(float dist) {
  unsigned long now = millis();

  // Each screen is three 5cm zones that line up with the three 80px rows.
  // firstRow skips a non-selectable top row (timer / bus info).
  int numZones = 3;
  int firstRow = 0;
  switch (_currentState) {
  case TIMER_SCREEN: {
    bool toggled = updateTimerMenu(dist);
    bool menuReady = _timerMenuVisible && !toggled && !_display->isAnimating();
    numZones = menuReady ? 3 : 0;
    firstRow = 1;
    break;
  }
  case BUS_TIMES:
    firstRow = 1;
    if (now - _lastBusRequest > BUS_SCREEN_REFRESH_MS)
      requestBus();
    break;
  default:
    break;
  }

  int zone = calculateZone(dist, numZones, _rawZone);
  _rawZone = zone;
  int row = (zone >= firstRow) ? zone : ROW_NONE;

  // Interaction Tracking
  if (dist < BASE_ACTIVE_MAX_DISTANCE)
    resetStandbyTimer();

  // Re-arm once the hand moves to a different row (or away)
  if (!_armed) {
    if (_rearmRow == ROW_UNSET)
      _rearmRow = row;
    else if (row != _rearmRow)
      _armed = true;
  }

  // Dwell tracking
  if (row != _lastZoneIdx) {
    _lastZoneIdx = row;
    _zoneStartTime = now;
  }
  float progress = 0;
  if (_armed && row >= 0)
    progress = (now - _zoneStartTime) / (float)DWELL_THRESHOLD_MS;

  // Draw
  switch (_currentState) {
  case MAIN_MENU:
    _display->drawMainMenu(row);
    break;
  case TIMER_SCREEN:
    _display->drawTimerScreen(row, _timerMenuVisible);
    if (_timerRunning)
      _display->drawTimerValue(now - _timerStartTime);
    break;
  case LIGHT_SETTINGS:
    _display->drawLightSettings(row);
    break;
  case BUS_TIMES:
    _display->drawBusScreen(busView(), row);
    break;
  default:
    break;
  }
  _display->drawDwellProgress(row, progress);

  if (progress >= 1.0f)
    handleSelection(row);
}

void StateManager::handleSelection(int row) {
  switch (_currentState) {
  case MAIN_MENU:
    actionMainMenu(row);
    break;
  case TIMER_SCREEN:
    actionTimer(row);
    break;
  case LIGHT_SETTINGS:
    actionLights(row);
    break;
  case BUS_TIMES:
    actionBusTimes(row);
    break;
  default:
    break;
  }
  disarm(); // One selection per visit to a row
}

// --- Actions ---

void StateManager::actionMainMenu(int row) {
  if (row == 0) { // Timer
    goToState(TIMER_SCREEN);
    _timerStartTime = millis();
    _timerRunning = true;
    _timerMenuVisible = false;
    _timerMenuArmed = false;
  } else if (row == 1) { // Bus Times
    enterBusScreen();
  } else if (row == 2) { // Lights
    goToState(LIGHT_SETTINGS);
  }
}

void StateManager::actionTimer(int row) {
  if (row == 1) { // Reset
    _timerStartTime = millis();
  } else if (row == 2) { // Back
    goToState(MAIN_MENU);
  }
}

void StateManager::actionLights(int row) {
  if (row == 0) { // Rainbow
    _networkManager->requestWledRainbow();
    goToState(SHOWING_SUCCESS);
    _display->drawSuccessScreen("Rainbow!");
  } else if (row == 1) { // Solid (Now Coffee)
    _networkManager->requestWledCoffee();
    goToState(SHOWING_SUCCESS);
    _display->drawSuccessScreen("Coffee");
  } else if (row == 2) { // Back
    goToState(MAIN_MENU);
  }
}

void StateManager::actionBusTimes(int row) {
  if (row == 1) { // Refresh
    startBusEffect();
    _networkManager->requestWledLoading();
    requestBus();
  } else if (row == 2) { // Back
    goToState(MAIN_MENU);
  }
}

// --- Bus data ---

void StateManager::requestBus() {
  _lastBusRequest = millis();
  _busEverRequested = true;
  _networkManager->requestBusUpdate();
}

void StateManager::pollBusResult() {
  int seconds;
  unsigned long at;
  char line[MAX_LINE_NAME_LENGTH + 1];
  if (!_networkManager->takeBusResult(seconds, at, line, sizeof(line)))
    return;

  applyBusResult(seconds, at, line);

  if (_currentState != BUS_TIMES)
    return;

  int m = busMinutes();
  if (m == BUS_API_ERROR || m == BUS_OFFLINE) {
    // Nothing usable to show
    goToState(SHOWING_API_ERROR);
    _display->drawErrorScreen(m == BUS_OFFLINE ? "No WiFi" : "API Error");
    return;
  }

  if (_busEffectStartTime > 0) {
    if (m >= 0)
      _networkManager->requestWledBusTime(m);
    else
      endBusEffect(); // No buses: nothing to colour, give the lights back
  }
}

void StateManager::applyBusResult(int seconds, unsigned long at,
                                  const char *line) {
  _lastBusResultOk = (seconds >= 0 || seconds == BUS_NO_BUSES);

  // A failed refresh shouldn't wipe good data: keep counting down what we
  // had until it's too old to trust
  bool haveRecentData = _busSeconds >= 0 && millis() - _busAt < BUS_STALE_MS;
  if (!_lastBusResultOk && haveRecentData) {
    Serial.printf("[Bus] Refresh failed (%d), keeping last data\n", seconds);
    return;
  }

  _busSeconds = seconds;
  _busAt = at;
  strncpy(_busLine, line, sizeof(_busLine) - 1);
  _busLine[sizeof(_busLine) - 1] = '\0';
}

// Minutes until the next bus, counted down locally since the last fetch
int StateManager::busMinutes() {
  if (_busSeconds < 0)
    return _busSeconds;
  long remainingMs = (long)_busSeconds * 1000L - (long)(millis() - _busAt);
  if (remainingMs <= 0)
    return 0;
  return remainingMs / 60000L;
}

BusView StateManager::busView() {
  BusView v;
  v.minutes = busMinutes();
  v.line = _busLine;
  v.refreshing = _networkManager->isBusFetchPending();
  v.online = (WiFi.status() == WL_CONNECTED);
  return v;
}

void StateManager::enterBusScreen() {
  goToState(BUS_TIMES);
  startBusEffect();

  // Recent data shows instantly (and colours the lights straight away);
  // otherwise show the loading animation while we fetch
  if (_busSeconds >= 0 && millis() - _busAt < BUS_FRESH_MS)
    _networkManager->requestWledBusTime(busMinutes());
  else
    _networkManager->requestWledLoading();

  requestBus();
}

void StateManager::startBusEffect() {
  if (_busEffectStartTime == 0)
    _networkManager->requestWledSaveState();
  _busEffectStartTime = millis();
  if (_busEffectStartTime == 0)
    _busEffectStartTime = 1; // 0 means "inactive"
}

void StateManager::endBusEffect() {
  if (_busEffectStartTime == 0)
    return;
  _networkManager->requestWledRestoreState();
  _busEffectStartTime = 0;
}

// --- Passive states ---

void StateManager::updateWarmup() {
  _display->drawBootScreen();
  unsigned long now = millis();

  // Fetch as soon as WiFi is up, retrying every few seconds
  if (WiFi.status() == WL_CONNECTED &&
      (!_busEverRequested || now - _lastBusRequest > BUS_BOOT_RETRY_MS)) {
    requestBus();
  }

  if (now < SPLASH_MIN_MS)
    return;

  // Wait for a real answer ("no buses" counts), but never forever: at night,
  // offline or with TfL down we still need to get to the standby screen
  bool haveAnswer = (_busSeconds >= 0 || _busSeconds == BUS_NO_BUSES);
  if (!haveAnswer && now < SPLASH_MAX_MS)
    return;

  goToState(STANDBY);
}

void StateManager::updateStandby(float dist) {
  // Unlock Trigger
  if (dist < UNLOCK_DISTANCE_CM) {
    goToState(UNLOCK_SEQUENCE);
    return;
  }

  // Refresh periodically, sooner if the last attempt failed
  unsigned long interval =
      _lastBusResultOk ? BUS_UPDATE_INTERVAL_MS : BUS_ERROR_RETRY_MS;
  if (!_busEverRequested || millis() - _lastBusRequest > interval)
    requestBus();

  _display->drawStandbyScreen(busView());
}

void StateManager::updateUnlock(float dist) {
  if (dist > UNLOCK_DISTANCE_CM + UNLOCK_HYSTERESIS_CM) { // Hysteresis
    goToState(STANDBY);
    _display->drawStandbyScreen(busView());
    return;
  }

  float progress = (float)(millis() - _stateEnteredAt) / UNLOCK_TIME_MS;
  _display->drawUnlockVisual(progress);

  if (progress >= 1.0f)
    goToState(MAIN_MENU);
}
