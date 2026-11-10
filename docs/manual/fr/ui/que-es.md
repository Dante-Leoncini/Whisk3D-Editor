# Qu'est-ce que Whisk3D UI ?

**Whisk3D UI** est une bibliothèque C++ pour construire des interfaces : boutons, onglets, menus déroulants, cartes de propriétés, texte et icônes. Elle a son propre dépôt, [Whisk3D-UI](https://github.com/Dante-Leoncini/Whisk3D-UI), sous licence MIT. Elle est construite sur [Whisk3D Core](../core/README.md), et l'éditeur de Whisk3D est fait avec elle.

![L'architecture de Whisk3D : l'éditeur et Whisk3D UI, facultatifs, sur Whisk3D Core, qui parle à chaque système par son propre backend](img/whisk3d-capas.webp)

## Où elle tourne

Sur les mêmes systèmes que le Core : **Windows**, **Linux**, **macOS**, **Android**, le **navigateur**, les **Nokia N95** et **N8**, la **Sega Dreamcast**, la **Sony PSP**, la **PS Vita** et la **Nintendo 3DS**. L'éditeur la compile sur tous.

L'UI n'a pas de code propre à chaque système. Elle n'appelle pas OpenGL : elle dessine tout avec les fonctions de `w3dEngine` ([Graphismes](../core/graficos.md)) et le backend de chaque système fait le reste. Le peu qui change relève de la compilation :

- Les headers incluent `GL/gl.h`, ou `GLES/gl.h` sous Symbian, pour les types (`GLfloat`, `GLshort`, `GLubyte`).
- Sur Android, `loadColorsW3d` met à jour la table de couleurs en octets, mais pas celle en `float`.
- Le code est en C++03, car le compilateur de Symbian ne connaît pas C++11 : les enums sont placés dans une `struct` (`IconType::camera`) et on utilise `W3D_OVERRIDE` au lieu de `override`.

## Pourquoi des atlas et pas des vecteurs

Presque toutes les bibliothèques d'interface dessinent avec des vecteurs. Beaucoup de vieux appareils n'accélèrent pas cela : l'interface est dessinée par logiciel et elle est lente. Ce que des appareils comme le N95 accélèrent, c'est la 3D : des triangles texturés.

C'est pourquoi Whisk3D UI dessine tout avec des triangles qui lisent une texture, un **atlas**. Les lettres, les bords arrondis des cartes et les icônes sont dans la même texture : un seul `BindTexture` suffit pour toute l'interface. Un texte se dessine avec un appel par atlas, pas avec un appel par lettre.

Le prix à payer : les lettres sont en pixels et grandissent par multiples entiers ([Couleurs et échelle](tema.md)), et il n'y a que les lettres dessinées dans l'atlas ([Texte](texto.md)).

## Ce qu'il y a dedans

| Dossier | Ce qu'il prend en charge |
|---|---|
| `core/` | Les mesures de l'interface et leur échelle (`UI.h`) : [Couleurs et échelle](tema.md) |
| `theme/` | La palette de couleurs (`colores.h`) : [Couleurs et échelle](tema.md) |
| `draw/` | Le dessin 2D (`glesdraw`), les rectangles, les icônes et l'empaqueteur d'atlas : [Dessin et atlas](dibujo.md), [Icônes](iconos.md) |
| `text/` | La police bitmap, l'atlas étendu et le texte avec des polices TTF précalculées : [Texte](texto.md) |
| `widgets/` | Les cartes avec bordure, les boutons, les onglets, les menus et les champs de texte : [Boutons, onglets et cases à cocher](botones.md), [Menus déroulants](menus.md), [Entrées et champs de texte](entrada.md) |
| `Propieties/` | Les cartes de propriétés et leurs lignes : [Cartes et propriétés](propiedades.md) |
