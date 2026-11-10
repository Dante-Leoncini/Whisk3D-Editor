# Le projet (.w3d) et les fichiers

| Fonction | Description | Où |
|---|---|---|
| Ouvrir projet | Ouvre un .w3d (aussi par double clic depuis le système). | Propriétés → Fichier/Rendu → Fichier |
| Enregistrer / Enregistrer sous | Enregistre dans le dossier et sous le nom indiqués dans la carte ; Enregistrer sous en demande un autre. Si le nom manque, le champ devient rouge. | Carte Fichier, <kbd>Ctrl+S</kbd> |
| Enregistrer version | Enregistre et laisse en plus une copie numérotée (`proyecto_v03.w3d`). | Carte Fichier |
| Extraire assets | Copie tout le contenu interne dans un dossier (les copies ne modifient pas le projet). | Carte Fichier |
| Maillages en texte | Enregistre les maillages sous forme de texte lisible au lieu de binaire. | Carte Fichier |
| Imports | OBJ, FBX, glTF, GLB et interfaces .w3dui. Ce qui est importé est copié dans le projet (sans doublons). | Vue 3D → Ajouter → Imports |
| Exporter | OBJ, glTF ou GLB de l'objet sélectionné (avec ou sans modificateurs et transformations). | Propriétés → Exporter |
| Modifications non enregistrées | Ce qui a été modifié et pas encore enregistré (scripts, textures peintes) est marqué d'un `*`. Lorsque vous fermez ou ouvrez un autre projet, un avertissement liste ce qui serait perdu. | Automatique |
| Ressource interne ou externe | Une ressource externe n'est pas copiée : son chemin est enregistré et un avertissement s'affiche si elle manque. | Carte Ressource ; « Enregistrer dans le .w3d / hors du .w3d » dans l'IDE et l'Éditeur UV |

L'enregistrement ne laisse jamais le fichier à moitié écrit : il écrit d'abord une copie temporaire, puis la renomme.

## Dans le navigateur (version web)

Le navigateur ne permet pas à une page de lire ni d'écrire de lui-même les fichiers de l'ordinateur. C'est pourquoi, dans la version web :

- **Ouvrir projet**, **Imports** et le choix d'une texture ouvrent l'explorateur de fichiers du navigateur : un panneau apparaît avec le bouton **Choisir fichier**. Pour un OBJ, on choisit ensemble le .obj, son .mtl et ses textures ; pour un glTF, le .gltf et son .bin.
- **Enregistrer** (<kbd>Ctrl+S</kbd>), **Enregistrer sous**, **Enregistrer version** et **Extraire assets** téléchargent le fichier dans le dossier de téléchargements du navigateur. Il en va de même pour **Exporter** (l'OBJ se télécharge avec son .mtl et ses textures à côté), le rendu, la carte UV, les textures peintes enregistrées à l'extérieur et les scripts externes.
- Quand l'éditeur demande un dossier pour enregistrer, aucun explorateur ne s'ouvre : le fichier est téléchargé directement.
- Si le navigateur demande si la page peut télécharger plusieurs fichiers, il faut accepter. Sinon, seul le premier est téléchargé.
- Ce qui est ouvert vit dans la mémoire de la page : en la rechargeant, on perd tout ce qui n'a pas été téléchargé.
