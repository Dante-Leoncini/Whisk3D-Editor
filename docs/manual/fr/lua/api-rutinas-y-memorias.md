# Routines et mémoires

Les routines (listes d'étapes de dessin construites dans le panneau Propriétés) peuvent lire des valeurs placées par Lua. Chaque mémoire est un bloc nommé de 256 nombres, et les étapes la lisent sous la forme `@nombre[i]`.

| Fonction | Ce qu'elle fait |
|---|---|
| `setMemoria(nombre, i, v)`, `memoria(nombre, i)`, `setMemorias(nombre, i, v1, v2, ...)` | Écrire et lire des mémoires (l'indice va de **0** à 255). |
| `setPaso(rutina, i, campo, ...)`, `leerPaso(rutina, i, campo)` | Modifier ou lire une étape d'une routine (champs : `"on"`, `"n"`, `"color"`, `"ref"`, `"modo"`, `"entero"`). |
| `paso(nombre, ...)` | Exécute une étape de dessin ; ne fonctionne qu'à l'intérieur d'une fonction appelée par l'étape « Appeler Lua » d'une routine. |

Dans une routine dont **Répéter** est supérieur à 1, la mémoire `bucle` indique à quel tour on en est : `@bucle[0]` est le tour actuel (à partir de 0) et `@bucle[1]` le total.
