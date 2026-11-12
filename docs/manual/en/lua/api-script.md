# The script and communication

| Function | What it does |
|---|---|
| `yo() → obj` | The object that owns this script. |
| `objeto(prop) → obj \| nil` | The object chosen in the `prop` field of `propiedades`. |
| `opcion(prop) → texto` | The option chosen in a drop-down list of `propiedades`; if none was chosen, the first one in the list (the default). |
| `propiedad(prop) → valor` | This object's own number, yes/no or string (or the declared default). |
| `parametro(prop [, def]) → número` | Like `propiedad`, but always a number; if it doesn't exist it returns `def` (or 0). |
| `objetivo() → obj \| nil` | The owner object's target. |
| `compartido(clave) → valor` | Reads the space shared by all scripts. |
| `setCompartido(clave, valor)` | Writes a number, yes/no or string to the shared space; `nil` deletes it. |
