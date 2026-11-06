# Naviguer dans le 3D Viewport

> [!NOTE]
> Le Nokia N95 prend en charge une souris et un clavier (en Bluetooth). Les parties de ce tutoriel consacrées au Nokia expliquent comment l'utiliser sans souris ni clavier, uniquement avec les touches du téléphone.

Ce tutoriel explique comment naviguer dans le 3D Viewport. Naviguer, c'est déplacer votre point de vue : tourner autour de la scène, vous décaler sur le côté, vous rapprocher ou vous éloigner.

Savoir naviguer est important : pour créer des modèles 3D et des animations de façon confortable et rapide, il faut savoir se déplacer dans la scène et se placer dans une position confortable pour travailler.

Comme expliqué dans le [tutoriel précédent](viewports.md#le-viewport-actif), pour utiliser les commandes qui suivent, le 3D Viewport doit être le viewport actif, celui qui a la bordure verte.

Les trois gestes que vous utiliserez le plus sont **orbiter**, **déplacer la vue** et **zoomer**. Chacun se fait à la souris, avec les doigts sur un écran tactile ou avec le clavier du Nokia N95. Dans les vidéos sur PC, la souris en bas à droite montre en vert le bouton enfoncé.

## Orbiter

Orbiter, c'est faire tourner la vue autour de la scène, comme si vous faisiez le tour d'une table pour voir sous tous les angles ce qu'il y a dessus.

**À la souris :** maintenez enfoncé le **bouton du milieu** (on l'actionne en appuyant sur la molette) et faites glisser. Sur un trackpad, ou avec une souris sans bouton du milieu, maintenez <kbd>Alt</kbd> et faites glisser avec le **clic principal** (le gauche).

![Le pointeur sur le 3D Viewport : bouton du milieu enfoncé, le glissement fait tourner la vue autour du cube](img/navegacion-orbitar-raton.webp)

**Au doigt :** faites glisser un doigt sur le 3D Viewport. Commencez sur le fond : si vous le posez sur le gizmo (les flèches de couleur de l'objet), c'est le gizmo que vous allez déplacer.

![Un doigt glisse sur le fond du 3D Viewport et la vue tourne autour du cube](img/navegacion-orbitar-dedo.webp)

**Sur le Nokia N95 :** maintenez enfoncée une **flèche** du pavé directionnel. La vue tourne de ce côté tant que vous la maintenez.

![Un Nokia N95 à la verticale : avec les flèches droite, gauche, haut et bas enfoncées, la vue tourne autour du cube](img/navegacion-orbitar-nokia.webp)

## Déplacer la vue

Déplacer la vue (ou faire un *panoramique*), c'est la faire glisser sur le côté, vers le haut ou vers le bas, sans la faire tourner.

**À la souris :** maintenez <kbd>Shift</kbd> et faites glisser avec le **bouton du milieu**. Sur un trackpad : <kbd>Alt</kbd> + <kbd>Shift</kbd> et faites glisser avec le clic principal.

![Avec Shift et le bouton du milieu enfoncés, le pointeur décrit un cercle et la vue se déplace avec lui](img/navegacion-panear-raton.webp)

**Avec les doigts :** posez **deux doigts** et déplacez-les ensemble dans la même direction.

![Deux doigts décrivent un cercle ensemble et la vue se déplace](img/navegacion-panear-dedo.webp)

**Sur le Nokia N95 :** maintenez enfoncée la touche <kbd>\*</kbd> et utilisez les flèches.

![Un Nokia N95 avec la touche astérisque enfoncée : à chaque flèche, la vue se déplace de ce côté](img/navegacion-panear-nokia.webp)

## Zoom

Le zoom vous rapproche ou vous éloigne. Le pas s'adapte à la distance : près d'un petit objet, il avance lentement, et de loin, il avance vite. Il convient donc aussi bien pour regarder toute la scène que pour regarder un sommet.

**À la souris :** tournez la **molette**. Vers l'avant, elle rapproche ; vers l'arrière, elle éloigne. Sur un trackpad, faites glisser deux doigts vers le haut ou vers le bas.

![Le pointeur sur le cube : la molette vers l'avant rapproche la vue et vers l'arrière l'éloigne](img/navegacion-zoom-raton.webp)

**Avec les doigts :** pincez. Écarter les doigts rapproche, les resserrer éloigne.

![Deux doigts s'écartent et la vue se rapproche du cube ; ils se resserrent et elle s'éloigne](img/navegacion-zoom-dedo.webp)

**Sur le Nokia N95 :** maintenez enfoncé le <kbd>0</kbd> et utilisez la flèche **haut** pour vous rapprocher ou la flèche **bas** pour vous éloigner.

![Un Nokia N95 avec le 0 enfoncé : avec haut, la vue se rapproche du cube et avec bas, elle s'éloigne](img/navegacion-zoom-nokia.webp)

## Cadrer la sélection

Si vous êtes perdu, ou si vous voulez travailler sur quelque chose en particulier, **cadrez-le** : la vue se centre sur ce qui est sélectionné et ajuste le zoom pour que cela tienne tout juste. À partir de là, orbiter fait tourner la vue autour.

| | Comment cadrer |
|---|---|
| PC | <kbd>Num .</kbd> (le point du pavé numérique) |
| Nokia N95 | Un appui bref sur le <kbd>0</kbd>, sans flèche (maintenu avec une flèche, c'est le zoom) |
| Écran tactile | Double toucher à deux doigts |
| Menu | View → Frame Selected |

![La vue est loin et décalée, avec le cube tout petit dans un coin ; avec Num . le cube se retrouve centré et remplit la vue](img/navegacion-encuadrar.webp)

![Un Nokia N95 : avec un appui bref sur le 0, le cube qui était tout petit dans un coin se retrouve centré et cadré](img/navegacion-encuadrar-nokia.webp)

### En Edit Mode

En [Edit Mode](../editor/modo-edicion.md), c'est la partie sélectionnée du maillage qui est cadrée : les sommets, arêtes ou faces que vous avez choisis, pas l'objet entier. C'est utile pour travailler sur un détail d'un grand modèle : vous le sélectionnez, vous le cadrez et vous orbitez autour.

![Une sphère en Edit Mode avec six faces sélectionnées (en vert) : avec Num . la vue se rapproche et ces faces remplissent l'écran](img/navegacion-encuadrar-edicion.webp)

## La vue locale

Quand la scène contient beaucoup de choses, travailler sur l'une d'elles n'est pas pratique : celles de devant gênent, et quand vous orbitez, elles passent devant. La **vue locale** n'affiche que ce qui est sélectionné (un ou plusieurs objets) et le cadre. Le reste n'est ni supprimé ni masqué dans la scène : il n'est simplement plus affiché dans ce viewport. En haut à gauche apparaît l'étiquette « Local View ».

Pour revenir, refaites la même chose : tout réapparaît et la vue redevient comme vous l'aviez laissée.

| | Comment entrer et sortir |
|---|---|
| PC | <kbd>/</kbd> |
| Nokia N95 et écran tactile | Menu View → Local View |

![Une scène remplie de boîtes, de sphères, de cylindres et de cônes, avec un cône sélectionné : avec / il ne reste que le cône, cadré, et avec / une seconde fois tout revient](img/navegacion-vista-local.webp)

## Les vues de face, de côté et de dessus

Pour regarder la scène bien en face depuis l'un de ses côtés, sans régler l'orbite à la main :

| Vue | PC | Menu |
|---|---|---|
| Face / Arrière | <kbd>Num 1</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 1</kbd> | View → Viewpoint → Front / Back |
| Droite / Gauche | <kbd>Num 3</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 3</kbd> | View → Viewpoint → Right / Left |
| Dessus / Dessous | <kbd>Num 7</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 7</kbd> | View → Viewpoint → Top / Bottom |

Avec <kbd>Ctrl</kbd>, chaque touche donne la vue opposée.

![La scène remplie vue de face, de droite, de dessus et, avec Ctrl, depuis les vues opposées](img/navegacion-vistas.webp)

**Sur le Nokia N95 :** maintenez enfoncée la touche <kbd>#</kbd> et appuyez sur une flèche. Gauche et droite font tourner la vue de 90° en 90° autour de la scène. Haut passe à la vue de dessus ; bas revient à la vue de côté, et bas une seconde fois passe à la vue de dessous. Si vous orbitiez librement, la première flèche se contente de redresser la vue sur la plus proche.

![Un Nokia N95 avec la touche dièse enfoncée : avec la flèche droite, la vue tourne de 90 degrés en 90 degrés, avec haut elle passe à la vue de dessus et avec bas elle revient](img/navegacion-vistas-nokia.webp)

## Perspective et orthographique

Le 3D Viewport a deux façons d'afficher la scène :

- **Perspective :** comme la voit l'œil ou une caméra. Ce qui est loin paraît plus petit, et les lignes parallèles (les bords d'une rue, les rangées de boîtes de la vidéo) se rejoignent vers l'horizon. C'est ainsi que l'on voit vraiment, avec de la profondeur.
- **Orthographique :** sans perspective. Tout garde sa taille, que ce soit près ou loin, et les lignes parallèles restent parallèles. C'est comme un plan d'architecte ou un dessin technique.

La vue orthographique sert à modéliser avec précision : aligner des éléments, comparer des tailles et mesurer sans que la distance ne trompe. Combinée aux vues de face, de côté et de dessus, c'est comme travailler sur un plan. La perspective sert à voir à quoi ressemblera le résultat.

| | Comment basculer |
|---|---|
| PC | <kbd>Num 5</kbd> |
| Nokia N95 | Un appui bref sur le <kbd>#</kbd>, sans flèche |
| Menu | View → Perspective/Ortho |

![Deux rangées de boîtes identiques : en perspective, celles du fond paraissent petites ; avec Num 5, en orthographique, elles ont toutes la même taille](img/navegacion-perspectiva.webp)

![Un Nokia N95 : avec un appui bref sur le dièse, les rangées de boîtes passent de la perspective à l'orthographique, puis reviennent](img/navegacion-perspectiva-nokia.webp)

## La caméra active

La scène peut avoir plusieurs caméras, mais une seule est la **caméra active** : celle qui sert pour le jeu et pour les rendus. Ce que voit cette caméra, c'est ce que l'on voit en jouant et ce qui sort quand vous faites le rendu d'une image ou d'une vidéo. La vue avec laquelle vous naviguez ne sert qu'à travailler : elle n'apparaît ni dans le jeu ni dans les rendus.

Pour regarder à travers la caméra active :

| | Comment entrer et sortir |
|---|---|
| PC | <kbd>Num 0</kbd> |
| Nokia N95 | <kbd>8</kbd> (hors Edit Mode : là, le 8 est Loop Cut) |
| Menu | View → Cameras → Active Camera |

![Avec Num 0, le 3D Viewport regarde à travers la caméra, avec le cadre de ce qui sortira au rendu, et avec Num 0 une seconde fois il revient](img/navegacion-camara.webp)

![Un Nokia N95 : avec le 8, la vue passe à la caméra active et avec le 8 une seconde fois elle revient](img/navegacion-camara-nokia.webp)

Le rectangle est le cadrage : ce qui est à l'intérieur, c'est ce qui sort. En bas au centre, il y a un **cadenas** :

- **Ouvert** (au départ) : le zoom et le déplacement de la vue ne font que vous rapprocher pour vérifier le cadrage, sans toucher à la caméra. Orbiter vous fait sortir de la vue caméra, et vous continuez à naviguer depuis l'endroit où elle se trouvait.
- **Fermé :** orbiter, déplacer la vue et zoomer déplacent la **caméra**. C'est ainsi que l'on règle le plan.

Il s'ouvre et se ferme d'un toucher ou d'un clic.

Pour qu'une autre caméra devienne la caméra active : sélectionnez-la et appuyez sur <kbd>Ctrl</kbd> + <kbd>Num 0</kbd> (ou View → Cameras → Set Active Object as Camera).

## Autres options de la vue

| Option | Ce qu'elle fait | PC | Nokia N95 |
|---|---|---|---|
| Faire pivoter la vue | La fait tourner de 15° autour de son propre axe, comme quand on penche la tête. | <kbd>Num 4</kbd> / <kbd>Num 6</kbd> | — |
| Lock Orbit | La vue ne tourne plus : ce qui la faisait orbiter la déplace désormais, comme sur un plateau 2D. | View → Lock Orbit | <kbd>9</kbd> |

Tous les raccourcis de l'éditeur se trouvent dans [Raccourcis clavier](../editor/atajos.md), et la page [Navigation](../editor/navegacion.md) du manuel les résume dans des tableaux.
