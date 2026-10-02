---
name: tfl-api-parser
description: Rules and templates for fetching and parsing JSON data from external APIs (specifically TfL Transport for London API) on the ESP32. Use this skill when modifying BusClient.cpp or adding a new API integration.
---

# TfL API Parser

This skill covers how to extract JSON telemetry from external sources without fragmenting the ESP32's SRAM.

## Avoid `ArduinoJson` for Huge Payloads
The TfL API returns an array of JSON objects. Attempting to deserialize a 10KB+ string into a massive DOM tree using `ArduinoJson` causes heap crashes or significant lag.

Always use **ESP32 Stream Parsing** (`stream.find()`, `stream.parseInt()`, `stream.readStringUntil()`). Standardized in `BusClient.cpp` line 66.

## Asynchronous Execution
Never run `HTTPClient` operations on the main loop. Always offload the parsing logic to the `NetworkManager` running on a background FreeRTOS task (Core 0, 16KB stack), and communicate using thread-safe variables and flags (e.g., `isBusDataReady()`).

## Parsing Workflow
1. Use `HTTPClient` with `getStream()` and strict timeout (HTTP_TIMEOUT_MS from Config.h).
2. **Safety First**: Implement iteration limits (MAX_JSON_ITERATIONS) to prevent infinite parsing loops.
3. Locate the precise JSON Key string sequentially using `stream.find()`.
4. **Validate Data**: Check line name length (MAX_LINE_NAME_LENGTH) and time bounds (MAX_BUS_TIME_SECONDS).
5. Extract the immediate value using `readStringUntil()` or `parseInt()`.
6. Discard the rest of the stream.

## Error Handling
Always return precise integer codes to the `StateManager` for specific feedback:
- Return `-1`: Still loading initial data.
- Return `-2`: HTTP error or network failure.
- Return `-3`: Successfully parsed, but no buses match the user's filter.

For the JSON schema layout, reference [tfl_schema.md](references/tfl_schema.md).
For a boilerplate example of stream parsing, use [stream_parse_template.cpp](assets/stream_parse_template.cpp).
