# Tiempo, azar y utilidades

| Función | Qué hace |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Números al azar. |
| `clockMs() → ms` | Milisegundos, para medir cuánto tarda algo. |
| `luaProfile(frames [, label])` | Mide qué partes del script son lentas (`luaProfile(0)` lo apaga). |
| `createLine(name, points)`, `linePoint(id, s)` | Una curva suave cerrada calculada por el motor (recorridos). |

No hay una función de "tiempo desde que empezó": sumá `dt` en una variable.
