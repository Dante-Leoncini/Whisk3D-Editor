# Som

| Função | O que faz |
|---|---|
| `beep(freq [, ms, vol])` | Um bipe (sem arquivo). |
| `sound("sounds/x.wav" [, vol, pitch, loop, pan]) → h` | Reproduz um som; devolve um identificador. |
| `sound("sounds/x.wav", {vol, pitch, loop, pan, attack, decay, sustain, fall, release, duration, reverb}) → h` | O mesmo com nomes, mais o envelope (ataque, decaimento, sustentação de 0 a 1, `fall`: a sustentação cai 60 dB nesses segundos, liberação e `duration`: a nota se solta sozinha depois desses segundos) e quanto do som vai para a reverberação (0 a 1). |
| `stopSound(h [, fundido])`, `stopSounds()`, `adjustSound(h, vol [, pitch])` | Interromper ou ajustar sons. |
| `releaseSound(h)` | Solta a nota: o envelope passa para a liberação. |
| `reverb(room, damping, level)` | A reverberação geral: tamanho da sala, amortecimento e nível (0 a 1 cada; nível 0 a desliga). |
| `mute(bool)`, `isMuted()` | Silêncio geral. |
| `playSound(obj [, seg])`, `muteSound(obj)` | Toca de novo um **objeto Som** (a partir de `seg` segundos do seu trecho) ou o cala. |
| `soundObject(obj) → t`, `setSoundObject(obj, t)` | Ler / mudar um objeto Som: `file` (a fonte), `active`, `volume`, `pitch`, `reverb`, `pan`, `start`, `clipStart`, `clipEnd`, `loop`, `positional`, `radius`, `range` (e ao ler `duration`, `playing`). |
| `createSound("sounds/x.wav" [, parent]) → obj` | Um objeto Som novo (`destroy(obj)` o remove). Num prefab é instanciado como qualquer objeto. |
