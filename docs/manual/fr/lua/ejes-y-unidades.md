# Axes, unités et objets

- **3D :** dans les scripts, l'axe **Y pointe vers le haut**. Le panneau Propriétés affiche Z vers le haut, donc le champ « Z » du panneau correspond au `y` du script. Les positions, rotations et échelles sont relatives au parent de l'objet. Les rotations sont en **degrés**.
- **2D (écran et UI) :** pixels du canevas, avec le (0, 0) au **centre** ; +x vers la droite et +y vers le **bas**.
- **Objets :** les fonctions qui cherchent ou renvoient des objets (`yo`, `buscar`, `objeto`…) donnent une valeur qui n'a ni champs ni méthodes : vous la passez à d'autres fonctions. Deux objets peuvent se comparer avec `==`. Si l'objet a été détruit, il se comporte comme `nil`.
- **Tolérance :** presque toutes les fonctions acceptent `nil` ou un objet du mauvais type sans rien casser : celles qui lisent renvoient 0, `""` ou `false`, et celles qui écrivent ne font rien.
- **Indices :** comme en Lua, presque tout commence à 1 (sommets, doigts, couches, étapes de routine). L'exception, ce sont les mémoires des routines (`memoria`), qui vont de 0 à 255.
