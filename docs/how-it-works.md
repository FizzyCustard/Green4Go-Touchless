# How it works

## Architecture

Two cores, two jobs:

- **Core 1, the main loop** (`main.cpp`): reads the sensor and runs the state machine at 20Hz, runs display animations at up to 60fps between ticks, and handles WiFi upkeep and OTA. It never waits on the network.
- **Core 0, the network task** (`NetworkManager`): a FreeRTOS task that works through a queue of commands (fetch buses, set WLED effect, save/restore lights). Results come back through a lock-protected handoff. Every call has a time limit, so the task always gets back to its queue.

## Screen layout and distance zones

The 240x320 screen is three 80px rows plus an 80px base strip:

```
 y=0    ┌───────────────┬─┐
        │  Row 0        │ │   0-5cm
 y=80   ├───────────────┤ │
        │  Row 1        │ │   5-10cm      ← cursor gutter
 y=160  ├───────────────┤ │
        │  Row 2        │ │   10-15cm
 y=240  ├───────────────┤ │
        │  Base strip   │ │   15-25cm: "Closer to select"
 y=320  └───────────────┴─┘
```

Each screen maps hand distance straight onto the rows, so what's under your hand on screen is what you're pointing at. The 8px gutter on the right carries a cursor that glides to your hand's position.

## Selecting

- **Dwell:** hold on a row for `DWELL_THRESHOLD_MS` (1s) to select it. A bar fills along the bottom of the row while you hold.
- **Hysteresis:** once you're on a row, you stay on it until you move `ZONE_HYSTERESIS_CM` (0.5cm) past its edge, so jitter on a boundary doesn't flicker.
- **Re-arm:** after any selection or screen change, nothing can be selected until your hand leaves, or moves more than `REARM_DISTANCE_CM` (2cm) onto a different row. Without this, a still hand would select Bus Times and then immediately trigger whatever sits at the same height on the next screen.
- **Hands vs objects:** something that hasn't moved more than 1cm in 60s (a mug, the kettle) stops counting as a hand, so it can't keep the screen awake or wake it from standby.

## Screens

| Screen | Rows | Notes |
|---|---|---|
| Standby | none | Title, next bus, the one after, status. Breathing "Hold close to wake" hint. Hold under 3cm for 1s to unlock. |
| Main menu | Timer / Bus Times / Lights | |
| Timer | info / Reset / Back | Timer starts on entry. The menu opens when your hand returns to the base zone after leaving (over 30cm), and closes when it leaves again. A running timer keeps the screen awake for up to an hour. |
| Bus Times | info / Refresh / Back | Status colour in the info row. Refreshes every 30s while open. |
| Lights | Rainbow / Coffee / Back | Shows a confirmation, then returns to the main menu. |

After 30s without interaction, interactive screens return to standby. After 15 minutes idle in standby, the backlight fades out; a hand within 25cm fades it back in.

## Bus data

- `BusClient` fetches `https://api.tfl.gov.uk/StopPoint/{id}/Arrivals` over HTTP/1.0 and stream-parses it without buffering the whole response, keeping the two soonest arrivals matching `TFL_FILTER_LINES`.
- Results are stored as "seconds at fetch time", and the screen counts down locally between fetches.
- Refresh rate: every 60s normally, every 20s when a bus is approaching the leave-now window, every 30s on the Bus screen. Failures back off from 15s up to 5 minutes.
- A failed refresh keeps showing the last good data for up to 3 minutes before showing an error.
- Status colours: red at 5 minutes or less (too soon) or over 12 (long wait), green up to 9, orange up to 12.

## Lights (WLED)

Effects borrow the strip and give it back:

1. The first effect to take the strip saves the user's current WLED state.
2. While the Bus screen is open, the strip shows a blue breathe while loading, then the bus status colour for 10s.
3. The leave-now pulse outranks the bus effect: it takes over without saving again.
4. When the effect ends, the saved state is restored. A failed restore is retried every 30s until it lands.
5. Choosing Rainbow or Coffee discards any saved state, so a later restore never undoes your choice.

All payloads only touch segment 0, leaving any other segments alone. WLED calls time out after 1s and back off for 30s after a failure, so an unplugged controller never delays bus fetches.

## Leave-now alert

During `LEAVE_ALERT_DAYS` between `LEAVE_ALERT_START_HHMM` and `LEAVE_ALERT_END_HHMM` (local time via NTP), if either of the next two buses is between `LEAVE_WINDOW_MAX_MIN` and `LEAVE_WINDOW_MIN_MIN` minutes away, standby shows LEAVE NOW with that bus's time for the whole window, and the strip does a fast green breathe for `LEAVE_PULSE_MS` (3s) before going back to what it was showing. It wakes a sleeping screen, and stays off until the clock has synced. If the lights are coffee white there's no pulse: the status LEDs already show it.

## Bus status on the coffee light

Whenever the lights are coffee white (any day, any time), the first `STATUS_LEDS` (6) LEDs show the next bus you can still catch: red at `STATUS_RED_MIN` (15) minutes or more, fading to orange at `STATUS_ORANGE_MIN` (10), then to green at `LEAVE_WINDOW_MAX_MIN` (8). It holds green through the leave window, and turns red once the bus is under `LEAVE_WINDOW_MIN_MIN` (6) and there's no later bus to show. The rest of the strip stays white.

Every `STATUS_TICK_MS` (10s) the device reads the strip's state and only touches it if segment 0 is on, solid and exactly the coffee colour, so picking another look in the WLED app turns the status off. Drawing the LEDs freezes the segment; the device only sends an update when the colour changes, or when something has repainted the strip. While an effect has the lights it waits, and catches up as soon as they're handed back.

## Reliability

- WiFi reconnects in the background; nothing blocks the UI while it's down.
- Once a minute the device logs free heap, the largest free block and stack headroom.
- It restarts itself (only when idle in standby) if the network task is stuck for 2 minutes, the heap is too fragmented for TLS for 5 minutes, or there's been no good TfL data for 2 hours while WiFi is up. The serial log shows the reset reason at boot.
- OTA is opt-in, needs a 12+ character password, and a failed upload never changes the running firmware.
