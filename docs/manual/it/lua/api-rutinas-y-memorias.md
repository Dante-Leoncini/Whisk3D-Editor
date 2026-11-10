# Routine e memorie

Le routine (liste di passi di disegno che si costruiscono nel pannello Proprietà) possono leggere valori impostati da Lua. Ogni memoria è un blocco di 256 numeri con un nome, e i passi la leggono come `@nombre[i]`.

| Funzione | Cosa fa |
|---|---|
| `setMemoria(nombre, i, v)`, `memoria(nombre, i)`, `setMemorias(nombre, i, v1, v2, ...)` | Scrivere e leggere le memorie (l'indice va da **0** a 255). |
| `setPaso(rutina, i, campo, ...)`, `leerPaso(rutina, i, campo)` | Cambiare o leggere un passo di una routine (campi: `"on"`, `"n"`, `"color"`, `"ref"`, `"modo"`, `"entero"`). |
| `paso(nombre, ...)` | Esegue un passo di disegno; vale solo dentro una funzione chiamata dal passo "Chiama Lua" di una routine. |

In una routine con **Ripeti** maggiore di 1, la memoria `bucle` dice a quale giro si trova: `@bucle[0]` è il giro attuale (a partire da 0) e `@bucle[1]` il totale.
