# Son

| Fonction | Ce qu'elle fait |
|---|---|
| `beep(freq [, ms, vol])` | Un bip (sans fichier). |
| `sound("sounds/x.wav" [, vol, pitch, loop, pan]) → h` | Joue un son ; renvoie un identifiant. |
| `sound("sounds/x.wav", {vol, pitch, loop, pan, attack, decay, sustain, fall, release, duration, reverb}) → h` | La même chose avec des noms, plus l'enveloppe (attaque, déclin, maintien de 0 à 1, `fall` : le maintien baisse de 60 dB en autant de secondes, relâchement, et `duration` : la note se relâche seule après autant de secondes) et la part du son envoyée à la réverbération (0 à 1). |
| `stopSound(h [, fundido])`, `stopSounds()`, `adjustSound(h, vol [, pitch])` | Arrêter ou ajuster des sons. |
| `releaseSound(h)` | Relâche la note : l'enveloppe passe à son relâchement. |
| `reverb(room, damping, level)` | La réverbération générale : taille de la salle, amortissement et niveau (de 0 à 1 ; niveau 0 l'éteint). |
| `mute(bool)`, `isMuted()` | Silence général. |
| `playSound(obj [, seg])`, `muteSound(obj)` | Relance un **objet Son** (à partir de `seg` secondes de son passage) ou le fait taire. |
| `soundObject(obj) → t`, `setSoundObject(obj, t)` | Lire / changer un objet Son : `file` (la source), `active`, `volume`, `pitch`, `reverb`, `pan`, `start`, `clipStart`, `clipEnd`, `loop`, `positional`, `radius`, `range` (et en lecture `duration`, `playing`). |
| `createSound("sounds/x.wav" [, parent]) → obj` | Un nouvel objet Son (`destroy(obj)` le retire). Dans un prefab, il s'instancie comme n'importe quel objet. |
