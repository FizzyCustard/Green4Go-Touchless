#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

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

  // Queue requests (Non-blocking)
  void requestBusUpdate();
  void requestWledRainbow();
  void requestWledCoffee();
  void requestWledLoading();
  void requestWledBusTime(int minutes);
  void requestWledSaveState();
  void requestWledRestoreState();

  // Retrieve results
  bool isBusDataReady();
  int getLatestBusMinutes();
  void clearBusDataReady();

private:
  BusClient *_busClient;
  WledClient *_wledClient;

  QueueHandle_t _cmdQueue;

  volatile bool _busDataReady;
  volatile int _latestBusMinutes;

  static void networkTask(void *parameter);
};

#endif
