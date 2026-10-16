---
name: wled-json-controller
description: How the firmware drives a WLED light strip over its JSON API (WledClient), including the save/restore hand-off between the bus effect, the leave-now alert and manual light choices. Use this skill when modifying WledClient, NetworkManager's WLED actions, light effects, or StateManager's light ownership logic.
---

# WLED JSON Controller

## Transport (`WledClient`)
- `POST http://<WLED_IP>/json/state` with a JSON body; `GET` the same URL to save state. `WLED_IP` comes from `secrets.h`; it must be <= `MAX_IP_LENGTH` and the URL <= `MAX_URL_LENGTH`.
- Fresh connection every call (`http.setReuse(false)`): a kept-alive socket goes stale when WLED reboots.
- `WLED_TIMEOUT_MS` (1s) for connect and read: WLED is on the LAN, so fail fast.
- **Backoff:** any failed call puts the client in backoff for `WLED_BACKOFF_MS` (30s); calls during backoff are skipped and logged, so an unplugged controller doesn't delay the bus fetches queued behind it. A successful call clears backoff. Restores ignore backoff (see below).
- All calls run on the network task (core 0). The UI only queues commands through `NetworkManager` (`requestWledRainbow/Coffee/Loading/BusTime/LeaveNow/SaveState/RestoreState/DiscardSaved/CoffeeStatus`). The queue holds 10; a full queue drops the command with a `[Net] Queue full` log.

## Payload rules
- **Only touch segment 0** (`"seg":[{"id":0, ...}]`). Never send `{"stop":0}` placeholder entries or any other segment objects: they delete segments the user has set up on their controller.
- Send only the properties you need to change.
- Current payloads (copy kept in [wled_presets.json](assets/wled_presets.json)):
  - Rainbow: `fx` 9, bri 200, transition 7, seg on/unfrozen, bri 255.
  - Coffee: warm white `[255,221,209]`, `fx` 0, same brightness/transition.
  - Loading (bus fetch in flight): blue Breathe, `fx` 2, `sx` 128.
  - Leave now: fast green Breathe, `fx` 2, `sx` 220, for `LEAVE_PULSE_MS` (3s).
  - Coffee status (`updateCoffeeStatus(seconds)`): `{"seg":[{"id":0,"i":[0,6,"RRGGBB",6,len,"FFDDD1"]}]}`. Individual LEDs freeze the segment; `{"seg":[{"id":0,"frz":false}]}` hands it back to plain white. Only sent after `readCoffee()` (a streamed GET relying on WLED's key order) sees segment 0 on, `fx` 0 and exactly `[255,221,209]`. Any other POST clears `_status` so the next update redraws.
  - Bus colour (`updateForBusTime(minutes)`): solid `fx` 0, red if `<= BUS_RED_MAX_MIN` (5) or `> BUS_ORANGE_MAX_MIN` (12), green up to `BUS_GREEN_MAX_MIN` (9), else orange. Same thresholds as the Bus screen.

## Save / restore
- `saveState()` GETs `/json/state` (HTTP/1.0) into a `String` reserved once at boot to `WLED_STATE_MAX_BYTES` (8KB). Bodies that are larger, empty or don't start with `{` are refused, so a misbehaving controller can't eat the heap TLS needs.
- If a saved state is already held (an earlier restore failed), `saveState()` keeps it: it is the user's real state, while the strip is probably still showing an effect.
- `restoreState()` POSTs the saved body back. It **ignores backoff**; if it fails, the state is kept and `restorePending()` is set, and the network task retries every 30s until it lands (even with nothing queued).
- `discardSavedState()` drops the saved state and any pending retry.

## Ownership (`StateManager::_wledOwner`)
`WledOwner { WLED_NONE, WLED_BUS, WLED_LEAVE }`. Whoever takes the strip from `NONE` queues a save first; handing back to `NONE` queues a restore.
- **Bus effect:** `startBusEffect()` on entering Bus Times or pressing Refresh. Returns false (no effect) if the leave alert owns the strip. Shows Loading, then the bus colour once data arrives. `endBusEffect()` restores after `BUS_EFFECT_TIMEOUT_MS` (10s), when leaving the Bus screen (via `goToState`), or when the result is "no buses".
- **Leave now:** `updateLeaveAlert()` turns on when either of the next two buses is 6-8 min away (`LEAVE_WINDOW_MIN/MAX_MIN`), on weekdays in `LEAVE_ALERT_DAYS`, between 07:00 and 09:30 local time (`LEAVE_ALERT_START/END_HHMM`, `TIMEZONE`, needs NTP). It only pulses if the last coffee check said the lights aren't coffee white. It takes over from the bus effect without a second save, and restores after `LEAVE_PULSE_MS` (or when the window passes, if sooner). LEAVE NOW stays on screen for the whole window.
- **Coffee status:** `updateCoffeeStatus()` queues `requestWledCoffeeStatus(seconds)` every `STATUS_TICK_MS` (10s), only while the owner is `NONE` (it catches up straight after an effect, and after Rainbow/Coffee is picked). It doesn't own the strip: it never saves or restores. The result feeds `NetworkManager::coffeeLightsOn()`.
- **Manual Lights choice** (Rainbow / Coffee): `takeBackLights()` always queues `DiscardSaved` and sets the owner to `NONE` before sending the preset. The user's choice wins: nothing later "restores" over it, and a state left over from a failed restore can't resurface. It is discarded even when no effect is running.
- The health monitor only restarts the device when `_wledOwner == WLED_NONE`, so a restart never strands an effect.

## Adding an effect
1. Add a `const char[]` payload in `WledClient.cpp` (segment 0 only) and a method.
2. Add a `NetworkAction`, a `requestWledX()` and a `case` in `NetworkManager::networkTask`.
3. Decide ownership: temporary effects must go through the save/restore hand-off above; user choices must call `takeBackLights()`.

Effect IDs and fields: [wled_api.md](references/wled_api.md).
