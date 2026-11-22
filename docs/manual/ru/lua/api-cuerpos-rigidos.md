# Твёрдые тела, столкновения и хитбоксы

| Функция | Что делает |
|---|---|
| `physicsVel`, `physicsAngVel`, `physicsImpulse`, `physicsForce` | Скорость, вращение, толчки и силы твёрдого тела. |
| `physicsGravity([g])`, `physicsActive(obj, bool)`, `physicsAwake(obj)` | Гравитация (по умолчанию 9.81), включение/выключение физики объекта. |
| `physicsRay(ox, oy, oz, dx, dy, dz, max) → dist, nx, ny, nz, obj` | Пускает луч и сообщает, во что он попал. |
| `contacts() → list` | Все столкновения этого кадра. |
| `colGround(mesh, x, y, z)`, `colWall(...)`, `colRay(...)` | Столкновение с треугольниками меша (стоять на полу, не проходить сквозь стены). |
| `hitboxActive`, `setHitboxActive`, `setHitboxSize`, `hitboxInside`, `hitboxTouching` | Управлять хитбоксами и узнавать, что внутри. |
| `showHitboxes(bool)` | Рисует хитбоксы для отладки. |
