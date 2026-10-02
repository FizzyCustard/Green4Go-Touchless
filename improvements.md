# Green4Go Screen: Review & Improvements

Review of the touchless coffee-zone controller firmware (ESP32-S3 + HC-SR04 + ST7789 + TfL + WLED). Everything under **Bugs fixed** and **UX fixed** has been implemented on this branch. **Questions for Naji** are decisions I left for you. **Feature ideas** are for review only, nothing has been built.

Verification: there's no hardware in the review environment, so the firmware was compile-checked and linked against Arduino-ESP32 2.0.17 + GFX 1.4.9 (the same versions PlatformIO resolves), with zero warnings. The state machine was also exercised in a host-side simulation with scripted hand movements (see the end of this file). It still needs a real-device pass before you trust it.

---

## Bugs fixed

### Interaction / state machine

| # | Bug | Effect | Fix |
|---|-----|--------|-----|
| 1 | After unlocking (hand < 3cm) the hand is sitting in the "Timer" zone (0-5cm) with no lockout | Keep your hand there one more second and Timer opens by itself | **Re-arm rule**: after any selection or screen change, nothing can be selected until the hand moves to a different row (or away) |
| 2 | Selecting **Bus Times** at 5-10cm lands on a screen where the whole 0-15cm range is "Back" | Bus screen shows for ~1.4s then bounces back to the menu, then can re-select Bus Times: a loop | Same re-arm rule, plus the new Bus screen layout (see UX) |
| 3 | Selecting **Lights** at 10-15cm lands with the hand on Lights' "Back" row | Lights screen immediately exits back to the menu | Re-arm rule |
| 4 | Holding still on a row re-fired the selection every 1.4s | Timer "Reset" kept resetting, success screens chained into the next screen's selection | One selection per visit to a row |
| 5 | Boot splash waited for *valid* bus data with no upper limit | At night (no buses), offline, or with TfL down the device **never leaves the splash screen** | Splash exits on any real answer ("no buses" counts) or after 10s max (`SPLASH_MAX_MS`) |
| 6 | First boot fetch only fired at 5s, not 2s (`millis() - 0 > 5000` check) | Slower boot than intended | Fetch fires as soon as WiFi connects |
| 7 | WiFi down returned the same code as "loading" (-1) | Bus screen showed **"Loading..." forever** with WiFi down | Separate `BUS_OFFLINE` code; shows "No WiFi" |
| 8 | WLED state was only restored by the 10s effect timer, and only while still on the Bus screen | Leaving Bus Times early (Back, or the API error screen) left the strip **stuck on the bus colour or the blue loading breathe** | All exits from Bus Times go through one place that restores WLED |
| 9 | Timer screen auto-locked after 30s with no hand present | A running coffee timer was **killed after 30 seconds** | A running timer keeps the screen awake (up to 1 hour, `TIMER_AWAKE_MAX_MS`). See question Q2 |
| 10 | Spec'd ±0.5cm zone hysteresis was never implemented (the constant was unused) | Hand on a zone boundary made the highlight flicker and kept restarting the dwell timer | Hysteresis added to `calculateZone` |
| 11 | Timer menu opened as soon as the hand passed through 15-25cm on its way *out* after picking Timer | Every timer start played an open-then-close animation | Menu only opens after the hand has left (>30cm) once |
| 12 | During the timer menu slide, selection and hover drawing stayed live | Menu rows drawn at final positions on top of the moving animation | Zones are disabled while the menu is animating |
| 13 | `_ignoreInputUntil = millis() + N` comparisons | Breaks at the 49-day `millis()` rollover | Removed; all timing uses `millis() - start` |

### Display / flicker

