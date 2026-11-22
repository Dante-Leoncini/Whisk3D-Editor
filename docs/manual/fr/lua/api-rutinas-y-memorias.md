# Routines et mémoires

Les routines (listes d'étapes de dessin construites dans le panneau Propriétés) peuvent lire des valeurs placées par Lua. Chaque mémoire est un bloc nommé de 256 nombres, et les étapes la lisent sous la forme `@nombre[i]`.

| Fonction | Ce qu'elle fait |
|---|---|
| `setMemory(name, i, v)`, `getMemory(name, i)`, `setMemories(name, i, v1, v2, ...)` | Écrire et lire des mémoires (l'indice va de **0** à 255). |
| `setStep(routine, i, field, ...)`, `getStep(routine, i, field)` | Modifier ou lire une étape d'une routine (champs : `"on"`, `"n"`, `"color"`, `"ref"`, `"modo"`, `"entero"`). |
| `step(name, ...)` | Exécute une étape de dessin ; ne fonctionne qu'à l'intérieur d'une fonction appelée par l'étape « Appeler Lua » d'une routine. |

Dans une routine dont **Répéter** est supérieur à 1, la mémoire `loop` indique à quel tour on en est : `@loop[0]` est le tour actuel (à partir de 0) et `@loop[1]` le total.
