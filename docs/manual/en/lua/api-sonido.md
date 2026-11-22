# Sound

| Function | What it does |
|---|---|
| `beep(freq [, ms, vol])` | A beep (no file needed). |
| `sound("sounds/x.wav" [, vol, pitch, loop, pan]) → h` | Plays a sound; returns a handle. |
| `sound("sounds/x.wav", {vol, pitch, loop, pan, attack, decay, sustain, fall, release, duration, reverb}) → h` | The same with names, plus the envelope (attack, decay, sustain 0 to 1, `fall`: the sustain drops 60 dB in that many seconds, release, and `duration`: the note releases itself after that many seconds) and how much of the sound goes to the reverb (0 to 1). |
| `stopSound(h [, fundido])`, `stopSounds()`, `adjustSound(h, vol [, pitch])` | Stop or adjust sounds. |
| `releaseSound(h)` | Releases the note: the envelope moves to its release. |
| `reverb(room, damping, level)` | The global reverb: room size, damping and level (0 to 1 each; level 0 turns it off). |
| `mute(bool)`, `isMuted()` | Mute everything. |
| `playSound(obj [, seg])`, `muteSound(obj)` | Start a **Sound object** again (from `seg` seconds of its section) or silence it. |
| `soundObject(obj) → t`, `setSoundObject(obj, t)` | Read / change a Sound object: `file` (the source), `active`, `volume`, `pitch`, `reverb`, `pan`, `start`, `clipStart`, `clipEnd`, `loop`, `positional`, `radius`, `range` (and `duration`, `playing` when reading). |
| `createSound("sounds/x.wav" [, parent]) → obj` | A new Sound object (`destroy(obj)` removes it). In a prefab it is instantiated like any other object. |
