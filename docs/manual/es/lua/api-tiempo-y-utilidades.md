# Tiempo, azar y utilidades

| Función | Qué hace |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Números al azar. |
| `clockMs() → ms` | Milisegundos, para medir cuánto tarda algo. |
| `unixTime() → seconds, ms` | La hora del sistema como tiempo Unix: los segundos desde el 1/1/1970 en UTC, y aparte los milisegundos (0–999). |
| `unixTimeMs() → ms`, `unixTimeSec() → seconds` | El mismo tiempo Unix en un solo número: en milisegundos (entero) o en segundos con decimales. Medir es una resta: `t = unixTimeSec() - inicio`. Exactos con Lua de 64 bits (todas las plataformas menos la Dreamcast; ahí, `unixTime()`). |
| `date([seconds [, utc]]) → table` | La fecha y la hora de un tiempo Unix (si no se pasa, la de ahora), local, o en UTC con `utc = true`: `year`, `month`, `day`, `hour`, `min`, `sec`, `wday` (1 = domingo), `yday`. |
| `luaProfile(frames [, label])` | Mide qué partes del script son lentas (`luaProfile(0)` lo apaga). |
| `createLine(name, points)`, `linePoint(id, s)` | Una curva suave cerrada calculada por el motor (recorridos). |

El tiempo desde que empezó: guardá `unixTimeSec()` en `start()` y después restalo (tiempo real), o sumá `dt` en una variable (tiempo de juego).
