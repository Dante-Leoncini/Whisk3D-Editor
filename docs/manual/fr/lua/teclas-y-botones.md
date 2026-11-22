# Noms des touches et des boutons

## Pour `key` et `keyPressed`

| Où | Noms |
|---|---|
| PC, Android et Web | `"a"` … `"z"` (en minuscules), `"0"` … `"9"`, `"up"`, `"down"`, `"left"`, `"right"`, `"space"`, `"enter"`, `"shift"`, `"ctrl"` |
| Nokia N95 (clavier du téléphone) | `"up"`, `"down"`, `"left"`, `"right"`, `"enter"` (le centre des flèches envoie aussi `"space"`), `"softleft"`, `"softright"`, `"0"` … `"9"`, `"*"`, `"#"` |

> [!NOTE]
> Il n'y a pas de nom pour Esc : dans l'éditeur, Esc met le jeu en pause et libère la souris. Il n'y a pas non plus de touches F ni de Tab.

## Pour `button` et `buttonPressed` (manette)

`"a"`, `"b"`, `"x"`, `"y"`, `"up"`, `"down"`, `"left"`, `"right"` (la croix directionnelle). Les flèches de la croix déplacent aussi `stick("left")` quand le stick est immobile.

En outre : `"l1"`, `"r1"` (les boutons d'épaule), `"l2"`, `"r2"` (les gâchettes utilisées comme boutons : enfoncées à plus de la moitié ; pour la valeur analogique, il y a `trigger`), `"l3"`, `"r3"` (les sticks enfoncés), `"start"` et `"select"`. Les noms sont ceux d'une manette PlayStation ; sur une manette Xbox, ce sont LB / RB, LT / RT et Back. Les lettres suivent la position du bouton et non le symbole : `"a"` est celui du bas (la croix ×), `"b"` celui de droite (le rond ○), `"x"` celui de gauche (le carré □) et `"y"` celui du haut (le triangle △).
