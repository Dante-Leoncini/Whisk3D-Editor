# Rutinas y memorias

Las rutinas (listas de pasos de dibujo que se arman en el panel de Propiedades) pueden leer valores que pone Lua. Cada memoria es un bloque de 256 números con nombre, y los pasos la leen como `@nombre[i]`.

| Función | Qué hace |
|---|---|
| `setMemory(name, i, v)`, `getMemory(name, i)`, `setMemories(name, i, v1, v2, ...)` | Escribir y leer memorias (el índice va de **0** a 255). |
| `setStep(routine, i, field, ...)`, `getStep(routine, i, field)` | Cambiar o leer un paso de una rutina (campos: `"on"`, `"n"`, `"color"`, `"ref"`, `"modo"`, `"entero"`). |
| `step(name, ...)` | Ejecuta un paso de dibujo; solo vale dentro de una función llamada por el paso "Llamar Lua" de una rutina. |

En una rutina con **Repetir** mayor que 1, la memoria `loop` dice en qué vuelta va: `@loop[0]` es la vuelta actual (desde 0) y `@loop[1]` el total.
