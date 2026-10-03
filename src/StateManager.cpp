#include "StateManager.h"
#include <WiFi.h>
#include <math.h>
#include <time.h>

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
  _disarmDist = 0;

  _lastInteractionTime = millis();
  _busEffectStartTime = 0;
  _wledOwner = WLED_NONE;
  _lastPresenceTime = millis();
  _leaveAlertActive = false;

  _moveRefDist = -1;
  _lastMoveAt = millis();

  _lastHealthCheck = millis();
  _heapLow = false;
  _lowHeapSince = 0;
  _lastBusOkAt = millis();

  _busSeconds = BUS_LOADING;
  _busSecond = -1;
  _busAt = 0;
  _busLine[0] = '\0';
  _lastBusResultOk = false;
  _busFailCount = 0;
  _busEverRequested = false;
  _lastBusRequest = 0;

  _timerStartTime = 0;
  _timerRunning = false;
  _timerMenuVisible = false;
  _timerMenuArmed = false;
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
  // _rawZone is kept: every screen shares the same zone geometry, so the
  // hysteresis that held the hand in its row should carry over too
  disarm();
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
  _disarmDist = _sensor ? _sensor->getSmoothedDistance() : 0;
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
  trackMovement(dist);

  checkHealth();
  pollBusResult();

  if (_currentState != BOOT_WARMUP) {
    maintainBusData();
    updateLeaveAlert();
  }
  updateDisplaySleep(dist);

  checkStandbyTimeout();

  AppState tickState = _currentState;
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

  // Base strip and cursor go after the screen draw, and wait a tick after a
  // state change so the new screen has set up its layout (and cursor gutter)
  if (isInteractive(_currentState) && _currentState == tickState) {
    bool baseActive =
        (dist > BASE_ACTIVE_MIN_DISTANCE && dist < BASE_ACTIVE_MAX_DISTANCE);
    _display->updateBaseZone(baseActive);
    _display->setHandCursor(dist);
  }

  // Hand the lights back after the bus colour has been shown for a while
  if (_wledOwner == WLED_BUS &&
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
    break;
  default:
    break;
  }

  int zone = calculateZone(dist, numZones, _rawZone);
  _rawZone = zone;
  int row = (zone >= firstRow) ? zone : ROW_NONE;

  // Interaction Tracking
  if (handPresent(dist))
    resetStandbyTimer();

  // Re-arm once the hand leaves, or genuinely moves to a different row.
  // The distance check stops a hand resting near a boundary from drifting
  // across it and re-arming without the user meaning to move.
  if (!_armed) {
    bool arm = false;
    if (_rearmRow == ROW_UNSET)
      _rearmRow = row;
    else if (row == ROW_NONE && _rearmRow != ROW_NONE)
      arm = true;
    else if (row != _rearmRow && fabsf(dist - _disarmDist) > REARM_DISTANCE_CM)
      arm = true;
    if (arm) {
      // The dwell counts from the moment of arming, never from earlier
      // time spent on the row while disarmed
      _armed = true;
      _zoneStartTime = now;
    }
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
    takeBackLights();
    _networkManager->requestWledRainbow();
    goToState(SHOWING_SUCCESS);
    _display->drawSuccessScreen("Rainbow!");
  } else if (row == 1) { // Coffee (warm white)
    takeBackLights();
    _networkManager->requestWledCoffee();
    goToState(SHOWING_SUCCESS);
    _display->drawSuccessScreen("Coffee");
  } else if (row == 2) { // Back
    goToState(MAIN_MENU);
  }
}

