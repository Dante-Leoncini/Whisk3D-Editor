# Matematica, scena e formati

**Vettori, matrici 4x4 e quaternioni** propri (`math/`), che compilano allo stesso modo sul PC e sui sistemi retro. Sopra, la **scena** (`objects/`): oggetti con gerarchia genitore e figlio, mesh, materiali, luci (direzionale, puntiforme e spot), camere e armature. E le **animazioni** (`animation/`): keyframe, armature, vertici, flipbook e locomozione.

I formati propri sono pensati perché una macchina piccola li legga in fretta:

| Formato | Cos'è |
|---|---|
| `.w3d` | Il progetto: uno ZIP standard (si apre con qualsiasi programma di decompressione). Si scrive senza compressione e si legge anche se compresso. Senza ZIP64 |
| `.w3dm` | La geometria in testo, riga per riga: leggibile, modificabile a mano e facile da leggere per l'N95 |
| `.w3db` | La stessa mesh in binario, già pronta per la memoria: aprirla significa copiare array |
| `.w3dt` | Una texture pronta per la GPU (16 bit e mipmap già fatte) |
| `.w3dpack` | Un contenitore di asset cifrato con ChaCha20; la chiave la mette l'applicazione |
| Pak incorporato | I file di un gioco compilato, offuscati con XOR. Serve contro i curiosi, non è crittografia |
| glTF / GLB | Un lettore e renderer in un solo header (`io/w3dGltf.h`), da usare fuori dall'editor |
