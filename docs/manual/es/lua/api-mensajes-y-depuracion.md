# Mensajes y depuración

| Función | Qué hace |
|---|---|
| `print(...)` | Escribe en el registro (log) del motor, no en pantalla. |
| `info(msg)`, `warning(msg)`, `error(msg)` | Mensajes de información, aviso o error en el registro. `error` **no** corta el script. |
| `debugLog(msg)`, `isDebug()` | Mensajes solo en versiones de depuración. |
| `logCount()`, `logLine(i)` | Leer el registro desde el juego (una consola propia). |
