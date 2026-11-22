# Routine e memorie

Le routine (liste di passi di disegno che si costruiscono nel pannello Proprietà) possono leggere valori impostati da Lua. Ogni memoria è un blocco di 256 numeri con un nome, e i passi la leggono come `@nombre[i]`.

| Funzione | Cosa fa |
|---|---|
| `setMemory(name, i, v)`, `getMemory(name, i)`, `setMemories(name, i, v1, v2, ...)` | Scrivere e leggere le memorie (l'indice va da **0** a 255). |
| `setStep(routine, i, field, ...)`, `getStep(routine, i, field)` | Cambiare o leggere un passo di una routine (campi: `"on"`, `"n"`, `"color"`, `"ref"`, `"modo"`, `"entero"`). |
| `step(name, ...)` | Esegue un passo di disegno; vale solo dentro una funzione chiamata dal passo "Chiama Lua" di una routine. |

In una routine con **Ripeti** maggiore di 1, la memoria `loop` dice a quale giro si trova: `@loop[0]` è il giro attuale (a partire da 0) e `@loop[1]` il totale.
