# Ce qui relève de l'UI et ce qui relève de l'éditeur

L'éditeur est construit avec Whisk3D UI, mais une grande partie de ce qu'on y voit lui est propre. Ce qui relève de l'UI se trouve dans `libs/WhiskUI/` ; ce qui relève de l'éditeur, dans `main/` du dépôt de Whisk3D.

## Ce qui relève de l'UI

- Le dessin 2D, les cartes avec bordure et les rectangles.
- La table d'icônes et l'empaqueteur d'atlas.
- La police bitmap, l'atlas étendu et le texte avec des polices TTF précalculées.
- La palette de couleurs et les mesures, avec leur échelle.
- Les boutons, les onglets, les cases à cocher et les menus déroulants.
- Les cartes de propriétés et leurs lignes, avec le losange d'animation.
- L'état des champs de texte et leur dessin.

## Ce qui relève de l'éditeur

| Quoi | Où |
|---|---|
| Les viewports : diviser l'écran, leurs bordures, leurs barres de menus et barres d'outils | `main/ui/ViewPorts/` (`ViewPorts.cpp`, `WithBorder.cpp`, `ToolbarBase.cpp`) |
| Le panneau des propriétés : quelles cartes il contient, ses onglets, le clic et le glissement sur les lignes | `main/ui/ViewPorts/Properties.cpp` |
| Les fenêtres modales : le sélecteur de couleur, l'explorateur de fichiers, les confirmations, la barre de progression, les claviers à l'écran | `main/ui/ViewPorts/PopUp/` |
| Les notifications | `main/ui/ViewPorts/Notificaciones.cpp` |
| Les barres de défilement des panneaux | `main/ui/ViewPorts/ScrollBar.cpp` |
| Les infobulles. Le texte est conservé dans `Button::ayuda` | `W3dAyudaRender`, dans `main/ui/ViewPorts/LayoutInput.cpp` |
| Quelle touche, quel clic ou quel toucher va à chaque widget, et les touches du N95 | `main/ui/ViewPorts/LayoutInput.cpp`, `main/app/controles.cpp`, `platform/symbian/src/` |
| Les fonctions de focus des champs de texte | `main/ui/ViewPorts/NumInput.cpp` |
| Les traductions, `T()`. L'UI n'a que le point d'accroche `W3dMenuTraducir` | `main/config/W3dLang.cpp` |
| Le démarrage de l'UI | `main/app/W3dInitUI.cpp` |
| Construire l'atlas du skin avec les icônes séparées | `main/io/SkinAtlas.cpp` |
| Recalculer l'échelle | `SetGlobalScale`, dans `main/ui/ViewPorts/ViewPorts.cpp` |
| Lire `skin.ini` sur PC et sur Android | `loadColors`, dans `main/app/main.cpp` |
| Les couleurs propres à l'éditeur : le sol, les axes, les normales | `main/ui/W3dColors.cpp` |
| Les fichiers du skin : `font.png`, `font-ext.png`, `atlas/iconos/`, `skin.ini` | `res/Skins/Whisk3D/` |
| Le générateur de l'atlas étendu | `tools/fuente_ext.py` |
| L'[Éditeur 2D](../editor/editor-2d.md) et les interfaces `.w3dui` des jeux : textes, images, rectangles, conteneurs, slice 9, boutons et vidéos | `main/objects/` (`UI.h`, `Texto2D.h`, `Boton2D.h`...), `main/render/UIOverlay.cpp`, `main/io/UI2DFormato.cpp`, `main/io/Fuente2D.cpp`, `main/ui/ViewPorts/Editor2D.cpp` |

L'Éditeur 2D dessine le texte avec `W3dTextAtlas`, qui relève de l'UI. Précalculer une TTF pendant l'exécution, avec stb_truetype, relève de l'éditeur (`main/io/Fuente2D.cpp`).

## Dans l'UI, mais propre à l'éditeur

Des éléments qui se trouvent dans `libs/WhiskUI/` et qui relèvent encore de l'éditeur :

- `PropListMeshParts` lit des maillages, des squelettes, des animations, des contraintes et des scripts, et appelle `RebindMaterialMeshPart()` du panneau des propriétés.
- `PropColor` conserve la palette et l'objet d'où vient la couleur (`palRef`, `palObj`), pour le sélecteur de couleur.
- `GroupProperties` est la liste globale des cartes du panneau des propriétés.
- `core/UI.h` contient de l'état de l'éditeur : le mode de vue tactile du Mode Édition (`g_viewEditMode`), le mouvement de la souris (`dx`, `dy`), les touches Alt, Shift et Ctrl (`LAltPressed`, `LShiftPressed`, `LCtrlPressed`) et les mesures des viewports (`paddingViewport`, `MinViewportHeight`, `MinViewportWidth`).
- La plupart des icônes sont des outils de l'éditeur.

Ce qui reste à séparer, et l'état de la bibliothèque : la fin de [Utiliser l'UI directement](empezar.md).
