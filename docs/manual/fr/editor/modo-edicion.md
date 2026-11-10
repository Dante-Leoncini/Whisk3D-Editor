# Mode Édition (maillages)

On y entre avec <kbd>Tab</kbd> sur un maillage. On travaille sur les **sommets**, les **arêtes** ou les **faces** selon le bouton de la barre supérieure.

## Sélectionner

| Fonction | Description | Raccourci |
|---|---|---|
| Sélectionner | Choisit un sommet, une arête ou une face. | Clic ; <kbd>Shift</kbd>+clic alterne |
| Sélectionner lié | Tout l'îlot connecté sous le pointeur. | <kbd>L</kbd> |
| Sélection de boucle | Un anneau d'arêtes ou de faces. | <kbd>Shift+Alt</kbd>+clic |
| Chemin le plus court | Sélectionne le chemin entre l'élément actif et l'élément cliqué. | <kbd>Ctrl</kbd>+clic |
| Chemin le plus court (remplir région) | Sélectionne toute la région entre les deux. | <kbd>Ctrl+Shift</kbd>+clic |
| Tout / Aucun / Inverser / Sélection par boîte | Comme en Mode Objet, sur les éléments du maillage. | <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> / <kbd>B</kbd> |

## Modéliser

| Fonction | Description | Raccourci |
|---|---|---|
| Déplacer / Tourner / Échelle | Transforme la sélection. Pendant un déplacement, un nouvel appui sur <kbd>G</kbd> passe en glissement. | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| Rétrécir/Gonfler | Déplace chaque sommet le long de sa normale (gonfle ou dégonfle). | <kbd>Alt+S</kbd> |
| Extruder | Crée de la nouvelle géométrie à partir de la sélection (les faces sortent selon leur normale). | <kbd>E</kbd> |
| Coupe en boucle et glisser | Coupe un anneau d'arêtes. La molette change le nombre de coupes ; un clic applique, puis on fait glisser. | <kbd>Ctrl+R</kbd> |
| Nouvelle arête/face depuis sommets | Relie les sommets sélectionnés par une arête ou une face. | <kbd>F</kbd> |
| Connecter chemin de sommets | Coupe en ligne droite entre deux sommets. | <kbd>J</kbd> |
| Glisser sommets | Fait glisser les sommets le long de leurs arêtes. | <kbd>Shift+V</kbd> |
| Déchirer | Sépare le maillage le long de la sélection. | <kbd>V</kbd> |
| Dupliquer | Copie la sélection et la laisse saisie. | <kbd>Shift+D</kbd> |
| Séparer | Déplace les faces sélectionnées dans un nouvel objet. | <kbd>P</kbd> |
| Fusionner | Soude des sommets : Au centre, Au curseur, Effondrer ou Par distance. | <kbd>M</kbd> |
| Fusion auto | À la fin d'un déplacement, soude les sommets restés plus proches que le seuil. | Menu Maillage |
| Supprimer | Sommets, arêtes, faces ou boucles d'arêtes. | <kbd>X</kbd> ou <kbd>Delete</kbd> |
| Recalculer normales / Retourner | Corrige l'orientation des faces, ou les retourne. | <kbd>Shift+N</kbd> |
| Trianguler faces | Découpe en triangles les faces de plus de 3 côtés. | <kbd>Ctrl+T</kbd> |
| Ombrage lisse / Ombrage plat | La façon dont la surface est éclairée. | Menu Face |
| Marquer / Effacer vive | Arêtes qui paraissent dures même si le maillage est lisse. | Menu Arête |
| Marquer / Effacer couture | Endroits où le maillage est coupé lors du dépliage des UV. | Menu Arête ou UV |
| Menu contextuel | Les opérations du sous-mode actuel (sommet, arête ou face), sous le pointeur. | <kbd>W</kbd> ou clic droit |
| Insérer image clé | Enregistre la forme du maillage à la frame actuelle (animation de sommets). | <kbd>I</kbd> |

## Menu UV

| Fonction | Description | Raccourci |
|---|---|---|
| Déplier | Par angles, conforme ou étirement minimal : ouvre le maillage le long de ses coutures. | <kbd>U</kbd> |
| Projection UV intelligente | Coupe et déplie automatiquement selon les angles entre les faces. | <kbd>U</kbd> |
| Empaqueter lightmap / Suivre quads actifs | Îlots rangés sans chevauchement / UV ordonnées en suivant la face active. | <kbd>U</kbd> |
| Projection cube, cylindre, sphère, Projeter depuis la vue | Projections simples. | <kbd>U</kbd> |
| Réinitialiser | Chaque face occupe toute l'image. | <kbd>U</kbd> |

## Édition des courbes (chemins)

Avec <kbd>Tab</kbd> sur une courbe, on modifie ses nœuds comme s'il s'agissait d'un maillage de points : <kbd>G</kbd> déplace, <kbd>E</kbd> ajoute le nœud suivant, <kbd>X</kbd> supprime.
