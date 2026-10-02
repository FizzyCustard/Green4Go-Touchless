// Example boilerplate for Stream parsing objects on ESP32
// Replace "targetKey1" and "targetKey2" with actual json keys.

int parseStreamData(Stream &stream) {
  int bestResult = -1;

  // Safety: Prevent infinite loops
  const int MAX_ITERATIONS = 50; // Use MAX_JSON_ITERATIONS from Config.h
  int iterations = 0;

  while (stream.available() && iterations < MAX_ITERATIONS) {
    iterations++;
    // 1. Locate the first key in an object
    if (stream.find("\"targetKey1\":")) {

      if (stream.find("\"")) { // Skip the quoted string marker
        String val1 = stream.readStringUntil('\"');

        // Validate string length to prevent memory issues
        if (val1.length() > 20) { // Use MAX_LINE_NAME_LENGTH from Config.h
          continue; // Skip invalid entries
        }

        // 2. Sequential lookahead for the payload key inside the SAME object
        if (stream.find("\"targetKey2\":")) {
          int val2 = stream.parseInt();

          // Validate reasonable bounds
          if (val2 < 0 || val2 > 7200) { // Use MAX_BUS_TIME_SECONDS from Config.h
            continue; // Skip invalid times
          }

          // 3. Conditional filter logic
          if (val1 == "MyFilterCondition") {
          // If we want the smallest int (closest arrival)
          if (bestResult == -1 || val2 < bestResult) {
            bestResult = val2;
          }
        }
        }
      }
    } else {
      // Reached the end of the array/stream
      break;
    }
  }

  return bestResult;
}
