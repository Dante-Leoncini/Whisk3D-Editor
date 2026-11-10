# Suono e musica

**W3dAudio** è un **mixer software** scritto in C++03, identico su tutti i sistemi (la stessa idea che usava Quake). Mixa fino a 64 voci in un flusso stereo a 16 bit, con interpolazione lineare (i cambi di intonazione non suonano granulosi), un limitatore di picchi, inviluppi ADSR, riverbero e un orologio proprio, così un sequencer musicale non dipende da quando gira il fotogramma.

Ogni sistema fornisce solo l'**uscita**: quattro funzioni (`W3dAudioBackendInit`, `Shutdown`, `Lock`, `Unlock`, in `W3dAudioBackend.h`) che aprono il dispositivo e chiedono al mixer il pezzo successivo.

| Sistema | Uscita |
|---|---|
| PC, Android, browser | SDL2 (44.100 Hz) |
| Symbian | `CMdaAudioOutputStream` nel proprio thread, con doppio buffer (22.050 Hz sull'N95) |
| Console | L'editor compilato per console non ha suono. Un gioco compilato con "Usa suono" include il mixer con l'uscita di SDL |
| Dreamcast | In più, `w3dAICA`: un modulo opzionale che usa direttamente il chip audio (64 canali in hardware e ADPCM a 4 bit). Non convive con W3dAudio |

Ci sono altri tre pezzi:

- **W3dMusic:** la musica lunga (mp3, ogg, aac) non si converte in PCM: la decodifica il sistema, quasi sempre in hardware. Oggi ha un backend per il browser (un elemento `<audio>`); quelli per Symbian e desktop sono in sospeso.
- **W3dClip:** suoni brevi dell'interfaccia. Nel browser passano per WebAudio, perché su iOS riprodurli con `<audio>` blocca il telefono. Negli altri sistemi sono suoni del mixer.
- **W3dVolumen:** il volume generale e il silenzio valgono per tutto ciò che suona e si salvano da soli nella configurazione.

Perché è importante: il gioco per Android usciva **muto** con i .wav correttamente impacchettati dentro l'APK, perché venivano aperti con `fopen` e lì non sono file. Da quando il suono si carica con `ReadFileBytes`, funziona ovunque. Da Lua: [Suono](../lua/api-sonido.md).
