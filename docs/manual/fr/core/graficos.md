# Graphismes

## L'API

Tout ce qui se dessine passe par les fonctions de `w3dEngine` (`graphics/w3dGraphics.h`) : activer et désactiver des capacités (`Enable`, `Disable`), matrices (`MatrixMode`, `PushMatrix`, `Translatef`, `Perspective`), tableaux de sommets (`VertexPointer3f`, `NormalPointer3b`, `TexCoordPointer2f`), buffers sur le GPU (`GenBuffer`, `ArrayBufferData`), le dessin (`DrawTriangles`, `DrawLines`, `DrawPoints`), matériaux (`Material`), lumières (`LightNfv`, `LightNSpot`), brouillard, mélange, découpe par rectangle, stencil et plans de découpe.

Cela ressemble exprès à l'OpenGL classique : c'est la façon de dessiner que comprennent le plus de systèmes, du N95 à la Dreamcast.

## Pipeline fixe et shaders

Il existe deux familles de GPU :

- **Pipeline fixe.** Le GPU arrive avec les étapes déjà câblées d'usine : transformer les sommets, calculer la lumière par sommet, combiner la texture, appliquer le brouillard. Le programme ne fait qu'actionner des interrupteurs et passer des valeurs. C'est ainsi que fonctionnent OpenGL ES 1.1 (le N95), l'OpenGL classique du PC, le GE de la PSP et la PVR de la Dreamcast.
- **Shaders.** Le GPU n'a rien de câblé : chaque étape est un petit programme qui tourne dessus. C'est ainsi que fonctionnent OpenGL ES 2.0, WebGL et Vulkan.

Le Core parle la langue du pipeline fixe. Le backend à shaders l'**émule** avec un « uber-shader » qui fait les mêmes calculs qu'OpenGL ES 1.1 (lumière par sommet avec jusqu'à 8 lumières, atténuation, cône, brouillard, mélange de deux textures). C'est pourquoi le navigateur et Android ont le même rendu que le PC et le N95.

## Les backends

| Backend | Ce que c'est | Où il est utilisé |
|---|---|---|
| `opengl/` | Pipeline fixe : OpenGL de bureau, OpenGL ES 1.1, GLdc, vitaGL | Linux, Windows, macOS, N95 et S60, Dreamcast, PS Vita |
| `gles2/` | Shaders : OpenGL ES 2.0 et WebGL | Android, le navigateur, le Nokia N8 (Symbian^3), le PC (facultatif) |
| `vulkan/` | Le même backend à shaders, compilé sur un OpenGL ES 2.0 réalisé avec Vulkan | Le PC (facultatif) |
| `gu/` | Le GE de la PSP | PSP |
| `citro3d/` | Le GPU PICA200 de la 3DS | Nintendo 3DS |
| `pvr/` | Dessin direct sur la PVR de la Dreamcast | Module facultatif pour les jeux natifs |
| `video/` | Vidéo vers texture | Facultatif : FFmpeg sur le bureau, `<video>` dans le navigateur |

## Comment il est choisi

Presque toujours, **à la compilation** : chaque système compile un seul backend et les appels sont directs, sans aucun coût supplémentaire.

L'exception est le PC (Windows et Linux). Là, l'exécutable embarque OpenGL, OpenGL ES 2.0 et Vulkan, et le choix se fait **au démarrage**, dans **Propriétés → Réglages → Graphismes** (il s'applique au redémarrage ; voir [Autres fonctions](../editor/otras-funciones.md)). Chaque backend est compilé dans son propre namespace (`w3dEngine::gl`, `w3dEngine::es2`, `w3dEngine::vk`) et remplit une **table de répartition** (`w3dBackendTabla.h`) avec ses fonctions. Chaque fonction de `w3dEngine` appelle celle du backend actif :

```cpp
// graphics/w3dDespacho.inc, généré par tools/generar_despacho.py
void Enable(Cap c) { W3D_TABLA()->Enable(c); }
void DrawTriangles(int count, const MeshIndex* indices) { W3D_TABLA()->DrawTriangles(count, indices); }
```

Si celui qui a été choisi ne marche pas sur cette machine, c'est OpenGL qui reste. Vulkan a besoin du pilote, du GPU et du compilateur de shaders `shaderc_shared` à côté de l'exécutable, et il est désactivé dans les builds 32 bits. Sous macOS, l'exécutable n'embarque qu'OpenGL.

## Le voyage d'un `DrawTriangles`

La même demande aboutit à des endroits très différents :

![Un même gfx::DrawTriangles passe par Whisk3D Core et descend vers chaque backend : opengl (PC, macOS, N95, Dreamcast, PS Vita), gles2 (Android, navigateur, N8, PC), vulkan (PC), gu (PSP) et citro3d (3DS)](img/whisk3d-core-despacho.webp)

- **OpenGL fixe (PC, N95) :** presque directement vers `glDrawElements`. La matrice n'est envoyée que juste avant le dessin, en un seul `glLoadMatrixf`.
- **OpenGL ES 2.0 et WebGL :** la pile de matrices vit sur le CPU et voyage sous forme d'uniforms vers l'uber-shader. WebGL n'accepte pas de tableaux en RAM, donc le backend les passe dans des buffers. Sur le GPU VideoCore IV d'Android 2.3, il construit un petit programme par combinaison d'états et regroupe les dessins de l'interface : de 179 par image, on passe à 16.
- **PSP (GE) :** le CPU fait passer chaque sommet dans l'espace de découpe et découpe à ce stade, car le GE ne découpe pas sur les côtés. Ensuite, il les écrit entrelacés dans un anneau de mémoire que lit le GE. La lumière (jusqu'à quatre) et le brouillard sont gérés par le matériel.
- **3DS (PICA200) :** les sommets sont copiés en mémoire linéaire et transformés par un vertex shader maison (`w3d.v.pica`). Les petits dessins qui se suivent avec le même état partent ensemble : de 191 par image, on passe à 18. Les lignes et les points sont construits sur le CPU sous forme de triangles.
- **Dreamcast :** GLdc fait office d'OpenGL 1.x sur la PVR et utilise le même backend fixe que le PC et le N95, avec ses propres correctifs. Pour un jeu qui a besoin de chaque cycle, il y a `pvr/` : GLdc dépense environ 400 cycles du SH-4 par sommet, alors que le dessin direct transforme chacun avec une seule instruction (`FTRV`). Les deux ne cohabitent pas dans le même programme.
- **PS Vita :** vitaGL émule le pipeline fixe sur le GPU de la console et compile ses shaders pendant l'exécution (c'est pourquoi il a besoin de `libshacccg.suprx`).

