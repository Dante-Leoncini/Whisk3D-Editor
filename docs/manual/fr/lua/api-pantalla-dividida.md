# Écran partagé (deux joueurs sur la même machine)

| Fonction | Ce qu'elle fait |
|---|---|
| `setVistas(n [, "lado" \| "arriba"])` | `2` divise l'écran en deux (côte à côte ou l'une au-dessus de l'autre) ; `1` revient à l'écran entier. L'interface 2D (le HUD) est dessinée une seule fois, par-dessus les deux. |
| `setVistaCamara(i, camara)` | La caméra de la vue `i` (1 ou 2). Si elle n'est pas définie, c'est la caméra active. |
| `vista() → i, n` | La vue en cours de dessin (1 ou 2 ; 0 en dehors du dessin) et leur nombre. |
| `antesDeVista(i)` | (C'est votre script qui la définit.) Le moteur l'appelle avant de dessiner chaque vue, dans le script qui a appelé `setVistas`. C'est là que vous tournez vers CETTE caméra ce qui regarde la caméra (panneaux, particules) ou que vous cachez ce que cette vue ne doit pas voir. Ne déplacez rien qui avance avec le temps : cela va dans `actualizar`. |

Chaque moitié est une image complète avec sa caméra : le cadre du jeu (le rapport d'aspect de la caméra) est coupé en deux.

## Une manette pour chacun

`boton`, `botonApretado`, `stick` et `gatillo` acceptent le **numéro de manette** (1 à 4, dans l'ordre où elles ont
bougé pour la première fois) : `boton("a", 2)`, `stick("izq", 1)`. Sans numéro, n'importe laquelle (toutes ensemble),
comme d'habitude. Le clavier n'a pas de numéro : c'est à vous de le répartir (par exemple, le joueur 1 avec les flèches
et les chiffres, le 2 avec WASD et les lettres ; sur un Nokia avec clavier QWERTY, les lettres arrivent aussi).

```lua
function actualizar(dt)
  local x1, y1 = stick("izq", 1)
  local x2, y2 = stick("izq", 2)
  if tecla("w") then y2 = -1 end
  mover(jugador1, x1 * dt * 5, 0, y1 * dt * 5)
  mover(jugador2, x2 * dt * 5, 0, y2 * dt * 5)
end
```
