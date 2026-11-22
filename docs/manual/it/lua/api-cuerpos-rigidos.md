# Corpi rigidi, collisioni e hitbox

| Funzione | Cosa fa |
|---|---|
| `physicsVel`, `physicsAngVel`, `physicsImpulse`, `physicsForce` | Velocità, rotazione, spinte e forze di un corpo rigido. |
| `physicsGravity([g])`, `physicsActive(obj, bool)`, `physicsAwake(obj)` | Gravità (9.81 predefinita), accendere/spegnere la fisica di un oggetto. |
| `physicsRay(ox, oy, oz, dx, dy, dz, max) → dist, nx, ny, nz, obj` | Lancia un raggio e dice contro cosa colpisce. |
| `contacts() → list` | Tutte le collisioni di questo fotogramma. |
| `colGround(mesh, x, y, z)`, `colWall(...)`, `colRay(...)` | Collisione contro i triangoli di una mesh (camminare sul pavimento, non attraversare i muri). |
| `hitboxActive`, `setHitboxActive`, `setHitboxSize`, `hitboxInside`, `hitboxTouching` | Gestire gli hitbox e chiedere cosa c'è dentro. |
| `showHitboxes(bool)` | Disegna gli hitbox per il debug. |
