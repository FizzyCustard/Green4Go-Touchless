# WLED API Reference

## State Object
To change the lights, send a JSON object to `/json/state`.

```json
{
  "on": true,
  "bri": 255,
  "transition": 7,
  "seg": [
    {
      "col": [[255, 0, 0], [0, 255, 0], [0, 0, 255]],
      "fx": 0,
      "sx": 128,
      "ix": 128
    }
  ]
}
```

### Key Properties
- `on`: Boolean. True to turn on, False to turn off.
- `bri`: Integer (0-255). Master brightness.
- `transition`: Integer. Duration of the crossfade in 100ms increments. (e.g., `7` = 700ms).
- `seg`: Array of segment objects. We usually just target the main segment (index 0 implies this if `id` is omitted).

## Segment Properties (`seg`)
- `col`: Array of RGB color arrays. Index 0 is primary, 1 is secondary, 2 is tertiary. e.g., `[[R, G, B]]`.
- `fx`: Integer. Effect ID.
  - `0`: Solid
  - `1`: Blink
  - `2`: Breathe
  - `9`: Rainbow
- `sx`: Integer (0-255). Effect speed. Standard is 128.
- `ix`: Integer (0-255). Effect intensity. Standard is 128.
