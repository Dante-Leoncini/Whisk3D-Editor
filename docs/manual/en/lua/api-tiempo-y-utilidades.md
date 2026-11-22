# Time, randomness and utilities

| Function | What it does |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Random numbers. |
| `clockMs() → ms` | Milliseconds, to measure how long something takes. |
| `luaProfile(frames [, label])` | Measures which parts of the script are slow (`luaProfile(0)` turns it off). |
| `createLine(name, points)`, `linePoint(id, s)` | A smooth closed curve calculated by the engine (paths). |

There's no "time since start" function: add up `dt` in a variable.
