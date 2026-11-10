# Un script est un composant d'un objet

Dans Whisk3D, un script appartient toujours à un **objet** de la scène (un maillage, un empty, un bouton 2D…). Un objet peut avoir plusieurs scripts ; chacun s'exécute séparément, avec ses propres variables, dans l'ordre où ils apparaissent.

Depuis le script, `yo()` renvoie l'objet propriétaire. Pour utiliser *d'autres* objets, le script déclare une table globale nommée `propiedades`. L'éditeur la lit et affiche un champ pour chaque entrée dans la carte du script, dans le panneau Propriétés :

```lua
propiedades = {
  pelota     = "objeto",                          -- un champ pour choisir un objet de la scène
  dificultad = { "facil", "normal", "dificil" },  -- une liste déroulante (la première est celle par défaut)
  velocidad  = 200,                               -- un nombre propre à chaque objet qui utilise le script
  activo     = true,                              -- un oui/non
  etiqueta   = "caja",                            -- un texte
}

function inicio()
  local p   = objeto("pelota")         -- l'objet choisi dans le champ "pelota"
  local dif = opcion("dificultad")     -- "facil", "normal" ou "dificil"
  local v   = propiedad("velocidad")   -- 200, ou la valeur saisie sur cet objet
end
```

Ainsi, le même script sert pour de nombreux objets avec des valeurs différentes : le champ `velocidad` d'un ennemi peut valoir 200 et celui d'un autre 350.

> [!NOTE]
> Pour lire `propiedades`, l'éditeur **exécute le fichier** même si le jeu ne tourne pas. C'est pourquoi, en dehors des fonctions, ne mettez que des déclarations (tables, constantes, `local`), jamais du code qui agit.

Les scripts ne partagent pas de variables entre eux. Pour se transmettre des données (les points, si le joueur a gagné…), ils utilisent un espace partagé : `setCompartido("puntos", 10)` écrit et `compartido("puntos")` lit. Il stocke des nombres, des oui/non et des textes, et il est vidé à l'arrêt du jeu.
