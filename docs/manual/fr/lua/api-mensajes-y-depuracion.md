# Messages et débogage

| Fonction | Ce qu'elle fait |
|---|---|
| `print(...)` | Écrit dans le journal (log) du moteur, pas à l'écran. |
| `info(msg)`, `warning(msg)`, `error(msg)` | Messages d'information, d'avertissement ou d'erreur dans le journal. `error` n'interrompt **pas** le script. |
| `debugLog(msg)`, `isDebug()` | Messages uniquement dans les versions de débogage. |
| `logCount()`, `logLine(i)` | Lire le journal depuis le jeu (une console à vous). |
