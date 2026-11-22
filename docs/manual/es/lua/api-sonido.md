# Sonido

| Función | Qué hace |
|---|---|
| `beep(freq [, ms, vol])` | Un pitido (sin archivo). |
| `sound("sounds/x.wav" [, vol, pitch, loop, pan]) → h` | Reproduce un sonido; devuelve un identificador. |
| `sound("sounds/x.wav", {vol, pitch, loop, pan, attack, decay, sustain, fall, release, duration, reverb}) → h` | Lo mismo con nombres, más la envolvente (tiempos en segundos; `sustain` de 0 a 1; `fall`: el sostenido baja 60 dB en esos segundos; la nota se suelta sola a los `duration` segundos) y lo que el sonido manda a la reverb (0 a 1). |
| `stopSound(h [, fundido])`, `stopSounds()`, `adjustSound(h, vol [, pitch])` | Cortar o ajustar sonidos. |
| `releaseSound(h)` | Suelta la nota: la envolvente pasa a su liberación. |
| `reverb(room, damping, level)` | La reverb general (cada valor de 0 a 1; `level` 0 la apaga). |
| `mute(bool)`, `isMuted()` | Silencio general. |
| `playSound(obj [, seg])`, `muteSound(obj)` | Arranca de nuevo un **objeto Sonido** (desde `seg` segundos de su tramo) o lo calla. |
| `soundObject(obj) → t`, `setSoundObject(obj, t)` | Leer / cambiar un objeto Sonido: `file` (la fuente), `active`, `volume`, `pitch`, `reverb`, `pan`, `start`, `clipStart`, `clipEnd`, `loop`, `positional`, `radius`, `range` (y al leer `duration`, `playing`). |
| `createSound("sounds/x.wav" [, parent]) → obj` | Un objeto Sonido nuevo (`destroy(obj)` lo saca). En un prefab se instancia como cualquier objeto. |
