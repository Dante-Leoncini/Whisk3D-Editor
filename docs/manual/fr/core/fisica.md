# Physique

Tout est fait maison, en C++03, sans moteur externe et sans demander de mémoire à chaque image. Si personne ne l'utilise, elle ne coûte rien : chaque pas s'arrête dès la première ligne.

- **W3dFisica :** vitesse et rebond de boîtes alignées sur les axes. Rien de plus.
- **W3dRigido :** boîtes orientées avec masse, gravité, impulsions avec frottement et rebond, raycast et événements de contact.
- **W3dColMalla :** marcher sur les triangles d'un décor (rampes, marches) et être arrêté par les murs, avec une grille pour ne pas parcourir des milliers de triangles.
- **W3dHitbox :** des boîtes qui ne font que signaler (`alEntrar`, `alQuedarse`, `alSalir`, `alTocar`).
- **W3dRagdoll :** des poupées de chiffon, plus la cinématique inverse à deux os et FABRIK.
- **W3dNav :** le chemin d'un point à un autre sur une grille du sol (A*, puis lissage).

Pourquoi c'est important : le Play de l'éditeur et le jeu compilé appellent le même pas avec le même `dt`, donc le jeu se comporte de la même façon dans les deux, sur PC comme sur le N95. Depuis Lua : [Physique simple](../lua/api-fisica-simple.md) et [Corps rigides, collisions et hitbox](../lua/api-cuerpos-rigidos.md).
