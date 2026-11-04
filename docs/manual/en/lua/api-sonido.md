# Sound

| Function | What it does |
|---|---|
| `beep(freq [, ms, vol])` | A beep (no file needed). |
| `sonido("sonidos/x.wav" [, vol, tono, loop, pan]) → h` | Plays a sound; returns a handle. |
| `sonido("sonidos/x.wav", {vol, tono, loop, pan, ataque, decaimiento, sostenido, caida, liberacion, duracion, reverb}) → h` | The same with names, plus the envelope (attack, decay, sustain 0 to 1, `caida`: the sustain drops 60 dB in that many seconds, release, and `duracion`: the note releases itself after that many seconds) and how much of the sound goes to the reverb (0 to 1). |
| `pararSonido(h [, fundido])`, `pararSonidos()`, `ajustarSonido(h, vol [, tono])` | Stop or adjust sounds. |
| `soltarSonido(h)` | Releases the note: the envelope moves to its release. |
| `reverb(sala, amortiguacion, nivel)` | The global reverb: room size, damping and level (0 to 1 each; level 0 turns it off). |
| `silenciar(bool)`, `estaMudo()` | Mute everything. |
| `tocarSonido(obj [, seg])`, `callarSonido(obj)` | Start a **Sound object** again (from `seg` seconds of its section) or silence it. |
| `sonidoObj(obj) → t`, `setSonidoObj(obj, t)` | Read / change a Sound object: `archivo` (the source), `activo`, `volumen`, `tono`, `reverb`, `paneo`, `inicio`, `recorteIni`, `recorteFin`, `bucle`, `posicional`, `radio`, `alcance` (and `duracion`, `sonando` when reading). |
| `crearSonido("sonidos/x.wav" [, padre]) → obj` | A new Sound object (`destruir(obj)` removes it). In a prefab it is instantiated like any other object. |
