# Dessin et atlas

## Comment elle dessine

Tout passe par `w3dEngine` ([Graphismes](../core/graficos.md)). L'UI dessine des triangles (`DrawTrianglesArray`, `DrawTrianglesByte`) et des lignes, avec les sommets en `float`. Elle ne dessine ni avec `TRIANGLE_STRIP` ni avec des sommets `short` : le pilote du N95 ne les dessine pas pendant la phase 2D de l'image.

Les coordonnées sont des pixels d'écran, avec le Y vers le bas et l'origine en haut à gauche. Chaque widget est dessiné à l'origine de la matrice courante. Pour le placer, on utilise `Translatef`, entre `PushMatrix` et `PopMatrix`.

## Les fonctions de `glesdraw`

`draw/glesdraw.h` est le dessin de plus bas niveau, celui qu'utilisent tous les autres :

| Fonction | Ce qu'elle dessine |
|---|---|
| `W3dDrawStrip4(verts, uvs)` | Un carré : 4 sommets `GLshort` dans l'ordre d'une bande, avec leurs 8 UV. Elle le convertit en deux triangles |
| `W3dDrawElemsB(verts, uvs, idx, n)` | Un petit maillage avec des indices `GLubyte`, jusqu'à 64 |
| `W3dDrawLinesS(ancho, n, verts, idx)` | Jusqu'à 21 lignes, avec des sommets `GLshort` à 3 composantes |
| `W3dDrawLinesF(verts, idx, n)` | Des lignes avec des sommets `float` à 3 composantes, par paquets de 40 sommets par dessin |

Les sommets convertis en `float` vont dans un réservoir fixe de 256 places utilisé de façon circulaire, jamais sur la pile : le pilote du N95 lit les pointeurs tardivement.

`W3dPantallaAlto` est la hauteur de l'écran. C'est celui qui utilise l'UI qui la définit quand la taille change, et elle sert à convertir le Y de l'interface en celui de `Scissor` et `Viewport`, qui comptent depuis le bas.

## Nœuds et rectangles

`Object2D` (`draw/object2D.h`) est un nœud avec des enfants (`Childrens`) : il a une taille, une couleur, une opacité et `visible`. `Render()` le dessine, lui, puis ses enfants. Quand on le supprime, il supprime ses enfants.

`Rec2D` (`draw/rectangle.h`) est un rectangle d'une seule couleur. `SetSize(x, y, w, h)` le place et `SetColor(r, g, b)` lui donne sa couleur. Il ne lit pas la texture : il se dessine avec `Texture2D` désactivé.

```cpp
Rec2D velo;
velo.SetSize(0, 0, ancho, alto);
gfx::Disable(gfx::Texture2D);
gfx::Color4f(0.0f, 0.0f, 0.0f, 0.5f);
velo.RenderObject(false);          // false : avec la couleur courante, pas avec la sienne
gfx::Enable(gfx::Texture2D);
```

## Cartes

`Card` (`widgets/card.h`) est le rectangle à coins arrondis des boutons, des menus et des cartes de propriétés. C'est un **9-patch** : 16 sommets qui étirent le centre et laissent les coins à `2 * GlobalScale`. Les coins et le centre proviennent d'un recoin de `font.png`. `CalcCardUV(anchoAtlas, altoAtlas)` calcule ces coordonnées : on l'appelle une fois, avec la taille de la texture.

| Fonction | Ce qu'elle fait |
|---|---|
| `Resize(w, h)` | Change la taille |
| `RenderObject(propio)` | Dessine le remplissage |
| `RenderBorder(propio)` | Dessine seulement la bordure |
| `RenderObjectTab()`, `RenderBorderTab()` | La même chose, avec le côté du bas droit : pour les onglets |

Avec `propio` à `true`, elle utilise sa propre couleur (`color` et `opacity`) ; avec `false`, la couleur courante.

```cpp
Card fondo(NULL, 120, 40);
SetColorID(ColorID::gris);
fondo.RenderObject(false);
SetColorID(ColorID::accent);
fondo.RenderBorder(false);
```

## L'atlas du skin

Une seule texture pour toute l'interface. `font.png`, en 128x128, contient les lettres, les bords des cartes et les anciennes icônes. Avec cela seulement, l'UI fonctionne : `CrearIconos` prend les icônes à l'intérieur de `font.png`.

Les nouvelles icônes sont des fichiers séparés, dans `atlas/iconos/`. Au démarrage, l'éditeur construit une texture plus grande (`main/io/SkinAtlas.cpp`) : il place `font.png` entier en (0,0), ce qui évite de changer ses coordonnées, et empaquette chaque icône dans la place qui reste. Ensuite, il indique à l'UI où chacune a fini, avec `CrearIconosDesde`. Construire cet atlas relève de l'éditeur ; l'empaqueteur et la table d'icônes relèvent de l'UI. Voir [Icônes](iconos.md).

## L'empaqueteur

`W3dAtlasPack` (`draw/W3dAtlasPacker.h`) rassemble plusieurs images RGBA en une seule. Il ne touche pas au GPU : des pixels entrent et une toile sort, que l'appelant envoie au GPU. Il sert aussi bien pour un jeu : réunir les textures d'un niveau en une seule réduit les changements de texture.

```cpp
std::vector<W3dAtlasItem> items(n);       // pour chaque image : rgba, w, h
items[0].fijo = true;                     // celle-ci va obligatoirement là où on le demande
items[0].x = 0; items[0].y = 0;

std::vector<unsigned char> lienzo; int W = 0, H = 0;
if (W3dAtlasPack(&items[0], n, 2, lienzo, &W, &H)) {    // 2 pixels d'espace entre les images
    unsigned int tex = w3dEngine::UploadRGBA(&lienzo[0], W, H, true);
    // items[i].x, items[i].y : où chacune a fini
}
```

Il essaie des toiles de plus en plus grandes : 128x128, 256x128, 256x256, 512x256, 512x512 et 1024x512. Si les images ne tiennent pas même dans la dernière, il renvoie `false`. Les images libres sont placées en lignes, de gauche à droite, en sautant les fixes. L'espace entre les images évite que le filtrage linéaire mélange une image avec sa voisine.

## Ce qui reste d'avant

`draw/sprites.h` (`Sprite`) et `draw/image.h` (`Image`) ne sont inclus par personne. Ils utilisent des initialiseurs C++11, qui ne compilent pas sous Symbian.
