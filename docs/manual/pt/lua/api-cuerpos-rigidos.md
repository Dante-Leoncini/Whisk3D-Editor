# Corpos rígidos, colisões e hitboxes

| Função | O que faz |
|---|---|
| `physicsVel`, `physicsAngVel`, `physicsImpulse`, `physicsForce` | Velocidade, giro, empurrões e forças de um corpo rígido. |
| `physicsGravity([g])`, `physicsActive(obj, bool)`, `physicsAwake(obj)` | Gravidade (9.81 por padrão), ligar/desligar a física de um objeto. |
| `physicsRay(ox, oy, oz, dx, dy, dz, max) → dist, nx, ny, nz, obj` | Lança um raio e diz com o que ele colide. |
| `contacts() → list` | Todas as colisões deste quadro. |
| `colGround(mesh, x, y, z)`, `colWall(...)`, `colRay(...)` | Colisão contra os triângulos de uma malha (pisar no chão, não atravessar paredes). |
| `hitboxActive`, `setHitboxActive`, `setHitboxSize`, `hitboxInside`, `hitboxTouching` | Gerenciar hitboxes e perguntar o que há dentro. |
| `showHitboxes(bool)` | Desenha os hitboxes para depurar. |
