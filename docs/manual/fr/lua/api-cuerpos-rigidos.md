# Corps rigides, collisions et hitbox

| Fonction | Ce qu'elle fait |
|---|---|
| `physicsVel`, `physicsAngVel`, `physicsImpulse`, `physicsForce` | Vitesse, rotation, poussées et forces d'un corps rigide. |
| `physicsGravity([g])`, `physicsActive(obj, bool)`, `physicsAwake(obj)` | Gravité (9.81 par défaut), activer/désactiver la physique d'un objet. |
| `physicsRay(ox, oy, oz, dx, dy, dz, max) → dist, nx, ny, nz, obj` | Lance un rayon et indique ce qu'il touche. |
| `contacts() → list` | Toutes les collisions de cette image. |
| `colGround(mesh, x, y, z)`, `colWall(...)`, `colRay(...)` | Collision avec les triangles d'un maillage (marcher sur le sol, ne pas traverser les murs). |
| `hitboxActive`, `setHitboxActive`, `setHitboxSize`, `hitboxInside`, `hitboxTouching` | Gérer les hitbox et demander ce qu'elles contiennent. |
| `showHitboxes(bool)` | Dessine les hitbox pour le débogage. |
