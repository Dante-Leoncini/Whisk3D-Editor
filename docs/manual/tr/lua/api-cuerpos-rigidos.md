# Katı cisimler, çarpışmalar ve hitbox'lar

| Fonksiyon | Ne yapar |
|---|---|
| `physicsVel`, `physicsAngVel`, `physicsImpulse`, `physicsForce` | Bir katı cismin hızı, dönüşü, itmeleri ve kuvvetleri. |
| `physicsGravity([g])`, `physicsActive(obj, bool)`, `physicsAwake(obj)` | Yerçekimi (varsayılan 9.81), bir nesnenin fiziğini açma/kapama. |
| `physicsRay(ox, oy, oz, dx, dy, dz, max) → dist, nx, ny, nz, obj` | Bir ışın gönderir ve neye çarptığını söyler. |
| `contacts() → list` | Bu karedeki tüm çarpışmalar. |
| `colGround(mesh, x, y, z)`, `colWall(...)`, `colRay(...)` | Bir örgünün üçgenlerine karşı çarpışma (zemine basma, duvarlardan geçmeme). |
| `hitboxActive`, `setHitboxActive`, `setHitboxSize`, `hitboxInside`, `hitboxTouching` | Hitbox'ları yönetme ve içlerinde ne olduğunu sorma. |
| `showHitboxes(bool)` | Hata ayıklamak için hitbox'ları çizer. |
