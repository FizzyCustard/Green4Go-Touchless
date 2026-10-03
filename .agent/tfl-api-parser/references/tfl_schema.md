# TfL Arrivals API JSON Structure

Endpoint: `https://api.tfl.gov.uk/StopPoint/{stopId}/Arrivals` (optionally `?app_key=...`).

The body is an unsorted JSON array of arrival objects. The parser only reads `"lineName"` (string) and the `"timeToStation"` (integer) that follows it in the same object. Everything else is skipped by `find()`.

## Example (values are made up)
```json
[
  {
    "$type": "Tfl.Api.Presentation.Entities.Prediction, Tfl.Api.Presentation.Entities",
    "id": "-1234567890",
    "operationType": 1,
    "vehicleId": "AB12CDE",
    "naptanId": "490000000X",
    "stationName": "Example Street",
    "lineId": "999",
    "lineName": "999",
    "platformName": "X",
    "direction": "outbound",
    "destinationName": "Example Terminus",
    "timestamp": "2026-01-01T08:00:00.000Z",
    "timeToStation": 234,
    "currentLocation": "",
    "towards": "Example Terminus",
    "expectedArrival": "2026-01-01T08:03:54Z"
  },
  {
    "lineName": "998",
    "timeToStation": 61,
    ...
  }
]
```

## Notes
- `timeToStation` is in **seconds**. `BusClient` returns seconds; `StateManager` converts to minutes for display (`remainingSeconds / 60`, counted down locally from the fetch time).
- The array is **not sorted** by time; the parser scans every entry (up to `MAX_JSON_ITERATIONS` = 200) and keeps the two smallest.
- `lineName` must precede `timeToStation` within each object for the sequential `find()` approach to pair them correctly.
- An empty array `[]` (e.g. at night) is a valid response: it parses cleanly and gives `BUS_NO_BUSES`.
- Find a stop's `naptanId` on tfl.gov.uk and put it in `TFL_STOP_ID` in `src/secrets.h` (never in tracked files).
