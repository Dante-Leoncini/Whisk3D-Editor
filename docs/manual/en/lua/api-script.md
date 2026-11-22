# The script and communication

| Function | What it does |
|---|---|
| `self() → obj` | The object that owns this script. |
| `object(prop) → obj \| nil` | The object chosen in the `prop` field of `properties`. |
| `option(prop) → text` | The option chosen in a drop-down list of `properties`; if none was chosen, the first one in the list (the default). |
| `property(prop) → value` | This object's own number, yes/no or string (or the declared default). |
| `param(prop [, def]) → número` | Like `property`, but always a number; if it doesn't exist it returns `def` (or 0). |
| `target() → obj \| nil` | The owner object's target. |
| `getShared(clave) → value` | Reads the space shared by all scripts. |
| `setShared(clave, value)` | Writes a number, yes/no or string to the shared space; `nil` deletes it. |
