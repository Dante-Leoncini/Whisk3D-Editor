# Limites et conseils

## Ce qui manque de Lua

- `string`, `table` et `math` sont disponibles, ainsi que les fonctions de base (`print`, `pairs`, `ipairs`, `type`, `tostring`, `tonumber`, `pcall`, `assert`…).
- `io` et `os` ne sont **pas** disponibles (un jeu ne touche pas aux fichiers du disque ; c'est pourquoi il n'y a pas de `os.time`), ni `require`, `coroutine`, `utf8` ou `debug`. Pour enregistrer des données, utilisez `getConfig` / `saveConfig`.
- Chaque script est indépendant : il ne peut pas voir les variables d'un autre. Pour communiquer, utilisez `setShared` / `getShared`.

## Erreurs

- Si une fonction contient une erreur, celle-ci est notée dans le journal (`archivo.lua:LÍNEA: mensaje`) et l'appel est interrompu, mais la fonction est de nouveau appelée à l'image suivante : la même erreur se répète à chaque image jusqu'à ce que vous la corrigiez.
- Si le fichier ne peut pas être lu ou contient une erreur de syntaxe, ce script reste désactivé jusqu'au prochain Play.
- Dans Whisk3D, `error("...")` ne fait que noter le message, et le script continue. Si vous voulez l'interrompre, utilisez `assert(condicion, "mensaje")`.

## Points à garder en tête

- `update` ne s'exécute que sur les objets **visibles** de la scène active : masquer un objet désactive sa logique.
- `destroy`, `changeScene` et `playScene` s'appliquent à la fin de l'image, pas immédiatement.
- La police des textes du jeu n'affiche ni les accents ni la ñ : écrivez `"MENU"` au lieu de `"MENÚ"`.
- `safeArea`, `uiBox` et `resolution` n'ont de valeurs réelles qu'après la première image dessinée : ne les utilisez pas dans `start`.
- Sur le Web et Android, seule la **première** lumière éclaire.
- Sur le Nokia N95, il n'y a ni manette ni souris : utilisez `key` avec les noms du clavier du téléphone. Les nombres entiers sont sur 32 bits.

## Pour que ça tourne vite (surtout sur le téléphone)

- Cherchez les objets une seule fois, dans `start`, et gardez-les dans des variables `local`.
- Utilisez les versions « en lot » (`setVertices`, `setVerticesColor`) au lieu d'appeler mille fois la version unitaire.
- Préférez les fonctions du moteur (`scrollUV`, `linePoint`, `setRotation` avec ordre) plutôt que de faire les calculs en Lua.
- Mesurez avec `luaProfile` et `clockMs` avant d'optimiser à l'aveugle.
