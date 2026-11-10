# Icônes

## Les fichiers

Chaque icône est un PNG de **10x10** dans `res/Skins/Whisk3D/atlas/iconos/`, dans le dépôt de l'éditeur. Il y en a 106 aujourd'hui. Les pixels sont blancs sur fond transparent : l'icône prend la couleur avec laquelle elle est dessinée. Ainsi, la même icône sert en gris, en blanc ou dans la couleur d'accent.

## L'enum et les noms

`draw/icons.h` contient l'enum `IconType` et le nombre total, `ICON_TOTAL` (106). Dans `draw/icons.cpp`, `KNombres` donne le nom de chacune, dans le même ordre : c'est le nom du fichier sans `.png`.

```cpp
IconoNombre(IconType::carpeta);   // "carpeta"
IconoIndice("carpeta");           // IconType::carpeta, ou -1 si elle n'existe pas
```

Presque toutes portent le même nom que dans l'enum. L'exception est `candadoAbierto`, qui correspond au fichier `candado_abierto.png`.

Beaucoup sont propres à l'éditeur : outils de modélisation, types d'objets, courbes d'animation. Les icônes générales sont là aussi : `arrow` et `arrowRight` (les flèches pour replier), `visible` et `hidden`, `carpeta`, `guardar`, `borrar`, `mas` et `menos`, `lista` et `cuadricula`, `candado`, `notifOk` et `notifError`, `tijera`, `copiar` et `pegar`.

## Les coordonnées

`IconsUV` est un `std::vector<IconUV*>`, avec les coordonnées de chaque icône dans l'atlas (`uvs`, 8 nombres). Il se remplit de deux façons :

- `CrearIconos(ancho, alto)` : avec la table indiquant où se trouve chaque icône dans `font.png` (`TablaLegacy`, dans `icons.cpp`). C'est la façon de faire sans le dossier `atlas/iconos/`.
- `CrearIconosDesde(rects, ancho, alto)` : avec une liste de rectangles fournie, celle de l'atlas construit. Un rectangle avec `x = -1` utilise celui de `font.png` : une icône sans son PNG continue d'apparaître.

`SetIconScale(escala)` ajuste `IconSizeGS` et les deux maillages : `IconMesh`, le carré d'une icône, et `IconLineMesh`, un carré de la hauteur d'une ligne de texte, pour les lignes des arborescences.

## Dessiner une icône

Avec l'atlas en place, à l'origine courante :

```cpp
SetColorID(ColorID::grisUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::carpeta]->uvs);
```

`Button`, `Tab`, `PopupMenu` et `GroupPropertie` reçoivent l'icône sous forme d'un `int` de l'enum, ou `-1` pour aucune.

## En ajouter une

1. Le PNG de 10x10, dans `atlas/iconos/`.
2. Une ligne à la fin de l'enum `IconType`, et ajouter un à `ICON_TOTAL`.
3. Son nom à la fin de `KNombres`.
4. Une ligne à la fin de `TablaLegacy`, avec l'icône de `font.png` qui la remplace si le PNG manque.

Sans la ligne 4, l'icône reste sans coordonnées quand il n'y a pas d'atlas construit.
