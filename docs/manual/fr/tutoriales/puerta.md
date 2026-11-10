# Changer l'origine : une porte

Dans ce tutoriel, vous allez faire en sorte qu'une porte s'ouvre depuis la charnière. Pour cela, il faut changer son **origine** : le point autour duquel l'objet tourne (voir [L'origine](modo-objeto.md#lorigine) dans le tutoriel du Mode Objet).

Nous partons de la porte du tutoriel précédent, [Modéliser une porte](modelar-puerta.md), avec l'origine au centre. Si vous l'avez faite en décalant un côté, l'origine s'est retrouvée décalée : **Objet → Définir l'origine → Origine à la géométrie** la ramène au centre.

## La porte tourne mal

Essayez de la faire tourner. La porte tourne sur son centre, comme une porte tambour, et non depuis la charnière :

![PC](img/icono-pc.svg) Avec le gizmo : un clic en son centre le fait passer en **rotation**, et l'anneau bleu fait tourner la porte autour de l'axe vertical.

![Un clic au centre du gizmo le fait passer en rotation ; on fait glisser l'anneau bleu et la porte tourne sur son centre, comme une porte tambour, puis revient](img/puerta-gira-mal-raton.webp)

![Android](img/icono-android.svg) Au doigt : un toucher au centre du gizmo le fait passer en **rotation**, et on fait glisser l'anneau bleu.

![Le doigt touche le centre du gizmo et le fait passer en rotation ; ensuite il fait glisser l'anneau bleu et la porte tourne sur son centre, puis revient](img/puerta-gira-mal-dedo.webp)

![Nokia](img/icono-nokia.svg) Sur le Nokia N95, le gizmo est désactivé au départ : <kbd>2</kbd> (faire pivoter), <kbd>3</kbd> (l'axe Z, l'axe vertical) et les flèches ; <kbd>OK</kbd> confirme.

![Un Nokia N95 : 2 et 3 lancent la rotation autour de l'axe vertical ; la flèche droite fait tourner la porte sur son centre, la gauche la ramène et OK confirme](img/puerta-gira-mal-nokia.webp)

Elle tourne mal parce que son origine est au centre. Nous allons la placer sur la charnière.

## Placer l'origine sur la charnière

1. **Passez en Mode Édition.** Un clic sur le bouton du mode, puis un autre sur **Mode Édition** (sur PC, aussi avec <kbd>Tab</kbd>).

   ![Le bouton du mode ouvert, avec Mode Édition mis en évidence](img/puerta-paso-1.webp)

2. **Choisissez les faces.** En Mode Édition apparaît le bouton **Sommet / Arête / Face** : un clic, puis choisissez **Face**.

   ![Le bouton Sommet, Arête, Face ouvert, avec Face mis en évidence](img/puerta-paso-2.webp)

3. **Sélectionnez la face de la charnière.** Un clic sur la face étroite du côté de la porte, du côté où se trouve la charnière : elle devient verte.

   ![La face étroite du côté de la porte, sélectionnée en vert et entourée d'un cercle](img/puerta-paso-3.webp)

4. **Amenez-y le curseur 3D.** Dans le menu **Maillage**, **Aimantation** → **Curseur sur le sélectionné** : le curseur 3D se place sur la charnière.

   ![Le menu Maillage ouvert, avec le sous-menu Aimantation et Curseur sur le sélectionné mis en évidence](img/puerta-paso-4.webp)

5. **Revenez en Mode Objet.** Un clic sur le bouton du mode, puis un autre sur **Mode Objet** (ou <kbd>Tab</kbd>).

   ![Le bouton du mode ouvert, avec Mode Objet mis en évidence ; le curseur 3D est déjà sur la charnière](img/puerta-paso-5.webp)

6. **Amenez l'origine au curseur 3D.** Dans le menu **Objet**, **Définir l'origine** → **Origine au curseur 3D**. La porte ne bouge pas : c'est son origine qui se déplace, et avec elle le gizmo, qui se retrouve sur la charnière.

   ![Le menu Objet ouvert, avec le sous-menu Définir l'origine et Origine au curseur 3D mis en évidence](img/puerta-paso-6.webp)

## Maintenant, elle tourne bien

Faites-la tourner de nouveau avec le gizmo : elle s'ouvre et se ferme maintenant depuis la charnière.

![Avec l'origine sur la charnière, l'anneau bleu du gizmo ouvre la porte jusqu'à ce qu'elle soit de profil, puis la referme](img/puerta-gira-bien-raton.webp)

![Au doigt, l'anneau bleu ouvre la porte depuis la charnière, puis la referme](img/puerta-gira-bien-dedo.webp)

![Un Nokia N95 : 2, 3 et la flèche droite ouvrent la porte depuis la charnière ; la gauche la ferme et OK confirme](img/puerta-gira-bien-nokia.webp)

![Android](img/icono-android.svg) Sur un écran tactile, les étapes ci-dessus sont les mêmes, avec des touchers : les menus s'ouvrent et les options se choisissent d'un toucher.

![Nokia](img/icono-nokia.svg) Sur le Nokia N95, <kbd>OK</kbd> passe du Mode Objet au Mode Édition et les menus s'ouvrent avec la touche de sélection gauche.

## Les options de Définir l'origine

| Option | Ce qu'elle fait |
|---|---|
| **Origine à la géométrie** | Amène l'origine au centre du maillage. |
| **Origine au curseur 3D** | Amène l'origine là où se trouve le curseur 3D. |
| **Géométrie à l'origine** | L'inverse : laisse l'origine en place et déplace le maillage jusqu'à ce que son centre coïncide avec elle. |

Dans le prochain tutoriel, nous lui ajoutons la poignée : [Modéliser la poignée](modelar-picaporte.md).
