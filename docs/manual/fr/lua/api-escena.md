# La scène : chercher, créer et détruire

| Fonction | Ce qu'elle fait |
|---|---|
| `buscar(nombre [, raiz]) → obj \| nil` | Cherche un objet par son nom dans l'Outliner (ou seulement à l'intérieur de `raiz`). |
| `nombre(obj) → texto` | Le nom de l'objet. |
| `tipo(obj) → texto` | Ce que c'est : `"mesh"`, `"luz"`, `"camara"`, `"vacio"`, `"esqueleto"`, `"texto"`, `"imagen"`, `"boton"`, `"hitbox"`, `"prefab"`, `"rutina"`… (`""` s'il n'existe pas). |
| `instanciar(prefab [, x, y, z [, rotY [, padre]]]) → obj \| nil` | Crée une copie d'un prefab à cette position (rotY en degrés). `"Libreria/Escena"` crée celle d'une bibliothèque liée. Avec `padre`, elle est rattachée à cet objet (position et rotation relatives à celui-ci). |
| `destruir(obj) → bool` | Supprime l'objet à la fin de l'image. |
| `importarW3D(ruta) → obj \| nil` | Ajoute à la scène un autre fichier .w3d pendant que le jeu tourne. |
| `cargar(obj)` / `descargar(obj)` / `cargaAuto(obj)` | Charge ou décharge manuellement une partie du niveau (streaming), ou la remet en mode automatique. |
| `cargado(obj) → bool` | Si cette partie est déjà chargée. |
