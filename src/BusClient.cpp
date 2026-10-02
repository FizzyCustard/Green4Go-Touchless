#include "BusClient.h"
#include "Config.h"

BusClient::BusClient() {}

void BusClient::begin() {
  // For ESP32 Secure Client
  _secureClient.setInsecure(); // Skip certificate validation for simplicity
}

int BusClient::getNextBusSeconds(char *lineOut, size_t lineLen) {
  if (lineLen > 0)
    lineOut[0] = '\0';

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
    apiResult = parseNextArrival(http.getStream(), lineOut, lineLen);
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

int BusClient::parseNextArrival(Stream &stream, char *lineOut,
                                size_t lineLen) {
  int minSeconds = -1;
  int iterations = 0;
  unsigned long started = millis();

  // TfL returns an unsorted array of arrival objects. Scan for
  // "lineName":"X" followed by "timeToStation":Y and keep the smallest.
  // find() returns false at end of stream (or on timeout), which ends the
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

    if (minSeconds == -1 || seconds < minSeconds) {
      minSeconds = seconds;
      if (lineLen > 0) {
        strncpy(lineOut, lineName.c_str(), lineLen - 1);
        lineOut[lineLen - 1] = '\0';
      }
    }
  }

  if (minSeconds != -1)
    return minSeconds;

  return BUS_NO_BUSES;
}
