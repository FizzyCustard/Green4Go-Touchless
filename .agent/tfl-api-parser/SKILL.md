---
name: tfl-api-parser
description: How the firmware fetches and stream-parses TfL bus arrivals on the ESP32 (BusClient), how results reach the UI (NetworkManager), and the refresh/status-code rules. Use this skill when modifying BusClient, the bus refresh policy in StateManager, or adding another HTTPS JSON API.
---

# TfL API Parser

## Request (`BusClient::getNextBusSeconds`)
- URL: `https://api.tfl.gov.uk/StopPoint/<TFL_STOP_ID>/Arrivals`, plus `?app_key=<TFL_APP_KEY>` only if `TFL_APP_KEY` is defined in `secrets.h` (optional; anonymous use works for one device).
- `WiFiClientSecure` with `setInsecure()` (no certificate check) and `setHandshakeTimeout(TLS_HANDSHAKE_TIMEOUT_S)` = 10s (library default is 120s, which would block every queued WLED command).
- `http.useHTTP10(true)`: **HTTP/1.0 so the body is never chunked**. Chunk-size lines in a raw chunked stream could split a key mid-match. Keep this.
- Connect timeout `HTTP_CONNECT_TIMEOUT_MS` (4s), read timeout `HTTP_TIMEOUT_MS` (3s).
- Returns `BUS_OFFLINE` without trying if WiFi is down.

## Never build a DOM
Busy stops return 50+ arrivals (tens of KB). Don't use `ArduinoJson`/`getString()`; stream-parse with `find()`, `readBytesUntil()` and `parseInt()`, no `String` allocations.

## BodyStream: knowing whether the body really ended
`BusClient.cpp` wraps `http.getStream()` in `BodyStream` (a read-only `Stream`) that:
- Ends **cleanly** when `Content-Length` bytes have been read, or the server closed with nothing left buffered. It then sets its timeout to 0 so `find()` returns at once instead of waiting out the read timeout at the end of every fetch.
- Treats a gap longer than the read timeout as a **stall** (`find()` gives up, `endedCleanly()` stays false).
- Enforces an **absolute deadline** `BUS_PARSE_DEADLINE_MS` (8s) for the whole body, so a server trickling bytes can't hold the network task. Also not clean.
- `delay(1)` while waiting for the next TLS record rather than spinning.

## Parse loop (`BusClient::parseNextArrival`)
- `while (iterations < MAX_JSON_ITERATIONS && stream.find("\"lineName\":"))`, with `MAX_JSON_ITERATIONS` = 200. **Do not gate the loop on `stream.available()`**: it can read 0 mid-response while the next TLS record is in flight, which used to cut parsing short.
- Line name: `readBytesUntil('"', buf, MAX_LINE_NAME_LENGTH + 1)` into a fixed `char[MAX_LINE_NAME_LENGTH + 2]`. Bounded whatever the server sends; a name of length 0 or > `MAX_LINE_NAME_LENGTH` (20) is skipped.
- Then `find("\"timeToStation\":")` + `parseInt()`. Relies on `lineName` coming before `timeToStation` in each object (it does in TfL's responses). Values outside 0..`MAX_BUS_TIME_SECONDS` (7200) are skipped.
- Filter: `lineMatchesFilter()` does an **exact** match against the comma-separated `TFL_FILTER_LINES` (spaces trimmed; `""` = all lines), so `"136"` does not match route `36`. No allocation.
- Keeps the **two soonest** times; the line name is taken from the soonest.

## Return contract
`int getNextBusSeconds(char *lineOut, size_t lineLen, int &secondOut)` returns **seconds** (not minutes) to the soonest matching bus, sets `secondOut` to the second-soonest (or -1), and copies its route into `lineOut`. Negative values are status codes from `Config.h`:

| Code | Value | Meaning |
|---|---|---|
| `BUS_LOADING` | -1 | No data yet / fetch in flight (initial state, never returned by BusClient) |
| `BUS_API_ERROR` | -2 | HTTP error, URL too long, or body didn't end cleanly and nothing was found |
| `BUS_NO_BUSES` | -3 | Whole body read cleanly, no matching arrivals |
| `BUS_OFFLINE` | -4 | WiFi not connected |

"No buses" is only trusted if the body ended cleanly. An incomplete body with at least one arrival still returns that arrival (logged as "Response incomplete").

## Threading (`NetworkManager`)
HTTP never runs on the UI loop. `StateManager` calls `_networkManager->requestBusUpdate()` (skipped if a fetch is already queued or running); a FreeRTOS task on core 0 (16KB stack, queue of 10) runs the fetch and stores the result under a `portMUX` lock. The UI polls:
- `bool takeBusResult(int &seconds, int &second, unsigned long &completedAt, char *line, size_t lineLen)` returns true **once** per finished fetch.
- `bool isBusFetchPending()` drives the "Updating..." / "Loading..." text.

## Refresh policy (`StateManager`)
- Stores seconds plus the `millis()` of the fetch and **counts down locally**; minutes shown = remaining seconds / 60.
- `maintainBusData()` runs in every state after boot: every 60s normally; every 20s (`LEAVE_REFRESH_MS`) during leave-alert hours while either bus is 6-12 min away (the leave window plus `LEAVE_REFRESH_LEAD_MIN`); at most every 30s on the Bus screen; after failures 15s doubling to 5 min.
- A failed refresh keeps the previous good data for up to `BUS_STALE_MS` (3 min) instead of overwriting it. On the Bus screen an error with no usable data shows the error screen ("API Error" / "No WiFi").
- Entering Bus Times shows data younger than `BUS_FRESH_MS` (60s) instantly and always requests a refresh.
- Boot splash waits for a real answer (data or "no buses") for 2-10s, retrying every 5s.

Schema notes: [tfl_schema.md](references/tfl_schema.md). Pattern for a new streamed API: [stream_parse_template.cpp](assets/stream_parse_template.cpp).
