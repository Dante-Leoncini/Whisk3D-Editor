# Scènes et cinématiques

| Fonction | Ce qu'elle fait |
|---|---|
| `changeScene(name [, restart])` | Passe à une autre scène (menu, niveau 2…) à la fin de l'image. |
| `playScene(name [, "funcion"])` | Lit une scène 3D comme cinématique, avec le jeu en pause ; à la fin, appelle cette fonction. |
| `stopScene3D()`, `scenePlaying()` | Interrompt la cinématique / indique celle qui est en cours. |
| `draw3D([on])` | Désactive ou active toute la 3D (par exemple, derrière un menu opaque). |