## Demander au backend ce qu'il sait faire

Tous les GPU ne savent pas faire la même chose, et le Core ne le cache pas : chaque backend répond à ce qu'il possède, et celui qui dessine prend un autre chemin s'il manque quelque chose.

| Question | Ce qui se passe s'il répond non |
|---|---|
| `VBOSoportado()` | Les maillages passent sous forme de tableaux en RAM (Dreamcast, Vita) |
| `EstencilBits()` | Le reflet de l'eau se rabat sur la découpe par plans |
| `PlanosRecorteMax()` | Le reflet se débrouille sans plans (la PVR n'en a pas) |
| `MezclaTexturasSoportada()` | Les couches d'un matériau sont dessinées en plusieurs passes |
| `TieneTexGen()` | Le reflet sphérique est calculé par logiciel ou avec la matrice de texture (`TexMatrixMatcap`, qui marche bien sur le N95) |
| `SombreadoPorPixelSoportado()` | La lumière reste par sommet et l'éditeur ne propose même pas l'option |
| `ShadersEscritos()` | Le matériau avec un shader personnalisé est dessiné comme d'habitude (voir [Shaders écrits](../editor/shaders.md)) |

## Le cache d'état

Chaque changement d'état coûte quelque chose au pilote. Le Core se souvient de ce qui est en place : activer quelque chose de déjà activé, ou lier la texture déjà liée, ne génère aucun appel. `IsEnabled` et `GetMatrix` répondent depuis cette mémoire et n'interrogent pas le GPU, car sur le N95 une telle question oblige à attendre qu'il ait fini de dessiner.

## Textures

`LoadTexture` se fait en deux étapes. Le **décodage** (du fichier aux pixels RGBA) dépend du système : stb presque partout, l'ICL sous Symbian. L'**envoi** (des pixels vers le GPU) est commun, et chaque backend convertit au format de sa puce :

- **PSP :** puissance de deux jusqu'à 512, « swizzlée », en 565 ou 8888.
- **3DS :** blocs de 8x8 en ordre de Morton, de 8 à 1024 de côté.
- **Dreamcast :** puissance de deux de 8 à 1024, en 16 bits (565 ou 4444), sans mipmaps.
- **N95 :** uniquement des puissances de deux ; les autres sont rééchantillonnées avant l'envoi.

Un projet peut demander des textures en **16 bits** (la moitié de la mémoire) et, sur le N95 uniquement, les envoyer **à moitié de leur taille** à partir d'une certaine taille (`texturasMitadDesde`). Le format `.w3dt` enregistre une texture déjà prête pour le GPU, avec ses mipmaps : la charger revient à la lire et l'envoyer.

Un détail, en toute honnêteté : l'envoi des textures et les lumières appellent encore directement deux ou trois fonctions d'OpenGL. Sur les consoles sans OpenGL, le backend fournit une petite couche qui les implémente (`gu/w3dGLgu.cpp`, `citro3d/w3dGLc3d.cpp`).

## L'écran et la zone sûre

`PantallaSetTam` et `PantallaEsVertical` enregistrent une seule fois si l'écran est en portrait ou en paysage (paysage = plus large que haut), pour que n'importe quelle partie du moteur puisse le demander. La **zone sûre** (`SafeAreaGet`) indique combien de pixels de chaque bord ne peuvent pas servir à l'interface : l'encoche, la barre de gestes, les barres du navigateur sur mobile. Le fond est dessiné sur toute la fenêtre et l'interface à l'intérieur de la zone sûre. Sur PC, sous Symbian et sur les consoles, les bords valent 0 ; dans le navigateur, ils sont mesurés.

## Les limites du plus petit

Ce que l'on écrit pour le Core doit tenir sur la machine la plus limitée. Voici les limites qui se font sentir :

| Limite | Où | Comment le Core la gère |
|---|---|---|
| Indices 16 bits (jusqu'à 65 535 sommets par maillage) | Symbian | `MeshIndex` y est sur 16 bits et sur 32 ailleurs ; l'importateur refuse ce qui ne tient pas |
| 8 lumières à la fois | Tous (`LucesMax`) | Le backend à shaders les émule avec le même calcul ; la PSP en fait 4 par le matériel et Android 2.3 en utilise jusqu'à 4 |
| Pas de shaders | Les backends fixes | Les fonctions de shaders répondent non et le matériau est dessiné comme d'habitude |
| Z sur 16 bits | PSP | Le plan proche s'ajuste à la distance d'orbite, comme sur le N95 |
| 16 Mo pour tout | Dreamcast | On compile avec `-Os` et l'éditeur de liens élimine ce qui ne sert pas ; les textures sont en 16 bits |
| C++03 | Symbian | Pas de `nullptr`, `auto` ni `override` dans les headers (`W3D_OVERRIDE`) ; `tools/prueba_independencia.sh` compile chaque header seul, en C++03 |