void StateManager::actionBusTimes(int row) {
  if (row == 1) { // Refresh
    if (startBusEffect())
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

// Keeps bus data fresh in every state (after boot), so the countdown and the
// leave-now alert stay accurate whichever screen is up
void StateManager::maintainBusData() {
  unsigned long interval;
  if (!_lastBusResultOk) {
    // Back off while it keeps failing (TfL down, no internet), so the
    // network task isn't tied up retrying and TfL isn't hammered
    interval = BUS_ERROR_RETRY_MS;
    for (uint8_t i = 1; i < _busFailCount && interval < BUS_ERROR_MAX_RETRY_MS;
         i++)
      interval *= 2;
    if (interval > BUS_ERROR_MAX_RETRY_MS)
      interval = BUS_ERROR_MAX_RETRY_MS;
  } else {
    interval = busNearLeaveWindow() ? LEAVE_REFRESH_MS : BUS_UPDATE_INTERVAL_MS;
  }
  if (_currentState == BUS_TIMES && interval > BUS_SCREEN_REFRESH_MS)
    interval = BUS_SCREEN_REFRESH_MS;
  if (!_busEverRequested || millis() - _lastBusRequest > interval)
    requestBus();
}

void StateManager::pollBusResult() {
  int seconds, second;
  unsigned long at;
  char line[MAX_LINE_NAME_LENGTH + 1];
  if (!_networkManager->takeBusResult(seconds, second, at, line, sizeof(line)))
    return;

  applyBusResult(seconds, second, at, line);

  if (_currentState != BUS_TIMES)
    return;

  int m = busMinutes();
  if (m == BUS_API_ERROR || m == BUS_OFFLINE) {
    // Nothing usable to show
    goToState(SHOWING_API_ERROR);
    _display->drawErrorScreen(m == BUS_OFFLINE ? "No WiFi" : "API Error");
    return;
  }

  if (_wledOwner == WLED_BUS) {
    if (m >= 0)
      _networkManager->requestWledBusTime(m);
    else
      endBusEffect(); // No buses: nothing to colour, give the lights back
  }
}

void StateManager::applyBusResult(int seconds, int second, unsigned long at,
                                  const char *line) {
  _lastBusResultOk = (seconds >= 0 || seconds == BUS_NO_BUSES);
  if (_lastBusResultOk) {
    _busFailCount = 0;
    _lastBusOkAt = millis();
  } else if (_busFailCount < 255) {
    _busFailCount++;
  }

  // A failed refresh shouldn't wipe good data: keep counting down what we
  // had until it's too old to trust
  bool haveRecentData = _busSeconds >= 0 && millis() - _busAt < BUS_STALE_MS;
  if (!_lastBusResultOk && haveRecentData) {
    Serial.printf("[Bus] Refresh failed (%d), keeping last data\n", seconds);
    return;
  }

  _busSeconds = seconds;
  _busSecond = (seconds >= 0) ? second : -1;
  _busAt = at;
  strncpy(_busLine, line, sizeof(_busLine) - 1);
  _busLine[sizeof(_busLine) - 1] = '\0';
}

// Minutes until the next bus, counted down locally since the last fetch
int StateManager::busMinutes() {
  if (_busSeconds < 0)
    return _busSeconds;
  return remainingSeconds(_busSeconds) / 60;
}

// Seconds left for a bus that was fetchedSeconds away at the last fetch
// (never negative), or -1 if there's no such bus
long StateManager::remainingSeconds(int fetchedSeconds) {
  if (fetchedSeconds < 0)
    return -1;
  long left = (long)fetchedSeconds - (long)((millis() - _busAt) / 1000UL);
  return left < 0 ? 0 : left;
}

BusView StateManager::busView() {
  BusView v;
  v.minutes = busMinutes();
  v.line = _busLine;
  v.refreshing = _networkManager->isBusFetchPending();
  v.online = (WiFi.status() == WL_CONNECTED);
  long second = remainingSeconds(_busSecond);
  v.secondMinutes = (_busSeconds >= 0 && second >= 0) ? second / 60 : -1;
  v.leaveNow = _leaveAlertActive;
  v.alertMinutes = _leaveAlertActive ? leaveAlertMinutes() : -1;
  return v;
}

void StateManager::enterBusScreen() {
  goToState(BUS_TIMES);

  // Recent data shows instantly (and colours the lights straight away);
  // otherwise show the loading animation while we fetch
  if (startBusEffect()) {
    if (_busSeconds >= 0 && millis() - _busAt < BUS_FRESH_MS)
      _networkManager->requestWledBusTime(busMinutes());
    else
      _networkManager->requestWledLoading();
  }

  requestBus();
}

// Returns false if the leave-now alert has the lights (it wins)
bool StateManager::startBusEffect() {
  if (_wledOwner == WLED_LEAVE)
    return false;
  if (_wledOwner == WLED_NONE)
    _networkManager->requestWledSaveState();
  _wledOwner = WLED_BUS;
  _busEffectStartTime = millis();
  return true;
}

void StateManager::endBusEffect() {
  if (_wledOwner != WLED_BUS)
    return;
  _networkManager->requestWledRestoreState();
  _wledOwner = WLED_NONE;
}

// A manual Lights choice wins over any running effect: drop the effect's
// claim on the strip and its saved state, so nothing "restores" over it later.
// Always discard, even with no effect running: WledClient keeps a state whose
// restore failed, and that copy would otherwise resurface after this choice.
void StateManager::takeBackLights() {
  _networkManager->requestWledDiscardSaved();
  _wledOwner = WLED_NONE;
}

// --- Leave-now alert ---

bool StateManager::inLeaveAlertHours() {
  // Read the clock directly: getLocalTime() sleeps 10ms when unsynced
  time_t now = time(nullptr);
  if (now < NTP_VALID_EPOCH)
    return false; // NTP hasn't synced yet
  struct tm t;
  localtime_r(&now, &t);
  if (!(LEAVE_ALERT_DAYS & (1 << t.tm_wday)))
    return false;
  int hhmm = t.tm_hour * 100 + t.tm_min;
  return hhmm >= LEAVE_ALERT_START_HHMM && hhmm < LEAVE_ALERT_END_HHMM;
}

// Minutes to the first of the next two buses that leaving now still
// catches, or -1 if neither is in the window
int StateManager::leaveAlertMinutes() {
  if (_busSeconds < 0 || millis() - _busAt > BUS_STALE_MS)
    return -1;
  long buses[2] = {remainingSeconds(_busSeconds), remainingSeconds(_busSecond)};
  for (int i = 0; i < 2; i++) {
    if (buses[i] >= LEAVE_WINDOW_MIN_MIN * 60L &&
        buses[i] <= LEAVE_WINDOW_MAX_MIN * 60L)
      return buses[i] / 60;
  }
  return -1;
}

// A bus is a few minutes from the window: worth refreshing more often
bool StateManager::busNearLeaveWindow() {
  if (_busSeconds < 0 || !inLeaveAlertHours())
    return false;
  long buses[2] = {remainingSeconds(_busSeconds), remainingSeconds(_busSecond)};
  for (int i = 0; i < 2; i++) {
    if (buses[i] >= LEAVE_WINDOW_MIN_MIN * 60L &&
        buses[i] <= (LEAVE_WINDOW_MAX_MIN + LEAVE_REFRESH_LEAD_MIN) * 60L)
      return true;
  }
  return false;
}

void StateManager::updateLeaveAlert() {
  bool want = leaveAlertMinutes() >= 0 && inLeaveAlertHours();
  if (want == _leaveAlertActive)
    return;
  _leaveAlertActive = want;

  if (want) {
    Serial.println("[Alert] Leave now");
    if (_wledOwner == WLED_NONE)
      _networkManager->requestWledSaveState();
    _wledOwner = WLED_LEAVE; // Takes over from the bus effect if running
    _networkManager->requestWledLeaveNow();
  } else if (_wledOwner == WLED_LEAVE) {
    _networkManager->requestWledRestoreState();
    _wledOwner = WLED_NONE;
  }
}

// --- Display sleep ---

void StateManager::updateDisplaySleep(float dist) {
  // Anything going on keeps the screen lit: a hand nearby, any screen
  // other than standby, or a leave-now alert
  if (handPresent(dist) || _leaveAlertActive || _currentState != STANDBY)
    _lastPresenceTime = millis();
  _display->setAwake(millis() - _lastPresenceTime < DISPLAY_SLEEP_MS);
}

// --- Presence & health ---

void StateManager::trackMovement(float dist) {
  if (_moveRefDist < 0 || fabsf(dist - _moveRefDist) > MOVE_THRESHOLD_CM) {
    _moveRefDist = dist;
    _lastMoveAt = millis();
  }
}

// Something within reach that has moved recently. A hand always moves; a
// mug or kettle parked in front of the sensor doesn't, so it stops keeping
// the screen awake (or re-unlocking it) after STATIC_OBJECT_MS
bool StateManager::handPresent(float dist) {
  return dist >= 0 && dist < BASE_ACTIVE_MAX_DISTANCE &&
         millis() - _lastMoveAt < STATIC_OBJECT_MS;
}

// Once a minute: log heap and stack so slow problems are visible on serial,
// and restart cleanly if the device is stuck in a way it can't recover from
// by itself. Only restarts while idle in standby, so nobody's mid-use and
// no light effect is lost.
void StateManager::checkHealth() {
  unsigned long now = millis();
  if (now - _lastHealthCheck < HEALTH_CHECK_INTERVAL_MS)
    return;
  _lastHealthCheck = now;

  uint32_t maxBlock = ESP.getMaxAllocHeap();
  Serial.printf("[Health] heap free %u, largest block %u, lowest %u; stack "
                "free: loop %u, network %u\n",
                ESP.getFreeHeap(), maxBlock, ESP.getMinFreeHeap(),
                (unsigned)uxTaskGetStackHighWaterMark(NULL),
                _networkManager->stackFreeBytes());

  if (maxBlock < HEALTH_MIN_HEAP_BLOCK) {
    if (!_heapLow) {
      _heapLow = true;
      _lowHeapSince = now;
    }
  } else {
    _heapLow = false;
  }

  const char *reason = nullptr;
  if (_networkManager->busyForMs() > HEALTH_NET_STUCK_MS)
    reason = "network task stuck";
  else if (_heapLow && now - _lowHeapSince > HEALTH_LOW_HEAP_RESTART_MS)
    reason = "heap too fragmented for TLS";
  else if (WiFi.status() == WL_CONNECTED &&
           now - _lastBusOkAt > HEALTH_NO_DATA_RESTART_MS)
    reason = "no TfL data for 2 hours";
  if (!reason)
    return;

  bool idle = _currentState == STANDBY && _wledOwner == WLED_NONE &&
              !_leaveAlertActive;
  if (!idle)
    return; // Try again next check

  Serial.printf("[Health] Restarting: %s\n", reason);
  Serial.flush();
  ESP.restart();
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
  // Unlock Trigger (a hand, not something left sitting on the sensor)
  if (dist < UNLOCK_DISTANCE_CM && handPresent(dist)) {
    goToState(UNLOCK_SEQUENCE);
    return;
  }

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
