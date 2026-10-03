#include "NetworkManager.h"
#include "BusClient.h"
#include "WledClient.h"

NetworkManager::NetworkManager(BusClient *busClient, WledClient *wledClient) {
  _busClient = busClient;
  _wledClient = wledClient;
  _cmdQueue = nullptr;
  _task = nullptr;
  _busDataReady = false;
  _busFetchPending = false;
  _latestBusSeconds = BUS_LOADING;
  _latestSecondSeconds = -1;
  _latestBusAt = 0;
  _latestLine[0] = '\0';
  _commandRunning = false;
  _commandStartedAt = 0;
}

void NetworkManager::begin() {
  _cmdQueue = xQueueCreate(NET_QUEUE_LENGTH, sizeof(NetworkCommand));

  // Core 0 alongside the WiFi stack, so HTTP never blocks the UI on core 1
  xTaskCreatePinnedToCore(NetworkManager::networkTask, "NetworkTask",
                          NET_TASK_STACK_BYTES, this, 1, &_task, 0);
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

unsigned long NetworkManager::busyForMs() {
  portENTER_CRITICAL(&_lock);
  bool running = _commandRunning;
  unsigned long started = _commandStartedAt;
  portEXIT_CRITICAL(&_lock);
  return running ? millis() - started : 0;
}

unsigned NetworkManager::stackFreeBytes() {
  return _task ? (unsigned)uxTaskGetStackHighWaterMark(_task) : 0;
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

  unsigned long lastRestoreRetry = 0;

  while (true) {
    // Wake periodically even with nothing queued, so a light restore that
    // failed (WLED or WiFi briefly down) keeps being retried until it lands
    bool got = xQueueReceive(self->_cmdQueue, &cmd,
                             pdMS_TO_TICKS(WLED_BACKOFF_MS)) == pdTRUE;
    if (self->_wledClient->restorePending() &&
        millis() - lastRestoreRetry >= WLED_BACKOFF_MS) {
      lastRestoreRetry = millis();
      self->_wledClient->restoreState();
    }
    if (!got)
      continue;

    portENTER_CRITICAL(&self->_lock);
    self->_commandStartedAt = millis();
    self->_commandRunning = true;
    portEXIT_CRITICAL(&self->_lock);

    {
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

    portENTER_CRITICAL(&self->_lock);
    self->_commandRunning = false;
    portEXIT_CRITICAL(&self->_lock);
  }
}
