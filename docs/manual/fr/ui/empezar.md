# Utiliser l'UI directement

Pour ceux qui programment en C ou en C++ et veulent l'interface sans l'éditeur. Il faut :

- **Le Core, avec son modèle de scène.** L'UI dessine avec `w3dEngine` et certaines lignes lisent des objets de la scène (`objects/`, `animation/`, `script/`). Voir [Utiliser le Core directement](../core/empezar.md).
- **Les sources de l'UI.** Tous les `.cpp` de `libs/WhiskUI/`. Les répertoires d'includes sont `libs/`, pour `"WhiskUI/draw/icons.h"`, et les sous-dossiers `widgets/`, `text/`, `draw/`, `theme/` et `core/`, car à l'intérieur ils s'incluent entre eux sans le dossier (`"icons.h"`, `"card.h"`).
- **Les fichiers du skin.** `font.png` (les lettres et les bords des cartes), `font-ext.png` (l'atlas étendu), `skin.ini` (les couleurs) et, pour les nouvelles icônes, le dossier `atlas/iconos/`. Ils ne sont pas fournis avec la bibliothèque : ils se trouvent dans le dépôt de l'éditeur, dans `res/Skins/Whisk3D/`.

## Démarrer

La même chose que ce que fait l'éditeur au démarrage, sans ce qui lui est propre :

```cpp
#include "w3dTexture.h"
#include "WhiskUI/core/UI.h"
#include "WhiskUI/theme/colores.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/font.h"
#include "WhiskUI/text/W3dFont.h"

loadColorsW3d("Skins/Whisk3D/skin.ini");    // sans le fichier, ce sont les couleurs du code qui restent

unsigned int atlas = 0; int w = 0, h = 0;
w3dEngine::LoadTexture("Skins/Whisk3D/font.png", atlas, &w, &h);

CalcCardUV(w, h);          // les bords arrondis
CrearIconos(w, h);         // les icônes qui se trouvent dans font.png
W3dFontSetTexSize(w, h);
SetIconScale(GlobalScale);

WhiskFont = new Font(w, h, atlas);
WhiskFont->SetScale(GlobalScale);
WhiskFont->SetAtlasExtendido("Skins/Whisk3D/font-ext.png");   // il n'est chargé que lorsqu'il le faut
```

`GlobalScale` démarre à 1. Pour une autre échelle, voir [Couleurs et échelle](tema.md). Pour les icônes séparées de `atlas/iconos/`, voir [Dessin et atlas](dibujo.md).

Chaque fois que la taille de la fenêtre change :

```cpp
W3dPantallaAlto = alto;    // pour convertir le Y de l'interface en celui de Scissor et Viewport
MenuPantallaW = ancho;     // pour que les menus ne sortent pas de l'écran
MenuPantallaH = alto;
```

## L'état 2D

L'UI dessine en pixels, avec le Y vers le bas et l'origine en haut à gauche. Chaque widget est dessiné à l'origine de la matrice courante : on le place avec `Translatef`. Avant de dessiner :

```cpp
namespace gfx = w3dEngine;

gfx::Viewport(0, 0, ancho, alto);
gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
gfx::Ortho(0, ancho, alto, 0, -1, 1);       // Y vers le bas
gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();
gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::CullFace);
gfx::Disable(gfx::Lighting);  gfx::Disable(gfx::Fog);
gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
gfx::DisableArray(gfx::NormalArray);
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(atlas);
gfx::TexFilter(false);                       // pixels nets
gfx::Enable(gfx::Blend); gfx::BlendAlpha();
```

## Une carte

Une carte avec un nombre, une case à cocher et un bouton. Chaque ligne pointe vers une de vos variables :

```cpp
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropButton.h"

static float volumen = 0.8f;
static bool  pantallaCompleta = false;
static void Salir() { /* ... */ }

GroupPropertie* opciones = new GroupPropertie("Opciones");

PropFloat* vol = new PropFloat("Volumen");
vol->value = &volumen;
vol->SetRango(0.0f, 1.0f);
opciones->properties.push_back(vol);

PropBool* pc = new PropBool("Pantalla completa");
pc->value = &pantallaCompleta;
opciones->properties.push_back(pc);

PropButton* salir = new PropButton("Salir", IconType::borrar);
salir->action = Salir;
opciones->properties.push_back(salir);

opciones->Resize(200 * GlobalScale, 0);    // la hauteur est calculée par la carte

// à chaque image, avec l'état 2D en place
gfx::PushMatrix();
gfx::Translatef(10, 10, 0);
opciones->Render();
gfx::PopMatrix();
```

L'UI ne lit ni le clavier ni la souris : c'est à vous de les lui transmettre. Avec le clavier :

```cpp
// flèche bas
if (opciones->editando) opciones->button_down();   // change la valeur
else opciones->NextSelect();                        // la ligne du dessous
// OK ou Enter : sur le titre, replie la carte ; sur une ligne, la modifie ou l'actionne
opciones->EnterPropertieSelect();
// Annuler : remet la ligne comme elle était
opciones->Cancel();
```

Avec la souris, c'est à vous de calculer la ligne sous le pointeur : voir [Cartes et propriétés](propiedades.md). Les menus, eux, reçoivent des coordonnées : voir [Menus déroulants](menus.md).

## L'état actuel, sans détour

- **Elle n'est pas prête pour la production.** Le `README.md` du dépôt le dit, et c'est le cas : elle contient du code de l'éditeur, qui est en cours de séparation.
- **Il n'y a pas d'exemple qui l'utilise seule.** Les exemples de `ui/` dans [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) sont des interfaces `.w3dui` avec du Lua exécuté par le runtime du jeu : cela relève de l'éditeur. Le code de cette page est construit avec les mêmes appels que ceux de l'éditeur ; il ne vient pas d'un exemple testé.
- **Aujourd'hui, elle ne se lie pas seule.** `PropList.cpp` appelle `RebindMaterialMeshPart()`, qui se trouve dans le panneau des propriétés de l'éditeur. Les fonctions de focus de `TextField.h` (`TextFieldEnfocar`, `TextFieldCancelar`, `TextFieldInputChar`, `TextFieldFinDeCuadro`) sont définies dans l'éditeur, dans `main/ui/ViewPorts/NumInput.cpp`.
- **Elle dépend du modèle de scène du Core.** `PropertieBase.h` inclut `objects/Objects.h`, et la liste de propriétés lit des maillages, des squelettes, des animations et des scripts.
- **C'est l'éditeur qui recalcule l'échelle.** `SetGlobalScale` se trouve dans `main/ui/ViewPorts/ViewPorts.cpp`. Seule, il faut recalculer les mesures à la main ([Couleurs et échelle](tema.md)).
- **Tout est global.** Il y a une police (`WhiskFont`), une table d'icônes (`IconsUV`), un menu ouvert (`MenuAbierto`) et un champ de texte ayant le focus (`g_textFieldActivo`) par programme.
- **Les headers exposent des types OpenGL** (`GLfloat`, `GLshort`, `GLuint`), même s'ils n'appellent plus OpenGL.
- **À venir,** d'après le `README.md` : nettoyer le code qui relève de l'éditeur et faire des exemples d'utilisation.

Ce qui relève de l'UI et ce qui relève de l'éditeur, point par point : [Ce qui relève de l'UI et ce qui relève de l'éditeur](ui-y-editor.md). Pour les questions, rendez-vous sur le groupe [Telegram](https://t.me/Whisk3D).
