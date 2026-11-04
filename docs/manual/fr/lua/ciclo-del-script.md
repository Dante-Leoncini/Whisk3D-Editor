# Quand le moteur appelle votre script

Votre script définit des fonctions aux noms spéciaux, et le moteur les appelle tout seul, au bon moment. Elles sont toutes facultatives : n'écrivez que celles dont vous avez besoin.

| Fonction | Quand elle est appelée |
|---|---|
| `inicio()` | Une fois, au lancement de Play (ou dès que l'objet est créé avec `instanciar`). Sert à chercher des objets et à préparer des variables. |
| `actualizar(dt)` | À chaque image, tant que l'objet est **visible** et dans la scène active. `dt` correspond aux secondes écoulées depuis l'image précédente (entre 0.001 et 0.1). |
| `alEntrar(otro, miHitbox, suHitbox)` | Quand une hitbox commence à en toucher une autre. |
| `alQuedarse(otro, miHitbox, suHitbox)` | À chaque image, tant qu'elles continuent de se toucher. |
| `alSalir(otro, miHitbox, suHitbox)` | Quand elles cessent de se toucher (aussi si l'une est masquée ou désactivée). |
| `alTocar(otro, impulso, x, y, z, nx, ny, nz)` | Quand deux **corps rigides** entrent en collision. `x, y, z` est le point de contact et `nx, ny, nz` la direction vers l'autre. |

Les événements de hitbox sont reçus par les scripts de la hitbox et par ceux de son objet parent. Si l'autre est un corps rigide, `suHitbox` arrive comme `nil`.

## Déplacer des choses avec dt

Multipliez toujours les vitesses par `dt`. Ainsi, le jeu va aussi vite sur un PC puissant que sur un téléphone lent :

```lua
function actualizar(dt)
  mover(yo(), 3 * dt, 0, 0)   -- 3 unités par seconde vers +X
end
```

## Ordre au sein d'une image

1. Physique (vitesses, corps rigides) et les événements de collision : `alTocar`, `alSalir`, `alEntrar`, `alQuedarse`.
2. `actualizar(dt)` de tous les scripts, dans l'ordre de l'Outliner.
3. Animations (scène, clips, armatures).
4. Fin de l'image : les `destruir()` en attente sont appliqués, les objets instanciés entrent dans la scène et le `cambiarEscena()` demandé est effectué.

## À l'arrêt du jeu

Quand vous appuyez sur Stop, tous les scripts sont fermés, l'espace partagé est effacé et la scène revient à son état d'avant le Play. Si vous modifiez un script pendant que le jeu tourne, l'objet concerné le recharge : ses variables reviennent à zéro et `inicio()` est appelée à nouveau.
