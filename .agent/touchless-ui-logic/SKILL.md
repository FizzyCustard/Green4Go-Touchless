---
name: touchless-ui-logic
description: How hand distance from the ultrasonic sensor drives the touchless UI - zones, dwell selection, re-arming, the timer menu, unlock and presence detection. Use this skill whenever adding screens or menu items, changing distance thresholds, or touching StateManager or DistanceSensor.
---

# Touchless UI Logic

All interaction is one number: the smoothed hand distance from `DistanceSensor::getSmoothedDistance()`, read once per 50ms tick (`MAIN_LOOP_INTERVAL_MS`) in `StateManager::update()`. Constants are in `src/Config.h`.

## Distance map
| Distance | Meaning |
|---|---|
| < 3cm (`UNLOCK_DISTANCE_CM`), held 1s | Unlock from standby (cancel if > 5cm: + `UNLOCK_HYSTERESIS_CM`) |
| 0-15cm (`MAX_INTERACTION_DISTANCE`) | Three 5cm zones = the three 80px screen rows. Zone 0 (closest) = top row |
| 15-25cm (`BASE_ACTIVE_MIN/MAX_DISTANCE`) | Base zone: "Closer to select" strip lights up; on the Timer screen it opens the menu |
| > 30cm (`TIMER_MENU_CLOSE_DISTANCE`) | Closes the timer menu (and arms it, see below) |
| < 25cm and moved recently | Counts as a hand being present (keeps screen awake) |

## Rules for every interactive screen
- **Always three zones, rows line up with zones.** Every screen is three 5cm zones over three 80px rows; don't change `numZones` per screen. A screen whose top row is information only sets `firstRow = 1` so zone 0 selects nothing:
  - Main menu: 0 Timer, 1 Bus Times, 2 Lights
  - Lights: 0 Rainbow, 1 Coffee, 2 Back
  - Timer: 0 timer (info), 1 Reset, 2 Back (only while the menu is open)
  - Bus Times: 0 bus info, 1 Refresh, 2 Back
- **Zone hysteresis.** `calculateZone(dist, numZones, prevZone)` keeps the current zone until the hand is `ZONE_HYSTERESIS_CM` (0.5cm) past its edge. `_rawZone` is the previous zone and is **not** reset on screen change (all screens share the geometry), so the hysteresis carries across screens.
- **Dwell to select.** Holding a selectable row for `DWELL_THRESHOLD_MS` (1s) selects it. Progress (0..1) is drawn as a bar along the bottom of the row by `drawDwellProgress(row, progress)`. Never select on a distance change alone, and never use `delay()`.
- **Re-arm rule (no double selections).** `handleSelection()` and every `goToState()` call `disarm()`. While disarmed, nothing can be selected until the hand either leaves the rows (row becomes `ROW_NONE`) or moves to a *different* row **and** more than `REARM_DISTANCE_CM` (2cm) from where it was when disarmed. The first reading after disarming records the row (`ROW_UNSET` -> row). There is no time-based input lockout.
- **All screen changes go through `goToState()`.** It disarms, resets dwell and the standby timer, gives back bus lights when leaving Bus Times, and stops the timer when leaving the Timer screen.
- **Base strip and cursor** (`updateBaseZone`, `setHandCursor`) are drawn after the screen, and skipped on the tick a state changes so the new screen sets up its layout first.
- `SHOWING_SUCCESS` / `SHOWING_API_ERROR` return to the main menu after 1s / 3s.
- Interactive screens drop to standby after `STANDBY_TIMEOUT_MS` (30s) without a present hand; a running timer extends this to `TIMER_AWAKE_MAX_MS` (1h).

## Timer screen menu
The menu (Reset / Back) slides up when the hand enters the base zone and closes when it goes beyond 30cm. **Arming:** after choosing Timer, the menu cannot open until the hand has first gone beyond 30cm (`_timerMenuArmed`), because the hand that picked Timer passes through the base zone on its way out. While the menu is hidden, toggling, or animating (`_display->isAnimating()`), `numZones` is 0 so nothing is selectable.

## Presence and static objects
`trackMovement()` resets `_lastMoveAt` whenever the distance changes by more than `MOVE_THRESHOLD_CM` (1cm). `handPresent(dist)` = `dist < 25cm` **and** moved within `STATIC_OBJECT_MS` (60s). A mug or kettle in front of the sensor therefore stops keeping the screen awake, re-unlocking it, or waking the backlight after a minute. Use `handPresent()`, not a raw distance check, for anything "is someone there".

## Sensor pipeline (`DistanceSensor::update`, every tick)
1. Trigger, `pulseIn` with a 12ms timeout (~2m). Readings outside 0.2-400cm are misses.
2. **Missed-echo bridging:** up to `SENSOR_MAX_MISSES` (3) consecutive misses reuse the last good reading; after that the reading is `SENSOR_FAR_CM` (100cm), so the cursor slides out rather than jumping.
3. **Median of the last 3** readings drops single spikes.
4. **EMA**, `SENSOR_EMA_ALPHA` = 0.35 weight on the new value.

## Adding a screen or menu
1. Add an `AppState`, a `DisplayManager::drawX(activeRow)` and an `actionX(row)`; wire them into `updateInteractive()` and `handleSelection()`.
2. Keep three rows. If the top row is info only, set `firstRow = 1` in `updateInteractive()` (that is what makes it non-selectable) and pass `nullptr` as its label to `drawMenuRows` so the menu code leaves that row to your own drawing.
3. Navigate only with `goToState()`.
4. Add the state to `isInteractive()` if it should time out to standby and show the cursor.

Exact zone maths and the dwell/re-arm code: [interaction_model.md](references/interaction_model.md).