| # | Bug | Effect | Fix |
|---|-----|--------|-----|
| 14 | Success / error screens used `fillScreen`, painting over the base strip, but the base cache wasn't reset | Bottom 80px **stayed green/red** on the menu after "Rainbow!" or "API Error" | Message screens only use the 240px content area; base has a proper "unknown" state |
| 15 | Base strip was never drawn at boot (cache already said "inactive") | Base was black on some screens and grey on others depending on history | Base repaints whenever a screen invalidates it |
| 16 | Timer animation ran **unthrottled** every `loop()` pass, clearing and redrawing the timer and both menu items thousands of times a second | Visible flicker/tearing during the slide, and a CPU spin | Frame-capped (~60fps), only vacated strips are cleared, every pixel written once per frame |
| 17 | During the slide, `StateManager` also drew the timer at its final position | Ghost timer digits left on screen | The display owns the timer position; the animation picks up value changes |
| 18 | Text redraws were clear-then-draw (black flash under every change), and the standby screen did a full 240x240 black fill every minute | Blink every time the bus minute changed; flash on every hover change | New `drawTextInBand` paints around the text box then draws opaque text, so no pixel flashes. Only changed rows/lines are repainted |
| 19 | Backlight switched on before the panel was cleared | Garbage pixels flash at power-on | Clear first, then backlight |
| 20 | Text wrap was on | Long strings ("Loading..." at size 4 is exactly 240px) could wrap onto a second line | Wrap off; text auto-sizes to fit |

### Sensor

| # | Bug | Effect | Fix |
|---|-----|--------|-----|
| 21 | The debug `printf` called `readDistance()` a **second time** every tick | Doubled the blocking time and fired a second ping whose echo collides with the first, causing bogus readings | One ping per tick; debug output behind `DEBUG_SENSOR` |
| 22 | Single stray echoes went straight into the EMA | Spikes smeared into several ticks of wrong distance: false unlocks in standby (one cause of the Phase 3 "standby flicker") and jumpy hover | Median-of-3 filter before the EMA |
| 23a | Missed echoes went straight in as "100cm" | Two dropouts in a row with a hand held still read as "hand left", re-arming the row so it fired again (found by the simulation) | Up to 3 consecutive misses (150ms) are bridged with the last good reading |
| 23 | 30ms echo timeout | With no hand present (most of the time) every tick blocked ~30ms, stalling animations | 12ms timeout (~2m range; interaction tops out at 30cm) |

### Network

| # | Bug | Effect | Fix |
|---|-----|--------|-----|
| 24 | WiFi reconnect in `loop()` blocked with `delay()` for up to 10s, every 30s | **UI frozen for 10s at a time** whenever WiFi was down | Non-blocking: auto-reconnect plus a `WiFi.reconnect()` nudge |
| 25 | `setup()` blocked up to 10s waiting for WiFi | Same work the splash already does | Removed; the splash handles it |
| 26 | Line filter used `indexOf` substring matching | A filter of `"136"` also matched routes **36, 13, 1, 3, 6** | Exact match on the comma-separated list |
| 27 | Parser stopped after 50 arrivals and gated on `stream.available()` | Busy stops return 50+ unsorted arrivals so the soonest bus could be missed; `available()` can read 0 mid-TLS-stream and end parsing early | Cap raised to 200, loop runs until `find()` hits end of stream, 8s hard deadline |
| 28 | HTTP/1.1 chunked responses parsed from the raw stream | Chunk headers can split a key and drop an arrival | `useHTTP10(true)`: no chunking |
| 29 | Bus result handoff between cores used separate get/clear calls on volatile fields | A result landing between "get" and "clear" was lost; no memory ordering | Single locked `takeBusResult()` |
| 30 | Every standby/warmup tick could queue another fetch | Duplicate fetches piled up behind slow requests | Pending flag dedupes fetches |
| 31 | WLED calls used the default 5s connect timeout and sat in the same queue as bus fetches | With WLED unplugged, opening Bus Times waited **10s+ for "Loading..."** | 1s LAN timeout plus 30s backoff after a failure |
| 32 | Connect timeout not set for TfL (only read timeout) | Up to 5s connect waits | Explicit connect timeout |

### Found by the independent code review (second pass)

| # | Bug | Effect | Fix |
|---|-----|--------|-----|
| 33 | The "Hold" label on the unlock ring painted a full-width band over the grey track | Two gaps in the ring at 3 and 9 o'clock | Label drawn before the ring |
| 34 | Cancelling an unlock did a full-screen clear and redraw | Hovering around 3-5cm made standby flash repeatedly | Only the ring is erased; the standby text repaints in place |
| 35 | WLED restore was skipped during the 30s failure backoff and the saved state thrown away | One network blip mid-effect left the lights **stuck on the bus colour** | Restore always tries and keeps the saved state until it succeeds; a re-save won't overwrite an unrestored original |
| 36 | A stalled or cut-off TfL response was reported as "No buses" (a success) | Wiped good countdown data and slowed retries to 60s | Body reader detects a clean end vs a stall; incomplete responses are errors. Also ends instantly at end-of-body (was a 1s busy-spin per fetch) |
| 37 | TLS handshake timeout was the library default of 120s | A stalled handshake blocked every queued WLED command (including restores) for 2 minutes | 10s (`TLS_HANDSHAKE_TIMEOUT_S`) |
| 38 | A hand resting near a zone boundary could drift half a centimetre, change row and re-arm | Possible screen ping-pong (e.g. Lights -> Back -> Lights) without the user moving | Re-arming needs the hand to leave, or move >2cm (`REARM_DISTANCE_CM`) to a different row. Hysteresis carries across screen changes |

