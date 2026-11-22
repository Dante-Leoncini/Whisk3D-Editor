# Routines and memories

Routines (lists of drawing steps built in the Properties panel) can read values set by Lua. Each memory is a named block of 256 numbers, and the steps read it as `@nombre[i]`.

| Function | What it does |
|---|---|
| `setMemory(name, i, v)`, `getMemory(name, i)`, `setMemories(name, i, v1, v2, ...)` | Write and read memories (the index goes from **0** to 255). |
| `setStep(routine, i, field, ...)`, `getStep(routine, i, field)` | Change or read a step of a routine (fields: `"on"`, `"n"`, `"color"`, `"ref"`, `"modo"`, `"entero"`). |
| `step(name, ...)` | Runs a drawing step; it only works inside a function called by a routine's "Call Lua" step. |

In a routine with **Repeat** greater than 1, the `loop` memory tells you which pass it's on: `@loop[0]` is the current pass (starting from 0) and `@loop[1]` the total.
