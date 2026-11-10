# Qu'est-ce que Lua et à quoi sert-il dans Whisk3D ?

Lua est un langage de programmation petit, rapide et facile à apprendre. Whisk3D intègre **Lua 5.4** et l'utilise pour la *logique* des jeux : déplacer un personnage avec les touches, compter des points, ouvrir un menu, jouer un son quand quelque chose entre en collision.

Les scripts s'écrivent dans l'**éditeur de scripts** de Whisk3D (ou dans n'importe quel éditeur de texte) et s'enregistrent sous forme de fichiers `.lua`, normalement à l'intérieur du projet `.w3d` (dans son dossier `scripts/`). Ils ne s'exécutent que lorsque le jeu tourne : avec la Timeline en mode **Jeu** et en **Play**, ou dans un jeu compilé.
