---
name: wled-json-controller
description: Guidelines for managing WLED light strips via the JSON API from the ESP32. Use this skill when modifying WledClient.cpp, creating new light effects, or updating the color logic based on the application state.
---

# WLED JSON Controller

This skill provides patterns for controlling a remote WLED strip using HTTP POST requests containing JSON state configurations.

## API Rules
The ESP32 communicates with WLED by sending JSON to `http://<WLED_IP>/json/state`.

**Safety Features**: All HTTP operations include input validation (IP length < MAX_IP_LENGTH, URL length < MAX_URL_LENGTH) and timeouts (HTTP_TIMEOUT_MS from Config.h) to prevent hangs and buffer overflows.

When sending commands, only send the properties you want to change to minimize payload size and processing time.
For example, to change just the color to Red and turn the strip on:
`{"on":true, "seg":[{"col":[[255,0,0]]}]}`

## Asynchronous Execution
WLED HTTP requests take time. Never trigger `WledClient` directly from the main UI loop. Instead, queue commands through `NetworkManager` (e.g., `_networkManager->requestWledSaveState()`) which manages the requests sequentially on a FreeRTOS background task.

## Common Actions
- **Saving/Restoring State:** To temporarily applying an effect (like a loading animation or a bus arrival flash), queue a save state request to the network manager first, apply your effect, and then queue the restore state request back after a timeout using `millis()`.

For effect IDs, speed properties, and JSON structures, review [wled_api.md](references/wled_api.md).
For full presets (e.g., Rainbow, Coffee), reference [wled_presets.json](assets/wled_presets.json).
