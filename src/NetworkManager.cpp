#include "NetworkManager.h"
#include "BusClient.h"
#include "WledClient.h"

NetworkManager::NetworkManager(BusClient *busClient, WledClient *wledClient) {
  _busClient = busClient;
  _wledClient = wledClient;
  _cmdQueue = nullptr;
  _busDataReady = false;
  _busFetchPending = false;
  _latestBusSeconds = BUS_LOADING;
  _latestSecondSeconds = -1;
  _latestBusAt = 0;
  _latestLine[0] = '\0';
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

void NetworkManager::enqueue(NetworkAction action, int param) {
  NetworkCommand cmd = {action, param};
  if (!_cmdQueue || xQueueSend(_cmdQueue, &cmd, 0) != pdTRUE) {
    Serial.printf("[Net] Queue full, dropped action %d\n", action);
    if (action == ACT_FETCH_BUS) {
      portENTER_CRITICAL(&_lock);
      _busFetchPending = false;
      portEXIT_CRITICAL(&_lock);
    }
  }
}

void NetworkManager::requestBusUpdate() {
  portENTER_CRITICAL(&_lock);
  bool alreadyPending = _busFetchPending;
  _busFetchPending = true;
  portEXIT_CRITICAL(&_lock);
  if (!alreadyPending)
    enqueue(ACT_FETCH_BUS);
}

void NetworkManager::requestWledRainbow() { enqueue(ACT_WLED_RAINBOW); }
void NetworkManager::requestWledCoffee() { enqueue(ACT_WLED_COFFEE); }
void NetworkManager::requestWledLoading() { enqueue(ACT_WLED_LOADING); }
void NetworkManager::requestWledBusTime(int minutes) {
  enqueue(ACT_WLED_BUS_TIME, minutes);
}
void NetworkManager::requestWledSaveState() { enqueue(ACT_WLED_SAVE); }
void NetworkManager::requestWledRestoreState() { enqueue(ACT_WLED_RESTORE); }
void NetworkManager::requestWledLeaveNow() { enqueue(ACT_WLED_LEAVE_NOW); }
void NetworkManager::requestWledDiscardSaved() { enqueue(ACT_WLED_DISCARD); }

// --- Status Retrieval ---

bool NetworkManager::takeBusResult(int &seconds, int &second,
                                   unsigned long &completedAt, char *line,
                                   size_t lineLen) {
  bool ready;
  portENTER_CRITICAL(&_lock);
  ready = _busDataReady;
  if (ready) {
    seconds = _latestBusSeconds;
    second = _latestSecondSeconds;
    completedAt = _latestBusAt;
    if (lineLen > 0) {
      strncpy(line, _latestLine, lineLen - 1);
      line[lineLen - 1] = '\0';
    }
    _busDataReady = false;
  }
  portEXIT_CRITICAL(&_lock);
  return ready;
}

bool NetworkManager::isBusFetchPending() {
  portENTER_CRITICAL(&_lock);
  bool pending = _busFetchPending;
  portEXIT_CRITICAL(&_lock);
  return pending;
}

// --- Background Task ---

void NetworkManager::networkTask(void *parameter) {
  NetworkManager *self = (NetworkManager *)parameter;
  NetworkCommand cmd;

  while (true) {
    if (xQueueReceive(self->_cmdQueue, &cmd, portMAX_DELAY)) {
      switch (cmd.action) {
      case ACT_FETCH_BUS: {
        char line[MAX_LINE_NAME_LENGTH + 1];
        int second;
        int seconds =
            self->_busClient->getNextBusSeconds(line, sizeof(line), second);
        portENTER_CRITICAL(&self->_lock);
        self->_latestBusSeconds = seconds;
        self->_latestSecondSeconds = second;
        self->_latestBusAt = millis();
        memcpy(self->_latestLine, line, sizeof(line));
        self->_busDataReady = true;
        self->_busFetchPending = false;
        portEXIT_CRITICAL(&self->_lock);
        break;
      }

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

      case ACT_WLED_LEAVE_NOW:
        self->_wledClient->setLeaveNow();
        break;

      case ACT_WLED_DISCARD:
        self->_wledClient->discardSavedState();
        break;
      }
    }
  }
}
