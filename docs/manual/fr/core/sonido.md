# Son et musique

**W3dAudio** est un **mixeur logiciel** écrit en C++03, identique sur tous les systèmes (la même idée que celle qu'utilisait Quake). Il mixe jusqu'à 64 voix dans un flux stéréo 16 bits, avec interpolation linéaire (les changements de hauteur ne sonnent pas granuleux), un limiteur de crêtes, des enveloppes ADSR, de la réverbération et sa propre horloge, pour qu'un séquenceur de musique ne dépende pas du moment où l'image est calculée.

Chaque système n'apporte que la **sortie** : quatre fonctions (`W3dAudioBackendInit`, `Shutdown`, `Lock`, `Unlock`, dans `W3dAudioBackend.h`) qui ouvrent le périphérique et demandent au mixeur le morceau suivant.

| Système | Sortie |
|---|---|
| PC, Android, navigateur | SDL2 (44 100 Hz) |
| Symbian | `CMdaAudioOutputStream` dans son propre thread, avec double buffer (22 050 Hz sur le N95) |
| Consoles | L'éditeur compilé pour console n'a pas de son. Un jeu compilé avec « Utiliser le son » embarque le mixeur avec la sortie de SDL |
| Dreamcast | En plus, `w3dAICA` : un module facultatif qui utilise directement la puce sonore (64 canaux matériels et ADPCM 4 bits). Il ne cohabite pas avec W3dAudio |

Il y a trois autres pièces :

- **W3dMusic :** la musique longue (mp3, ogg, aac) n'est pas convertie en PCM : c'est le système qui la décode, presque toujours en matériel. Aujourd'hui, il a un backend pour le navigateur (un élément `<audio>`) ; ceux de Symbian et du bureau sont en attente.
- **W3dClip :** les sons courts de l'interface. Dans le navigateur, ils passent par WebAudio, car sur iOS les déclencher avec `<audio>` bloque le téléphone. Ailleurs, ce sont des sons du mixeur.
- **W3dVolumen :** le volume général et la sourdine s'appliquent à tout ce qui sonne et s'enregistrent tout seuls dans la configuration.

Pourquoi c'est important : le jeu Android sortait **muet** alors que les .wav étaient bien empaquetés dans l'APK, car ils étaient ouverts avec `fopen` et, là-dedans, ce ne sont pas des fichiers. Depuis que le son se charge via `ReadFileBytes`, il marche partout. Depuis Lua : [Son](../lua/api-sonido.md).