### Code health

- `DisplayManager` destructor deleted `Arduino_DataBus` through a non-virtual destructor (undefined behaviour, compiler warning). Removed: the object lives for the whole run.
- `secrets.example.h` used `static const char*` values unused in most files (10 warnings). Now `#define`s. **Your existing `src/secrets.h` still works as-is**; switch it to `#define` if you want a warning-free build.
- Removed dead code: `DistanceSensor::getActiveZone()`, `renderMenuSlider`, `clearContentArea`, unused zone fields, `IGNORE_INPUT_TIME_MS`.
- All new timings are tunable in `Config.h`.

---

## UX fixed / improved

1. **Dwell progress bar.** The biggest usability gap. Holding on a row gave no feedback for a full second, then something happened. Now a bar fills along the bottom of the hovered row, so you can see a selection coming and pull away to cancel.
2. **Readable highlights.** White text on neon yellow / cyan highlights was near-unreadable. Text colour is now picked from the background's brightness.
3. **Bus Times screen: Refresh + Back** (plan.md Phase 3, item 2). Same 3-zone pattern as the Timer screen: top row = info (dead zone), middle = **Refresh**, bottom = **Back**. Refresh replays the WLED loading → colour effect.
4. **Bus screen shows the route** ("Route 36") and "Updating..." while refreshing, and auto-refreshes every 30s (spec said this; it wasn't implemented).
5. **Instant bus screen.** If data is under a minute old it shows immediately (and colours the lights immediately) while a fresh fetch runs in the background, instead of always starting at "Loading...".
6. **Live countdown.** Bus minutes count down locally between fetches instead of sitting stale for up to 60s.
7. **Graceful failures.** A failed refresh keeps counting down the last good data for up to 3 minutes instead of flipping to an error. The API error screen only appears when there's genuinely nothing to show. Standby shows "No WiFi" / "TfL unavailable" under the time instead of a bare `--`.
8. **Standby wake hint with breathing animation** ("Hold close to wake" slowly pulsing in the bottom strip). The spec called for a breathing animation in standby; it was missing, and there was no on-screen clue how to wake the device.
9. **Unlock ring.** The green blob is now a progress ring with a "Hold" label, so you can see what you're filling and how far along you are.
10. **Base strip hint.** When your hand is in the 15-25cm base zone it now says "Closer to select".
11. **Snappier timer menu.** Slide is 300ms with ease-out (was a linear 1s), and reversing mid-slide works smoothly.
12. **Faster WLED failure handling and boot** (see network fixes): the device never sits waiting on a dead light strip.

---

## Questions for Naji

**Q1. Unlock distance (3cm) is in the HC-SR04's blind spot.** The sensor's minimum range is about 2cm, and closer than that it often misses the echo entirely, which reads as "far". That leaves roughly a 1cm window to unlock in, which will feel finicky. I'd suggest 5cm (`UNLOCK_DISTANCE_CM`). Left at 3 for you to decide.

**Q2. Timer and auto-lock.** I made a running timer keep the screen awake for up to 1 hour instead of dropping to standby after 30s (which killed the timer). Alternatives: keep it running in the background and show it on the standby screen, or make re-entering "Timer" resume instead of restart. Which behaviour do you want?

**Q3. Physical orientation.** Screen position maps to distance (top row = closest, base strip = furthest). If the sensor points *up* and you wave above it, the *lowest* hand position selects the *top* row, which may feel upside down. If the sensor is mounted at the top pointing down, it's natural. Which is it? Flipping is a one-line change in `calculateZone`.

**Q4. After a Lights selection**, it returns to the main menu. Would you rather it returned to the Lights screen so you can try the other option?

**Q5. "No buses" on the Bus screen** currently restores the lights immediately. Should it show a colour instead (e.g. red)?

**Q6. Hardware check: HC-SR04 echo voltage.** If the sensor runs on 5V, its ECHO pin outputs 5V into GPIO18, which is a 3.3V-only pin. Is there a voltage divider (or an HC-SR04P / 3.3V variant)? If not, it'll work for a while and then damage the pin.

**Q7. TLS is unverified** (`setInsecure()`). Fine for bus times, but worth knowing. Pinning the TfL root CA is a small change if you want it.

**Q8. SPI speed / PSRAM.** The display runs at the library default of 40MHz. The ST7789 on this board can usually do 80MHz, which would halve redraw times. The board also has 8MB of PSRAM that isn't enabled (`board_build.arduino.memory_type = qio_opi` + `-DBOARD_HAS_PSRAM`), which would allow a full double-buffered canvas for perfectly tear-free animation. I didn't change either because they need testing on the real board. Want me to try them?

**Q9. `plan.md` Phase 3, item 1 (standby flicker).** Likely causes found and fixed: the minute-change full repaint (#18), sensor spikes triggering an unlock start then cancel, which cleared and redrew the screen (#21, #22), and the base strip/cache issues (#14, #15). Please confirm on the device whether it's gone.

---

## Feature ideas (for review, not built)

**Interaction**
1. **Hand-position cursor:** a thin marker down the right edge showing where your hand maps, so you can find rows without guessing. Cheap, and probably the biggest learnability win for a touchless UI.
2. **Swipe gesture for Back:** a fast hand withdrawal (big distance jump in ~200ms) as a universal "back", freeing a row on every screen.
3. **Adaptive dwell:** shorter dwell (600ms) for rows the user picks often, longer for "destructive" ones like Reset.
4. **Proximity wake preview:** as a hand approaches in standby (15-30cm), brighten the screen or show "closer..." before the unlock ring starts.

**Bus**
5. **Next two buses:** "4 min, then 11 min". Very useful when you'll just miss the first one.
6. **"Leave now" alert:** WLED pulses green at the ideal leave-the-house moment (configurable walk time to the stop).
7. **Multiple stops / directions:** a row on the Bus screen to toggle between stops.
8. **Disruption status:** TfL Line Status API, showing a small warning icon when the route has delays.

**Timer**
9. **Countdown presets:** 4 min (French press), 30s (espresso bloom) with a WLED flash and screen pulse at zero. Fits "Coffee Zone" better than a stopwatch.
10. **Timer survives standby:** shows on the standby screen and resumes when re-entered (see Q2).

**Lights**
11. **Brightness slider:** map 0-15cm continuously to WLED brightness (the "sliders animating from bottom" in the spec).
12. **Pull presets from WLED:** read `/presets.json` so new lighting scenes show up without reflashing.
13. **Auto night mode:** dim the display backlight (PWM on TFT_BL) and WLED after a set hour.

**Device**
14. **Display sleep:** turn the backlight off after N minutes of standby, wake on approach. Saves the panel and power.
15. **WiFi setup portal:** WiFiManager captive portal so credentials aren't compiled in.
16. **OTA updates:** ArduinoOTA so it can be reflashed without USB.
17. **Use the onboard IMU:** a knock/tap to wake, or detect the unit being moved and pause input.
18. **Status page:** tiny web page on the device showing sensor readings, last fetch, WiFi RSSI. Makes tuning distances much easier.

---

## Verification notes

- **Compile + link:** all sources compile with `-Wall -Wextra` and zero warnings against Arduino-ESP32 2.0.17 / GFX 1.4.9, and the firmware links (flash ~830KB). PlatformIO's registry was blocked in the review environment, so this used an equivalent Arduino-core toolchain rather than `pio run`. Run `pio run` locally to confirm.
- **Behaviour:** a host-side simulation compiled the real `StateManager.cpp` against stubs and ran scripted hand-movement scenarios (unlock-then-hold, bus screen bounce, lights back-out, API error restore, timer menu arming, standby timeout, hysteresis jitter, countdown, refresh cadence, fast sweeps, echo dropouts, boundary drift). 63 checks, all passing.
- **Code review:** an independent adversarial review checked the display maths, state machine, networking and cross-core locking against the GFX and Arduino-ESP32 sources. Its six findings are fixed (#33-38).
- **Not verified:** anything visual on the real panel (animation smoothness, colours, text placement) and real sensor/network behaviour. Worth a 10-minute hands-on pass.
