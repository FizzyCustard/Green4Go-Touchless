#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "BusClient.h"
#include "Config.h"
#include "DisplayManager.h"
#include "DistanceSensor.h"
#include "NetworkManager.h"
#include "WledClient.h"

enum AppState {
  BOOT_WARMUP,
  STANDBY,
  UNLOCK_SEQUENCE,
  MAIN_MENU,
  TIMER_SCREEN,
  BUS_TIMES,
  LIGHT_SETTINGS,
  SHOWING_SUCCESS,
  SHOWING_API_ERROR
};

// Who currently "owns" the WLED strip. Whoever takes it from NONE saves the
// user's state first; handing back to NONE restores it.
enum WledOwner { WLED_NONE, WLED_BUS, WLED_LEAVE };

class StateManager {
public:
  // Lives for the whole run (the network task holds pointers into it), so
  // there is deliberately no destructor
  StateManager(DistanceSensor *sensor, DisplayManager *display);
  void update();

private:
  DistanceSensor *_sensor;
  DisplayManager *_display;
  BusClient *_busClient;
  WledClient *_wledClient;
  NetworkManager *_networkManager;

  AppState _currentState;
  unsigned long _stateEnteredAt;

  // Selection Logic
  int _rawZone;                // Zone under the hand last tick (hysteresis)
  int _lastZoneIdx;            // Row being dwelled on
  unsigned long _zoneStartTime;
  bool _armed;                 // Cleared by a selection until the hand moves
  int _rearmRow;               // Row the hand was on when disarmed
  float _disarmDist;           // Hand distance when disarmed

  // Standby & Power Logic
  unsigned long _lastInteractionTime;
  unsigned long _busEffectStartTime; // When the bus colour effect started
  WledOwner _wledOwner;
  unsigned long _lastPresenceTime; // For display sleep
  bool _leaveAlertActive;

  // Hand vs static object: something that hasn't moved for STATIC_OBJECT_MS
  // (a mug, the kettle) stops counting as someone being there
  float _moveRefDist;
  unsigned long _lastMoveAt;

  // Health monitor
  unsigned long _lastHealthCheck;
  unsigned long _lowHeapSince; // 0 = heap fine
  unsigned long _lastBusOkAt;

  // Bus data. Stored as "seconds at fetch time" so the display can count
  // down locally between fetches.
  int _busSeconds;          // >= 0 valid, otherwise a BUS_* code
  int _busSecond;           // Seconds to the bus after that, or -1
  unsigned long _busAt;     // millis() when _busSeconds was fetched
  char _busLine[MAX_LINE_NAME_LENGTH + 1];
  bool _lastBusResultOk;
  uint8_t _busFailCount; // Consecutive failed fetches, for backoff
  bool _busEverRequested;
  unsigned long _lastBusRequest;

  // Timer State
  unsigned long _timerStartTime;
  bool _timerRunning;
  bool _timerMenuVisible;
  bool _timerMenuArmed; // Hand must leave once before the menu can open

  // Helpers
  static bool isInteractive(AppState s);
  void goToState(AppState s);
  int calculateZone(float dist, int numZones, int prevZone);
  void disarm();
  void resetStandbyTimer();
  void checkStandbyTimeout();
  void handleSelection(int row);

  // Bus helpers
  void requestBus();
  void maintainBusData();
  void pollBusResult();
  void applyBusResult(int seconds, int second, unsigned long at,
                      const char *line);
  int busMinutes();
  long remainingSeconds(int fetchedSeconds);
  BusView busView();
  void enterBusScreen();
  bool startBusEffect();
  void endBusEffect();
  void takeBackLights();

  // Leave-now alert & display sleep
  bool inLeaveAlertHours();
  int leaveAlertMinutes();
  bool busNearLeaveWindow();
  void updateLeaveAlert();
  void updateDisplaySleep(float dist);

  // Presence & health
  void trackMovement(float dist);
  bool handPresent(float dist);
  void checkHealth();

  // Actions
  void actionMainMenu(int row);
  void actionTimer(int row);
  void actionLights(int row);
  void actionBusTimes(int row);

  // State Handlers
  void updateWarmup();
  void updateStandby(float dist);
  void updateUnlock(float dist);
  void updateInteractive(float dist);
  bool updateTimerMenu(float dist);
};

#endif
