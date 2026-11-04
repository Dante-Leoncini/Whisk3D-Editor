# Suono

| Funzione | Cosa fa |
|---|---|
| `beep(freq [, ms, vol])` | Un bip (senza file). |
| `sonido("sonidos/x.wav" [, vol, tono, loop, pan]) → h` | Riproduce un suono; restituisce un identificatore. |
| `sonido("sonidos/x.wav", {vol, tono, loop, pan, ataque, decaimiento, sostenido, caida, liberacion, duracion, reverb}) → h` | Lo stesso con i nomi, più l'inviluppo (attacco, decadimento, sostegno da 0 a 1, `caida`: il sostegno scende di 60 dB in quei secondi, rilascio, e `duracion`: la nota si rilascia da sola dopo quei secondi) e quanto del suono va al riverbero (da 0 a 1). |
| `pararSonido(h [, fundido])`, `pararSonidos()`, `ajustarSonido(h, vol [, tono])` | Interrompere o regolare i suoni. |
| `soltarSonido(h)` | Rilascia la nota: l'inviluppo passa al rilascio. |
| `reverb(sala, amortiguacion, nivel)` | Il riverbero generale: dimensione della sala, smorzamento e livello (da 0 a 1; livello 0 lo spegne). |
| `silenciar(bool)`, `estaMudo()` | Silenzio generale. |
| `tocarSonido(obj [, seg])`, `callarSonido(obj)` | Fa ripartire un **oggetto Sound** (da `seg` secondi del suo tratto) o lo zittisce. |
| `sonidoObj(obj) → t`, `setSonidoObj(obj, t)` | Leggere / cambiare un oggetto Sound: `archivo` (la sorgente), `activo`, `volumen`, `tono`, `reverb`, `paneo`, `inicio`, `recorteIni`, `recorteFin`, `bucle`, `posicional`, `radio`, `alcance` (e in lettura `duracion`, `sonando`). |
| `crearSonido("sonidos/x.wav" [, padre]) → obj` | Un nuovo oggetto Sound (`destruir(obj)` lo toglie). In un prefab si istanzia come qualsiasi oggetto. |
