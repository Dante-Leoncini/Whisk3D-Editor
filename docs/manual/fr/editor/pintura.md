# Modes de peinture

> [!NOTE]
> Les trois modes de peinture sont **réservés aux maillages 3D** : ils n'apparaissent pas avec les caméras, les lumières, les armatures, les courbes ni les éléments 2D. Pendant la peinture, le clic ne change pas la sélection ; pour peindre un autre objet, choisissez-le dans l'Outliner.

## Ce qui est commun aux trois

| Fonction | Description | Utilisation |
|---|---|---|
| Peindre | Peint uniquement les faces visibles tournées vers vous (avec Rayons X, aussi celles de derrière). Chaque trait est une étape d'annulation. | Cliquer et glisser |
| Rayon | La taille du pinceau en pixels. Quand vous la modifiez, un cercle apparaît au centre de la vue avec la taille réelle. | Barre déplaçable au-dessus de la barre d'outils |
| Valeur | L'intensité du pinceau. | Barre déplaçable |
| Atténuation | La façon dont le bord du pinceau est adouci ; vous pouvez dessiner votre propre courbe. | Bouton d'atténuation |
| Vue (tactile) | Activé : le doigt navigue. Désactivé : le doigt peint. | Bouton en forme d'écran |

## Peinture de poids

Peint l'influence d'un os sur chaque sommet (bleu = aucune, rouge = totale). C'est ce qui permet au maillage de bien se déformer avec l'armature.

| Fonction | Description |
|---|---|
| Mode du pinceau | Ajouter (+), Soustraire (−) ou Égaliser (=, applique la valeur exacte). |
| Groupe de sommets | Le groupe (os) que l'on peint ; on peut ajouter des groupes. S'il n'y en a aucun, le premier trait en crée un. |
| Repères | Affiche un petit carré par sommet ; le pinceau n'agit que sur eux. |
| Sélection uniquement | Protège ce qui n'est pas sélectionné (affiché assombri). |

## Peinture de sommets

Peint des couleurs sur les sommets d'un **maillage 3D** (ne fonctionne pas pour les images ni les éléments 2D : pour cela, il y a la Peinture de texture). C'est très léger et c'est la méthode recommandée pour mettre de la couleur sur le N95.

| Fonction | Description |
|---|---|
| Couleur | Ouvre le sélecteur de couleur (RGB, HSV, hexadécimal ou un index de la palette). |
| Par sommet / Par coin | Par sommet, la couleur est partagée entre les faces ; par coin, chaque face peut avoir la sienne (bords nets). |
| Couche de couleur | Choisir ou créer des couches de couleur. |
| Repères / Sélection uniquement | Comme dans la Peinture de poids. |

## Peinture de texture

Peint directement sur l'objet 3D, et le trait est reporté sur sa texture.

| Fonction | Description |
|---|---|
| Texture automatique | Si le maillage n'a pas de texture, elle est créée automatiquement au premier trait : un matériau propre, des UV automatiques et une image de 1024×1024 (256×256 sur le N95) de la couleur du matériau. |
| Couleur, Atténuation, Rayon, Valeur | Comme dans les autres modes. |
| Enregistrer texture | La peinture reste en mémoire jusqu'à ce qu'elle soit enregistrée. La disquette devient verte s'il y a des modifications non enregistrées. Options : Enregistrer, Enregistrer sous…, Enregistrer dans le .w3d et Enregistrer hors du .w3d… |
