# Limiti e consigli

## Quali parti di Lua mancano

- Ci sono `string`, `table` e `math`, più le funzioni di base (`print`, `pairs`, `ipairs`, `type`, `tostring`, `tonumber`, `pcall`, `assert`…).
- **Non** ci sono `io` né `os` (un gioco non tocca i file del disco; per questo non c'è `os.time`), né `require`, `coroutine`, `utf8` o `debug`. Per salvare dati usa `getConfig` / `saveConfig`.
- Ogni script è indipendente: non può vedere le variabili di un altro. Per comunicare, `setShared` / `getShared`.

## Errori

- Se una funzione ha un errore, viene annotato nel registro (`archivo.lua:LÍNEA: mensaje`) e quella chiamata si interrompe, ma viene richiamata nel fotogramma successivo: lo stesso errore si ripete a ogni fotogramma finché non lo correggi.
- Se il file non si può leggere o ha un errore di sintassi, quello script resta spento fino al prossimo Play.
- `error("...")` in Whisk3D si limita ad annotare il messaggio e lo script continua. Se vuoi interromperlo, usa `assert(condicion, "mensaje")`.

## Cose da tenere a mente

- `update` viene eseguita solo negli oggetti **visibili** della scena attiva: nascondere un oggetto spegne la sua logica.
- `destroy`, `changeScene` e `playScene` si applicano alla fine del fotogramma, non nel momento in cui le chiami.
- Il font dei testi del gioco non disegna accenti né la ñ: scrivi `"MENU"` invece di `"MENÚ"`.
- `safeArea`, `uiBox` e `resolution` hanno valori reali solo dopo il primo fotogramma disegnato: non usarle in `start`.
- Su Web e Android solo la **prima** luce illumina.
- Sul Nokia N95 non ci sono controller né mouse: usa `key` con i nomi della tastiera del telefono. I numeri interi sono a 32 bit.

## Perché vada veloce (soprattutto sul telefono)

- Cerca gli oggetti una sola volta, in `start`, e salvali in variabili `local`.
- Usa le versioni "per tanti elementi" (`setVertices`, `setVerticesColor`) invece di chiamare mille volte quella per un solo elemento.
- Preferisci le funzioni del motore (`scrollUV`, `linePoint`, `setRotation` con ordine) piuttosto che fare i calcoli in Lua.
- Misura con `luaProfile` e `clockMs` prima di ottimizzare alla cieca.
