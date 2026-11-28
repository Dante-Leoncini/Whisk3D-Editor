# Time, randomness and utilities

| Function | What it does |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Random numbers. |
| `clockMs() → ms` | Milliseconds, to measure how long something takes. |
| `unixTime() → seconds, ms` | The system clock as Unix time: seconds since 1970-01-01 UTC, and the milliseconds (0–999) apart. |
| `unixTimeMs() → ms`, `unixTimeSec() → seconds` | The same Unix time in a single number: milliseconds (integer) or seconds with decimals. Measuring is a subtraction: `t = unixTimeSec() - start`. Exact with 64-bit Lua (every platform except Dreamcast; there, use `unixTime()`). |
| `date([seconds [, utc]]) → table` | Date and time of a Unix time (now if omitted), local, or UTC with `utc = true`: `year`, `month`, `day`, `hour`, `min`, `sec`, `wday` (1 = Sunday), `yday`. |
| `luaProfile(frames [, label])` | Measures which parts of the script are slow (`luaProfile(0)` turns it off). |
| `createLine(name, points)`, `linePoint(id, s)` | A smooth closed curve calculated by the engine (paths). |

Time since start: save `unixTimeSec()` in `start()` and subtract it later (real time), or add up `dt` in a variable (game time).
