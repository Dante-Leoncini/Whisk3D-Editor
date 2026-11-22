# Suono

| Funzione | Cosa fa |
|---|---|
| `beep(freq [, ms, vol])` | Un bip (senza file). |
| `sound("sounds/x.wav" [, vol, pitch, loop, pan]) → h` | Riproduce un suono; restituisce un identificatore. |
| `sound("sounds/x.wav", {vol, pitch, loop, pan, attack, decay, sustain, fall, release, duration, reverb}) → h` | Lo stesso con i nomi, più l'inviluppo (tempi in secondi; `sustain` da 0 a 1; `fall`: il sostegno scende di 60 dB in quei secondi; la nota si rilascia da sola dopo `duration` secondi) e quanto del suono va al riverbero (da 0 a 1). |
| `stopSound(h [, fundido])`, `stopSounds()`, `adjustSound(h, vol [, pitch])` | Interrompere o regolare i suoni. |
| `releaseSound(h)` | Rilascia la nota: l'inviluppo passa al rilascio. |
| `reverb(room, damping, level)` | Il riverbero generale (ogni valore da 0 a 1; `level` 0 lo spegne). |
| `mute(bool)`, `isMuted()` | Silenzio generale. |
| `playSound(obj [, seg])`, `muteSound(obj)` | Fa ripartire un **oggetto Suono** (da `seg` secondi del suo tratto) o lo zittisce. |
| `soundObject(obj) → t`, `setSoundObject(obj, t)` | Leggere / cambiare un oggetto Suono: `file` (la sorgente), `active`, `volume`, `pitch`, `reverb`, `pan`, `start`, `clipStart`, `clipEnd`, `loop`, `positional`, `radius`, `range` (e in lettura `duration`, `playing`). |
| `createSound("sounds/x.wav" [, parent]) → obj` | Un nuovo oggetto Suono (`destroy(obj)` lo toglie). In un prefab si istanzia come qualsiasi oggetto. |
