# Katı cisimler, çarpışmalar ve hitbox'lar

| Fonksiyon | Ne yapar |
|---|---|
| `fisicaVel`, `fisicaVelAng`, `fisicaImpulso`, `fisicaFuerza` | Bir katı cismin hızı, dönüşü, itmeleri ve kuvvetleri. |
| `fisicaGravedad([g])`, `fisicaActiva(obj, bool)`, `fisicaDespierto(obj)` | Yerçekimi (varsayılan 9.81), bir nesnenin fiziğini açma/kapama. |
| `fisicaRayo(ox, oy, oz, dx, dy, dz, max) → dist, nx, ny, nz, obj` | Bir ışın gönderir ve neye çarptığını söyler. |
| `contactos() → lista` | Bu karedeki tüm çarpışmalar. |
| `colSuelo(malla, x, y, z)`, `colPared(...)`, `colRayo(...)` | Bir örgünün üçgenlerine karşı çarpışma (zemine basma, duvarlardan geçmeme). |
| `hitboxActivo`, `setHitboxActivo`, `setHitboxTam`, `hitboxDentro`, `hitboxTocando` | Hitbox'ları yönetme ve içlerinde ne olduğunu sorma. |
| `verHitboxes(bool)` | Hata ayıklamak için hitbox'ları çizer. |
