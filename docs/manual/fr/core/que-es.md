# Qu'est-ce que Whisk3D Core ?

**Whisk3D Core** est la couche qui sépare Whisk3D de chaque machine. C'est l'ingrédient secret : grâce à lui, le même code tourne sous **Windows**, **Linux**, **macOS**, **Android**, dans le **navigateur** (WebGL), sur les **Nokia N95** et **N8** (Symbian), la **Sega Dreamcast**, la **Sony PSP**, la **PS Vita** et la **Nintendo 3DS**.

Le Core est une bibliothèque C++ qui a son propre dépôt, [Whisk3D-Core](https://github.com/Dante-Leoncini/Whisk3D-Core), sous licence MIT. Sa conception s'inspire de moteurs classiques comme **RenderWare** : une API petite et stable en haut, et une implémentation différente pour chaque système en bas.

![L'architecture de Whisk3D en trois couches : en haut l'éditeur (vert), avec votre jeu ou projet en Lua à l'intérieur ; au milieu Whisk3D UI (jaune) ; en bas Whisk3D Core (rouge). L'éditeur et l'UI sont facultatifs : de l'extérieur, un jeu en C++ peut utiliser l'UI et un autre directement le Core. Le Core parle à chaque système par son propre backend](img/whisk3d-capas.webp)

## L'idée : demander, pas faire

Un PC avec une carte graphique moderne, un téléphone de 2007 et une Dreamcast dessinent de façons qui n'ont rien à voir. Le PC parle OpenGL ou Vulkan ; la PSP a sa propre puce (le GE) et une bibliothèque (sceGu) ; la 3DS utilise citro3d ; le N95, OpenGL ES 1.1. Si le jeu appelait chacun d'eux, il faudrait l'écrire onze fois.

Une **couche d'abstraction** règle ce problème. Le jeu ne dessine pas : il **demande**. Il dit « dessine ces triangles avec cette texture et cette lumière », et la pièce qui connaît ce système, le **backend**, sait comment le faire. C'est la même chose pour le son, les fichiers, le log et le réseau : une seule façon de demander et une implémentation par système.

Les règles de la maison :

- **Le code du dessus n'appelle jamais le système.** Ni OpenGL, ni sceGu, ni Symbian. Tout passe par des fonctions du Core.
- **Les headers n'exposent aucun type du système.** Une texture est un `unsigned int`, et les capacités sont des enums propres (`w3dEngine::DepthTest`, `w3dEngine::Blend`), pas ceux d'OpenGL. Ainsi, qui utilise le Core ne dépend d'aucun header graphique.
- **Chaque système apporte le minimum.** Son démarrage (la fenêtre, le contexte du GPU) et ce qui n'a pas sa place dans le code partagé. Dans le dépôt de l'éditeur, cela se trouve dans `platform/<sistema>/` et dans des blocs `#ifdef W3D_PSP`, `#ifdef W3D_3DS`, etc.

Un petit exemple de jeu écrit pour utiliser Whisk3D Core, qui fonctionne sur tous les systèmes :

```cpp
#include "graphics/w3dGraphics.h"   // dessiner
#include "graphics/w3dTexture.h"    // textures
#include "io/w3dFilesystem.h"       // fichiers
#include "base/w3dlog.h"            // log
#include "audio/W3dAudio.h"         // son

namespace gfx = w3dEngine;

// un fichier : il peut être sur le disque, dans le .w3d, dans l'APK ou empaqueté dans le jeu
std::vector<unsigned char> nivel;
if (!w3dFileSystem::ReadFileBytes("niveles/uno.txt", nivel))
    w3dLogfE("no encontre %s", "niveles/uno.txt");

// une texture : stb (ou l'ICL de Symbian) la décode et le backend l'envoie au GPU
unsigned int pasto = 0;
gfx::LoadTexture("texturas/pasto.png", pasto);

// un son : le Core le mixe ; la sortie dépend de chaque système
w3dEngine::W3dSound* salto = w3dEngine::W3dSoundLoad("sonidos/salto.wav");
w3dEngine::W3dSoundPlay(salto, 1.0f, false);

// dessiner : pas un seul appel à OpenGL, à sceGu ni à citro3d
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(pasto);
gfx::EnableArray(gfx::VertexArray);   gfx::VertexPointer3f(0, posiciones);
gfx::EnableArray(gfx::TexCoordArray); gfx::TexCoordPointer2f(0, uvs);
gfx::DrawTriangles(cantidadDeIndices, indices);
```

## Ce qu'il y a dedans

Chaque dossier du Core est un module :

| Dossier | Ce qu'il prend en charge |
|---|---|
| `graphics/` | L'API de dessin (`w3dEngine`), les textures, l'écran et les backends : [Graphismes](graficos.md) |
| `audio/` | Effets, musique et volume : [Son et musique](sonido.md) |
| `io/` | Fichiers, conteneurs (.w3d, zip, pak), maillages et ressources : [Fichiers](archivos.md) |
| `base/` | Le log, la configuration enregistrée et ce qui est commun à tous : [Logs](logs.md) |
| `input/` | Tactile, manettes, inclinaison, vibration et presse-papiers : [Entrées](entrada.md) |
| `net/` | Jouer en réseau : [Réseau](red.md) |
| `physics/` | Physique, collisions, hitbox, ragdolls et navigation : [Physique](fisica.md) |
| `script/` | Lua : [Scripts Lua](lua.md) |
| `math/` | Vecteurs, matrices et quaternions : [Mathématiques, scène et formats](formatos.md) |
| `objects/`, `animation/` | La scène : objets, maillages, matériaux, lumières, caméras et animations : [Mathématiques, scène et formats](formatos.md) |
| `tools/` | Le générateur de la table de répartition, le test d'indépendance et l'empaqueteur |
