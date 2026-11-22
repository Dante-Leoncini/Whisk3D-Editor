# Messaggi e debug

| Funzione | Cosa fa |
|---|---|
| `print(...)` | Scrive nel registro (log) del motore, non sullo schermo. |
| `info(msg)`, `warning(msg)`, `error(msg)` | Messaggi di informazione, avviso o errore nel registro. `error` **non** interrompe lo script. |
| `debugLog(msg)`, `isDebug()` | Messaggi solo nelle versioni di debug. |
| `logCount()`, `logLine(i)` | Leggere il registro dal gioco (una console propria). |
