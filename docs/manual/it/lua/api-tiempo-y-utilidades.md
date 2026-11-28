# Tempo, casualità e utilità

| Funzione | Cosa fa |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Numeri casuali. |
| `clockMs() → ms` | Millisecondi, per misurare quanto tempo richiede qualcosa. |
| `unixTime() → seconds, ms` | L'ora del sistema come tempo Unix: i secondi dall'1/1/1970 in UTC, e a parte i millisecondi (0–999). |
| `unixTimeMs() → ms`, `unixTimeSec() → seconds` | Lo stesso tempo Unix in un solo numero: in millisecondi (intero) o in secondi con decimali. Misurare è una sottrazione: `t = unixTimeSec() - inizio`. Esatti con Lua a 64 bit (tutte le piattaforme tranne il Dreamcast; lì, `unixTime()`). |
| `date([seconds [, utc]]) → table` | Data e ora di un tempo Unix (adesso se non lo passi), locale, o in UTC con `utc = true`: `year`, `month`, `day`, `hour`, `min`, `sec`, `wday` (1 = domenica), `yday`. |
| `luaProfile(frames [, label])` | Misura quali parti dello script sono lente (`luaProfile(0)` lo spegne). |
| `createLine(name, points)`, `linePoint(id, s)` | Una curva morbida chiusa calcolata dal motore (percorsi). |

Il tempo dall'inizio: salva `unixTimeSec()` in `start()` e poi sottrailo (tempo reale), oppure somma `dt` in una variabile (tempo di gioco).
