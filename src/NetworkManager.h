#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "Config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

class BusClient;
class WledClient;

enum NetworkAction {
  ACT_FETCH_BUS,
  ACT_WLED_RAINBOW,
  ACT_WLED_COFFEE,
  ACT_WLED_LOADING,
  ACT_WLED_BUS_TIME,
  ACT_WLED_SAVE,
  ACT_WLED_RESTORE
};

struct NetworkCommand {
  NetworkAction action;
  int param; // Used for bus minutes
};

class NetworkManager {
public:
  NetworkManager(BusClient *busClient, WledClient *wledClient);
  void begin();

  // Queue requests (Non-blocking). A bus fetch is skipped if one is already
  // queued or running.
  void requestBusUpdate();
  void requestWledRainbow();
  void requestWledCoffee();
  void requestWledLoading();
  void requestWledBusTime(int minutes);
  void requestWledSaveState();
  void requestWledRestoreState();

  // Retrieve results. Returns true (once) when a new fetch has finished.
  // seconds is time to the next bus or a negative BUS_* code; completedAt is
  // the millis() when the result came back.
  bool takeBusResult(int &seconds, unsigned long &completedAt, char *line,
                     size_t lineLen);
  bool isBusFetchPending();

private:
  BusClient *_busClient;
  WledClient *_wledClient;

  QueueHandle_t _cmdQueue;
  portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;

  // Guarded by _lock (written on core 0, read on core 1)
  bool _busDataReady;
  bool _busFetchPending;
  int _latestBusSeconds;
  unsigned long _latestBusAt;
  char _latestLine[MAX_LINE_NAME_LENGTH + 1];

  void enqueue(NetworkAction action, int param = 0);
  static void networkTask(void *parameter);
};

#endif
