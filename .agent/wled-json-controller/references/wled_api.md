# WLED JSON API Reference (as used here)

Endpoint: `http://<WLED_IP>/json/state`. `POST` a partial state to change it; `GET` returns the full current state (what `saveState()` stores and `restoreState()` posts back).

## State object
```json
{
  "on": true,
  "bri": 200,
  "transition": 7,
  "seg": [
    { "id": 0, "on": true, "frz": false, "bri": 255,
      "col": [[255, 0, 0], [0, 0, 0], [0, 0, 0]],
      "fx": 0, "sx": 128, "ix": 128, "pal": 0 }
  ]
}
```

### Top level
- `on`: power.
- `bri`: master brightness 0-255.
- `transition`: crossfade in 100ms units (`7` = 700ms).
- `seg`: segment objects. **Always give `"id":0` and send only that one entry.** WLED treats extra entries (including `{"stop":0}` padding that some exported presets contain) as instructions for other segments and can delete the user's segments.

### Segment (`seg[]`)
- `id`: segment index; 0 is the main/first segment.
- `on`, `bri`: segment power and brightness.
- `frz`: freeze. A frozen segment ignores effect changes; presets that must animate set `"frz":false`.
- `col`: up to three RGB colours `[[R,G,B], ...]` (primary, secondary, tertiary).
- `fx`: effect ID. Used here: `0` Solid, `2` Breathe, `9` Rainbow.
- `sx`: effect speed 0-255. `ix`: effect intensity 0-255.
- `pal`: palette ID (`0` = default).

## Notes
- WLED returns HTTP 200 with a small JSON body on success; the firmware only checks for a positive HTTP code.
- A full `GET` state can be several KB with many segments; the firmware caps saved state at `WLED_STATE_MAX_BYTES` (8192) and skips saving if it is larger.
