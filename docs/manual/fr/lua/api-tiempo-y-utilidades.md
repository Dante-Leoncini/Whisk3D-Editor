# Temps, hasard et utilitaires

| Fonction | Ce qu'elle fait |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Nombres aléatoires. |
| `clockMs() → ms` | Millisecondes, pour mesurer le temps que prend une opération. |
| `luaProfile(frames [, label])` | Mesure quelles parties du script sont lentes (`luaProfile(0)` le désactive). |
| `createLine(name, points)`, `linePoint(id, s)` | Une courbe lisse fermée calculée par le moteur (parcours). |

Il n'y a pas de fonction « temps écoulé depuis le début » : additionnez `dt` dans une variable.
