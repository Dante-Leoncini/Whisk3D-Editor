# Texte

## La police bitmap

Les lettres de l'interface sont dessinées à la main dans `font.png`. Chaque caractère est une cellule de **5x11** pixels et avance de **6** (`CharacterWidth`). Une ligne de texte mesure **12** (`RenglonHeight`). La police est à chasse fixe : tous les caractères occupent la même place. Avec l'échelle, tout est multiplié par `GlobalScale`.

`Font` (`text/font.h`) enregistre les coordonnées de chaque caractère. Il n'y en a qu'une, globale : `WhiskFont`. Elle se crée avec la taille de l'atlas et sa texture ([Utiliser l'UI directement](empezar.md)).

`font.png` contient les lettres, les chiffres et presque tous les signes de l'ASCII, plus `ñ`, `Ñ`, `¡`, `°` et la coche `✔`. Deux d'entre eux proviennent d'un autre caractère retourné : la `\` est la `/` en miroir et le `¿` est le `?` pivoté, un peu plus bas.

## Les accents

Les lettres accentuées ne sont pas dessinées : elles sont construites avec la lettre de base et le signe par-dessus.

- L'**accent aigu** et le **tilde** viennent de `font.png` : l'aigu est un morceau de la virgule, et le tilde est celui du `ñ`. Ce sont des bandes de 2 pixels, placées plus haut au-dessus d'une majuscule.
- Les autres signes (grave, circonflexe, tréma, point, brève, cédille et ogonek) sont dans l'atlas étendu.

## L'atlas étendu

`font-ext.png`, en 128x64, contient ce qui ne tient pas dans `font.png` :

- les signes des accents,
- `ß`, `ı`, `ł`, `Ł`, `œ`, `Œ`, `æ` et `Æ`,
- `;`, `^`, `~`, `` ` ``, `±`, `«`, `»`, `–`, `—` et `…`,
- le **cyrillique**. Les lettres identiques à une lettre latine (`А В Е К М Н О Р С Т Х` et `а е о р с у х`) utilisent celle de `font.png`.

Il n'est chargé que la première fois qu'un texte en a besoin : `SetAtlasExtendido` ne fait qu'enregistrer le chemin. En anglais ou en espagnol, il peut ne jamais être chargé, et sur le N95 la mémoire compte. Si le fichier manque, une lettre composée sort sans son signe et un caractère de l'atlas étendu sort sous la forme `?`.

Les tables, qui indiquent quelle cellule contient chaque caractère et comment se construit chaque lettre, sont dans `text/FuenteExt.h`. Elles ne se modifient pas à la main : elles sont générées par `tools/fuente_ext.py`, qui dessine aussi le PNG et se trouve dans le dépôt de l'éditeur.

Avec les deux textures, on couvre l'espagnol, le portugais, le français, l'allemand, l'italien, le polonais, le turc et le russe. Le japonais, le chinois et l'hindi ne sont pas couverts : ce qui n'est dans aucun atlas sort sous la forme `?`. `WhiskFont->Soporta(cp)` indique si un caractère peut être dessiné.

Le texte est en UTF-8. `UTF8_Char` lit des séquences de 1 à 3 octets. Une séquence de 4 octets, comme un emoji, n'est pas lue : elle sort sous forme de points d'interrogation.

## Dessiner un texte

```cpp
SetColorID(ColorID::blanco);
RenderBitmapText("Hola", textAlign::left, 200);   // 200 : la largeur maximale, en pixels
```

Il est dessiné à l'origine de la matrice courante, avec la couleur courante. Chaque texte représente un appel de dessin par atlas : un, ou deux s'il utilise l'atlas étendu. À la fin, la texture de `font.png` est remise en place, pour que les icônes qui suivent la trouvent.

## Mesurer et couper

La largeur d'un texte est le nombre de lettres multiplié par `CharacterWidthGS`. `UTF8_Largo(texto)` compte des lettres, pas des octets : un `ñ` occupe deux octets et compte pour une lettre.

S'il ne tient pas dans la largeur maximale, `RenderBitmapText` le coupe :

| Alignement | Où il se place | Ce qui est coupé |
|---|---|---|
| `textAlign::left` | Il commence à l'origine | La fin |
| `textAlign::right` | Il se termine à l'origine | Le début : dans un champ, on voit ce qui a été tapé en dernier |
| `textAlign::center` | Centré dans la largeur maximale | La fin |

`RenderBitmapEtiqueta(texto, ancho)` sert pour l'étiquette d'une ligne : elle est alignée à droite, mais si elle ne tient pas, elle coupe la fin et ajoute `..` (« Vectores de movimi.. »).

`RenderBitmapFloat(valor, alineación, ancho, unidad)` dessine un nombre avec quatre décimales et l'unité collée : `90.0000°`. Elle n'utilise pas `%f`, car le `printf` de Symbian ne formate pas les `float`.

## Texte avec des polices TTF

Pour des lettres de taille libre, il y a `w3dui::W3dTextAtlas` (`text/W3dTextAtlas.h`). C'est un simple header. Il dessine une police TTF déjà précalculée dans un atlas, et chaque lettre se met à l'échelle et se place toute seule : il sert pour les titres, les scores et les lettres animées. Il ne rastérise pas la TTF pendant l'exécution.

L'atlas se précalcule sur PC avec `text/bake_atlas.py`, qui a besoin de Pillow : `bake_atlas.py <fuente.ttf> <lado del atlas> <alto de letra> <salida>`. Le côté de l'atlas vaut 128, 256, 512 ou 1024. Il précalcule l'ASCII et `áéíóúÁÉÍÓÚñÑüÜ¿¡`. Il en sort `<salida>.png`, avec la couverture en niveaux de gris et sans alpha, et `<salida>.w3dfnt`, avec les mesures de chaque lettre.

```cpp
#include "WhiskUI/text/W3dTextAtlas.h"
#include "io/w3dFilesystem.h"

std::vector<unsigned char> fnt, png;
w3dFileSystem::ReadFileBytes("fuentes/titulo.w3dfnt", fnt);
w3dFileSystem::ReadFileBytes("fuentes/titulo.png", png);

w3dui::W3dTextAtlas titulo;
titulo.Load(&fnt[0], fnt.size(), &png[0], png.size());

titulo.Begin();                                                // atlas, mélange et état
titulo.DrawText("Puntaje: 120", 10, 10, 24, 1, 1, 1, 1);      // x, y, hauteur, r, g, b, a
float ancho = titulo.TextWidth("Puntaje: 120", 24);
```

`(x, y)` est le haut de la ligne, avec le Y vers le bas. `DrawText` regroupe toutes les lettres en un seul dessin. `DrawGlyph` en dessine une seule, pour des lettres de taille ou de couleur différentes. `LineHeight(alto)` donne la hauteur d'une ligne. Par défaut, il mélange en alpha prémultiplié (`MezclaPremult`) : le texte peut être de n'importe quelle couleur.

## La table sans GPU

`text/W3dFont.h` contient la même table que la police bitmap, sans toucher au GPU : où se trouve chaque lettre dans `font.png` (`W3dFontGetGlyphPx`) et ses coordonnées (`W3dFontGetGlyph`). Elle sert à construire un autre atlas avec les mêmes lettres.
