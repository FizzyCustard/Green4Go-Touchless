---
name: touchless-ui-logic
description: Guidelines for implementing and modifying the touchless ultrasonic UX/UI interactions. Use this skill whenever adding new screens, menus, or changing the distance mapping logic on the device.
---

# Touchless UI Logic

This skill explains how the physical distance from the ultrasonic sensor maps to on-screen UI selections.

## Core Interaction Guidelines

When modifying `StateManager.cpp` or adding new screens, adhere to these rules:

- **Usable Interaction Zone:** 0cm to 15cm. All active menu items must be mapped proportionally within this range. (Exception: Unified Back Buttons. If a screen has no other actions, map the entire 0cm to 15cm zone as a single `index 0` button).
- **Base / Dead Zone:** 15cm to 25cm. Placing a hand here highlights the "Base" UI element without selecting an item.
- **Hysteresis & Smoothing:** Always use `_sensor->getSmoothedDistance()` (Exponential Moving Average) instead of raw values. Raw values fluctuate heavily.
- **Dwell to Select:** A user must hold their hand in a specific zone for a set duration (`DWELL_THRESHOLD`) to trigger a selection. Never trigger immediately on distance change.

## Creating New Menus

To map a new menu screen with `N` items, the code relies on `StateManager::calculateZone(dist, numItems)`.

1. **Calculate Zone:** Determine the active zone (0 to N-1).
2. **Track Dwell Time:** Detect if the zone changes. If it does, record `_zoneStartTime = millis()`.
3. **Trigger Selection:** If the zone remains stable for `DWELL_THRESHOLD_MS` ms, execute `handleSelection(zone)` and apply a non-blocking interaction lockout buffer using `_ignoreInputUntil = millis() + IGNORE_INPUT_TIME_MS` to prevent accidental double-selections. Avoid using `delay()`. All timing constants are now defined in `Config.h`.
4. **Draw State:** Pass the current active zone to the DisplayManager to render hover states.

For the exact mathematical implementation and standard snippets for StateManager, read [interaction_model.md](references/interaction_model.md).
