// Template for streaming "find the smallest value" out of a large JSON array
// on the ESP32, following the pattern in src/BusClient.cpp.
//
// Assumes `stream` is a BodyStream-style wrapper (see BusClient.cpp) that:
//   - returns -1 / ends find() once the body is complete or a deadline passes
//   - exposes endedCleanly() so "nothing found" can be told apart from "cut off"
// The HTTP request should use http.useHTTP10(true) so the body is not chunked.
//
// Replace "nameKey" / "valueKey", the bounds and the filter for your API.

#define NAME_MAX_LEN 20      // e.g. MAX_LINE_NAME_LENGTH
#define VALUE_MAX 7200       // e.g. MAX_BUS_TIME_SECONDS
#define MAX_ITEMS 200        // e.g. MAX_JSON_ITERATIONS
#define RESULT_ERROR -2      // e.g. BUS_API_ERROR
#define RESULT_NONE -3       // e.g. BUS_NO_BUSES

static bool matchesFilter(const char *name) {
  (void)name;
  return true; // Exact comparison, no String allocation
}

int parseSmallest(BodyStream &stream, char *nameOut, size_t nameLen,
                  int &secondOut) {
  int best = -1;
  int second = -1;
  int items = 0;
  char name[NAME_MAX_LEN + 2]; // One spare byte to detect over-long names

  if (nameLen > 0)
    nameOut[0] = '\0';
  secondOut = -1;

  // find() returns false at the end of the body or on a stall/deadline.
  // Do NOT add `stream.available()` to this condition: it reads 0 between
  // TLS records mid-response and would stop parsing early.
  while (items < MAX_ITEMS && stream.find("\"nameKey\":")) {
    items++;

    if (!stream.find("\""))
      break;
    // Bounded: never writes past the buffer, whatever the server sends
    size_t n = stream.readBytesUntil('"', name, sizeof(name) - 1);
    name[n] = '\0';

    // The value key must come after the name key in the same object
    if (!stream.find("\"valueKey\":"))
      break;
    int value = stream.parseInt();

    if (n == 0 || n > NAME_MAX_LEN)
      continue;
    if (value < 0 || value > VALUE_MAX)
      continue;
    if (!matchesFilter(name))
      continue;

    // Keep the two smallest
    if (best == -1 || value < best) {
      second = best;
      best = value;
      if (nameLen > 0) {
        strncpy(nameOut, name, nameLen - 1);
        nameOut[nameLen - 1] = '\0';
      }
    } else if (second == -1 || value < second) {
      second = value;
    }
  }

  // "Nothing found" is only trusted if we read the whole body. If the body
  // stalled, was cut off or hit the item cap with nothing found, report an
  // error so the caller keeps its last good data. Anything found is used.
  if (!stream.endedCleanly() && best == -1)
    return RESULT_ERROR;

  if (best != -1) {
    secondOut = second;
    return best;
  }
  return RESULT_NONE;
}
