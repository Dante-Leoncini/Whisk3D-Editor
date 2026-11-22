# La scène : chercher, créer et détruire

| Fonction | Ce qu'elle fait |
|---|---|
| `find(name [, root]) → obj \| nil` | Cherche un objet par son nom dans l'Outliner (ou seulement à l'intérieur de `root`). |
| `selected() → { obj, … }` | Les objets sélectionnés dans l'éditeur (pour les [boutons](scripts-y-objetos.md#boutons) qui servent de modules complémentaires de l'éditeur). |
| `getName(obj) → text` | Le nom de l'objet. |
| `getType(obj) → text` | Ce que c'est : `"mesh"`, `"light"`, `"camera"`, `"empty"`, `"armature"`, `"text"`, `"image"`, `"button"`, `"hitbox"`, `"prefab"`, `"rutina"`… (`""` s'il n'existe pas). |
| `instantiate(prefab [, x, y, z [, rotY [, parent]]]) → obj \| nil` | Crée une copie d'un prefab à cette position (rotY en degrés). `"Libreria/Escena"` crée celle d'une bibliothèque liée. Avec `parent`, elle est rattachée à cet objet (position et rotation relatives à celui-ci). |
| `destroy(obj) → bool` | Supprime l'objet à la fin de l'image. |
| `importW3D(path) → obj \| nil` | Ajoute à la scène un autre fichier .w3d pendant que le jeu tourne. |
| `streamLoad(obj)` / `streamUnload(obj)` / `streamAuto(obj)` | Charge ou décharge manuellement une partie du niveau (streaming), ou la remet en mode automatique. |
| `isLoaded(obj) → bool` | Si cette partie est déjà chargée. |
