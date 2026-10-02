#include "NetworkManager.h"
#include "BusClient.h"
#include "WledClient.h"

NetworkManager::NetworkManager(BusClient *busClient, WledClient *wledClient) {
  _busClient = busClient;
  _wledClient = wledClient;
  _busDataReady = false;
  _latestBusMinutes = -1;
}

void NetworkManager::begin() {
  _cmdQueue = xQueueCreate(10, sizeof(NetworkCommand));

  // Run on Core 0 (Networking core)
  xTaskCreatePinnedToCore(NetworkManager::networkTask, "NetworkTask",
                          16384, // Stack size increased to 16KB for HTTP safety
                          this,  // Parameter
                          1,     // Priority
                          NULL,  // Task handle
                          0      // Core 0
  );
}

// --- Request Enqueueing ---

void NetworkManager::requestBusUpdate() {
  NetworkCommand cmd = {ACT_FETCH_BUS, 0};
  xQueueSend(_cmdQueue, &cmd, 0);
}

void NetworkManager::requestWledRainbow() {
  NetworkCommand cmd = {ACT_WLED_RAINBOW, 0};
  xQueueSend(_cmdQueue, &cmd, 0);
}

void NetworkManager::requestWledCoffee() {
  NetworkCommand cmd = {ACT_WLED_COFFEE, 0};
  xQueueSend(_cmdQueue, &cmd, 0);
}

void NetworkManager::requestWledLoading() {
  NetworkCommand cmd = {ACT_WLED_LOADING, 0};
  xQueueSend(_cmdQueue, &cmd, 0);
}

void NetworkManager::requestWledBusTime(int minutes) {
  NetworkCommand cmd = {ACT_WLED_BUS_TIME, minutes};
  xQueueSend(_cmdQueue, &cmd, 0);
}

void NetworkManager::requestWledSaveState() {
  NetworkCommand cmd = {ACT_WLED_SAVE, 0};
  xQueueSend(_cmdQueue, &cmd, 0);
}

void NetworkManager::requestWledRestoreState() {
  NetworkCommand cmd = {ACT_WLED_RESTORE, 0};
  xQueueSend(_cmdQueue, &cmd, 0);
}

// --- Status Retrieval ---

bool NetworkManager::isBusDataReady() { return _busDataReady; }

int NetworkManager::getLatestBusMinutes() { return _latestBusMinutes; }

void NetworkManager::clearBusDataReady() { _busDataReady = false; }

// --- Background Task ---

void NetworkManager::networkTask(void *parameter) {
  NetworkManager *self = (NetworkManager *)parameter;
  NetworkCommand cmd;

  while (true) {
    if (xQueueReceive(self->_cmdQueue, &cmd, portMAX_DELAY)) {
      switch (cmd.action) {
      case ACT_FETCH_BUS:
        self->_latestBusMinutes = self->_busClient->getNextBusTime();
        self->_busDataReady = true;
        break;

      case ACT_WLED_RAINBOW:
        self->_wledClient->setRainbow();
        break;

      case ACT_WLED_COFFEE:
        self->_wledClient->setCoffee();
        break;

      case ACT_WLED_LOADING:
        self->_wledClient->setLoading();
        break;

      case ACT_WLED_BUS_TIME:
        self->_wledClient->updateForBusTime(cmd.param);
        break;

      case ACT_WLED_SAVE:
        self->_wledClient->saveState();
        break;

      case ACT_WLED_RESTORE:
        self->_wledClient->restoreState();
        break;
      }
    }
  }
}
