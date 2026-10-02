# TfL Arrivals API JSON Structure

Endpoint: `https://api.tfl.gov.uk/StopPoint/{id}/Arrivals`

The payload returned is an un-keyed Array `[]` containing multiple dictionary objects `{}`. 
We search locally for `"lineName":"X"` and `"timeToStation":Y`.

## Example Snippet
```json
[
  {
    "id": "1809206583",
    "operationType": 1,
    "vehicleId": "SK19FDO",
    "naptanId": "490008435E",
    "stationName": "Islington Green",
    "lineId": "19",
    "lineName": "19",
    "platformName": "B",
    "direction": "outbound",
    "destinationName": "Finsbury Park",
    "timeToStation": 234, 
    "currentLocation": "",
    "towards": "Highbury Or Finsbury Park"
  },
  {
    "id": "1809206584",
    ...
  }
]
```
Note: `timeToStation` is returned in **SECONDS**, but the UI displays **MINUTES**. It is critical that your parser divides the lowest found integer by `60` before returning it to the `StateManager`.
