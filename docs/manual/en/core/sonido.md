# Sound and music

**W3dAudio** is a **software mixer** written in C++03, identical on every system (the same idea Quake used). It mixes up to 64 voices into a 16-bit stereo stream, with linear interpolation (pitch changes don't sound grainy), a peak limiter, ADSR envelopes, reverb and its own clock so that a music sequencer doesn't depend on when the frame runs.

Each system provides only the **output**: four functions (`W3dAudioBackendInit`, `Shutdown`, `Lock`, `Unlock`, in `W3dAudioBackend.h`) that open the device and ask the mixer for the next chunk.

| System | Output |
|---|---|
| PC, Android, browser | SDL2 (44,100 Hz) |
| Symbian | `CMdaAudioOutputStream` on its own thread, with double buffering (22,050 Hz on the N95) |
| Consoles | The editor built for a console has no sound. A game built with "Use sound" carries the mixer with SDL's output |
| Dreamcast | Also `w3dAICA`: an optional module that uses the sound chip directly (64 hardware channels and 4-bit ADPCM). It can't live alongside W3dAudio |

There are three more pieces:

- **W3dMusic:** long music (mp3, ogg, aac) isn't converted to PCM: the system decodes it, almost always in hardware. Today it has a backend for the browser (an `<audio>` element); the Symbian and desktop ones are pending.
- **W3dClip:** short interface sounds. In the browser they go through WebAudio, because on iOS firing them with `<audio>` freezes the phone. Everywhere else they're mixer sounds.
- **W3dVolumen:** the master volume and mute apply to everything that plays and are saved to the settings automatically.

Why it matters: the Android game came out **silent** even though the .wav files were properly packed inside the APK, because they were opened with `fopen` and in there they aren't files. Since sound loads through `ReadFileBytes`, it works everywhere. From Lua: [Sound](../lua/api-sonido.md).
