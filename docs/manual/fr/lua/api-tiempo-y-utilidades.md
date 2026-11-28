# Temps, hasard et utilitaires

| Fonction | Ce qu'elle fait |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Nombres aléatoires. |
| `clockMs() → ms` | Millisecondes, pour mesurer le temps que prend une opération. |
| `unixTime() → seconds, ms` | L'heure du système en temps Unix : les secondes depuis le 1/1/1970 en UTC, et à part les millisecondes (0–999). |
| `unixTimeMs() → ms`, `unixTimeSec() → seconds` | Le même temps Unix en un seul nombre : en millisecondes (entier) ou en secondes avec décimales. Mesurer, c'est une soustraction : `t = unixTimeSec() - debut`. Exacts avec Lua 64 bits (toutes les plateformes sauf la Dreamcast ; là, `unixTime()`). |
| `date([seconds [, utc]]) → table` | La date et l'heure d'un temps Unix (maintenant si on ne le passe pas), en heure locale, ou en UTC avec `utc = true` : `year`, `month`, `day`, `hour`, `min`, `sec`, `wday` (1 = dimanche), `yday`. |
| `luaProfile(frames [, label])` | Mesure quelles parties du script sont lentes (`luaProfile(0)` le désactive). |
| `createLine(name, points)`, `linePoint(id, s)` | Une courbe lisse fermée calculée par le moteur (parcours). |

Le temps écoulé depuis le début : gardez `unixTimeSec()` dans `start()` puis soustrayez-le (temps réel), ou additionnez `dt` dans une variable (temps de jeu).
