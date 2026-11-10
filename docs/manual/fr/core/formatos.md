# Mathématiques, scène et formats

Des **vecteurs, matrices 4x4 et quaternions** maison (`math/`), qui compilent de la même façon sur PC et sur les systèmes rétro. Par-dessus, la **scène** (`objects/`) : objets avec hiérarchie parent-enfant, maillages, matériaux, lumières (directionnelle, ponctuelle et spot), caméras et squelettes. Et les **animations** (`animation/`) : images clés, squelettes, sommets, flipbooks et locomotion.

Les formats maison sont pensés pour qu'une petite machine les lise vite :

| Format | Ce que c'est |
|---|---|
| `.w3d` | Le projet : un ZIP standard (il s'ouvre avec n'importe quel décompresseur). Il s'écrit sans compression et se lit aussi compressé. Sans ZIP64 |
| `.w3dm` | La géométrie en texte, ligne par ligne : lisible, modifiable à la main et facile à lire pour le N95 |
| `.w3db` | Le même maillage en binaire, déjà prêt pour la mémoire : l'ouvrir revient à copier des tableaux |
| `.w3dt` | Une texture prête pour le GPU (16 bits et mipmaps déjà faits) |
| `.w3dpack` | Un conteneur d'assets chiffré avec ChaCha20 ; la clé est fournie par l'application |
| Pak intégré | Les fichiers d'un jeu compilé, obscurcis avec XOR. C'est contre les curieux, pas de la cryptographie |
| glTF / GLB | Un lecteur et un moteur de rendu dans un seul header (`io/w3dGltf.h`), à utiliser en dehors de l'éditeur |
