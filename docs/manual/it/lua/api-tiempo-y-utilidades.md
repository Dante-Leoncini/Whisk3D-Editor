# Tempo, casualità e utilità

| Funzione | Cosa fa |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Numeri casuali. |
| `clockMs() → ms` | Millisecondi, per misurare quanto tempo richiede qualcosa. |
| `luaProfile(frames [, label])` | Misura quali parti dello script sono lente (`luaProfile(0)` lo spegne). |
| `createLine(name, points)`, `linePoint(id, s)` | Una curva morbida chiusa calcolata dal motore (percorsi). |

Non esiste una funzione "tempo trascorso dall'inizio": somma `dt` in una variabile.
