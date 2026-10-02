#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "BusClient.h"
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

class StateManager {
public:
  StateManager(DistanceSensor *sensor, DisplayManager *display);
  ~StateManager();
  void update();

private:
  DistanceSensor *_sensor;
  DisplayManager *_display;
  BusClient *_busClient;
  WledClient *_wledClient;
  NetworkManager *_networkManager;

  AppState _currentState;

  // Selection Logic
  int _currentZoneIdx;
  int _lastZoneIdx;
  unsigned long _zoneStartTime;

  // Standby & Power Logic
  unsigned long _lastInteractionTime;
  unsigned long _lastBusUpdate;
  unsigned long _busEffectStartTime;
  unsigned long _successStartTime;
  unsigned long _errorDisplayTimer;
  unsigned long _ignoreInputUntil;
  int _cachedBusMinutes = -1;

  // Unlock Logic
  unsigned long _unlockStartTime;
  bool _unlocking;

  // Timer State
  unsigned long _timerStartTime;
  bool _timerRunning;
  bool _timerMenuVisible;

  // Helpers
  int calculateZone(float dist, int numItems);
  void handleSelection(int index);
  void resetSelectionTimer();
  void resetStandbyTimer();
  void checkStandbyTimeout();

  // Actions
  void actionMainMenu(int index);
  void actionTimer(int index);
  void actionLights(int index);
  void actionBusTimes(int index);

  // State Handlers
  void updateWarmup();
  void updateStandby();
  void updateUnlock();
};

#endif
