# Messages and debugging

| Function | What it does |
|---|---|
| `print(...)` | Writes to the engine's log, not to the screen. |
| `info(msg)`, `warning(msg)`, `error(msg)` | Info, warning or error messages in the log. `error` does **not** stop the script. |
| `debugLog(msg)`, `isDebug()` | Messages only in debug builds. |
| `logCount()`, `logLine(i)` | Read the log from the game (a console of your own). |
