#include "BusClient.h"
#include "Config.h"

// Read-only view of the HTTP body for the stream parser. Ends the moment
// the body is complete (Content-Length reached, or the server closed), so
// Stream::find() doesn't spin out its full timeout at the end of every fetch,
// and records whether the body ended cleanly or stalled.
class BodyStream : public Stream {
public:
  BodyStream(WiFiClient &client, int size, unsigned long timeoutMs)
      : _client(client), _left(size) {
    setTimeout(timeoutMs);
  }

  bool endedCleanly() const { return _clean; }

  int available() override { return _ended ? 0 : _client.available(); }

  int read() override {
    if (!waitForData())
      return -1;
    int c = _client.read();
    if (c >= 0 && _left > 0)
      _left--;
    return c;
  }

  int peek() override { return waitForData() ? _client.peek() : -1; }

  size_t write(uint8_t) override { return 0; }

private:
  WiFiClient &_client;
  int _left; // Bytes still expected, or -1 if unknown
  bool _ended = false;
  bool _clean = false;

  bool waitForData() {
    if (_ended)
      return false;
    if (_left == 0 || (!_client.available() && !_client.connected())) {
      _ended = true;
      _clean = true;
      setTimeout(0); // Makes the caller's timed read loop exit now
      return false;
    }
    if (!_client.available()) {
      delay(1); // Waiting on the next TLS record; don't hog the core
      return false;
    }
    return true;
  }
};

BusClient::BusClient() {}

void BusClient::begin() {
  // For ESP32 Secure Client
  _secureClient.setInsecure(); // Skip certificate validation for simplicity
  // Default is 120s, which would block every queued WLED command behind a
  // stalled handshake
  _secureClient.setHandshakeTimeout(TLS_HANDSHAKE_TIMEOUT_S);
}

int BusClient::getNextBusSeconds(char *lineOut, size_t lineLen,
                                 int &secondOut) {
  if (lineLen > 0)
    lineOut[0] = '\0';
  secondOut = -1;

  if (WiFi.status() != WL_CONNECTED) {
    return BUS_OFFLINE;
  }

  char url[128];
  int result = snprintf(url, sizeof(url), TFL_API_URL_TEMPLATE, TFL_STOP_ID);
  if (result < 0 || result >= (int)sizeof(url)) {
    Serial.println("[Bus] TFL_STOP_ID too long");
    return BUS_API_ERROR;
  }

  HTTPClient http;
  // HTTP/1.0 means no chunked transfer encoding, so the raw stream is plain
  // JSON and chunk headers can't split a key mid-match
  http.useHTTP10(true);
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);

  if (!http.begin(_secureClient, url)) {
    Serial.println("[Bus] http.begin failed");
    return BUS_API_ERROR;
  }

  int httpCode = http.GET();
  int apiResult = BUS_API_ERROR;

  if (httpCode == HTTP_CODE_OK) {
    BodyStream body(http.getStream(), http.getSize(), HTTP_TIMEOUT_MS);
    apiResult = parseNextArrival(body, lineOut, lineLen, secondOut);
  } else {
    Serial.printf("[Bus] HTTP failed: %s (%d)\n",
                  http.errorToString(httpCode).c_str(), httpCode);
  }

  http.end();
  return apiResult;
}

// Exact match against the comma separated TFL_FILTER_LINES list, so a filter
// of "136" no longer matches route "36"
bool BusClient::lineMatchesFilter(const String &lineName) {
  const char *filter = TFL_FILTER_LINES;
  if (filter[0] == '\0')
    return true;

  String list = String(",") + filter + ",";
  list.replace(" ", "");
  return list.indexOf("," + lineName + ",") != -1;
}

int BusClient::parseNextArrival(BodyStream &stream, char *lineOut,
                                size_t lineLen, int &secondOut) {
  int minSeconds = -1;
  int secondSeconds = -1;
  int iterations = 0;
  unsigned long started = millis();

  // TfL returns an unsorted array of arrival objects. Scan for
  // "lineName":"X" followed by "timeToStation":Y and keep the smallest.
  // find() returns false at end of body (or on a stall), which ends the
  // loop. Don't gate on available(): it can read 0 mid-response while the
  // next TLS record is still arriving.
  while (iterations < MAX_JSON_ITERATIONS &&
         millis() - started < BUS_PARSE_DEADLINE_MS &&
         stream.find("\"lineName\":")) {
    iterations++;

    if (!stream.find("\""))
      break;
    String lineName = stream.readStringUntil('"');

    if (!stream.find("\"timeToStation\":"))
      break;
    int seconds = stream.parseInt();

    if (lineName.length() == 0 || lineName.length() > MAX_LINE_NAME_LENGTH)
      continue;
    if (seconds < 0 || seconds > MAX_BUS_TIME_SECONDS)
      continue;
    if (!lineMatchesFilter(lineName))
      continue;

    // Keep the two soonest
    if (minSeconds == -1 || seconds < minSeconds) {
      secondSeconds = minSeconds;
      minSeconds = seconds;
      if (lineLen > 0) {
        strncpy(lineOut, lineName.c_str(), lineLen - 1);
        lineOut[lineLen - 1] = '\0';
      }
    } else if (secondSeconds == -1 || seconds < secondSeconds) {
      secondSeconds = seconds;
    }
  }

  // Only trust "no buses" if we actually read the whole response. A stall,
  // a cut-off body or hitting a parse limit is an error, so the caller keeps
  // its last good data instead of overwriting it with "no buses".
  if (!stream.endedCleanly()) {
    Serial.printf("[Bus] Response incomplete after %d arrivals\n", iterations);
    if (minSeconds == -1)
      return BUS_API_ERROR;
  }

  if (minSeconds != -1) {
    secondOut = secondSeconds;
    return minSeconds;
  }

  return BUS_NO_BUSES;
}
