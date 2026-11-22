# Rigid bodies, collisions and hitboxes

| Function | What it does |
|---|---|
| `physicsVel`, `physicsAngVel`, `physicsImpulse`, `physicsForce` | Velocity, spin, pushes and forces of a rigid body. |
| `physicsGravity([g])`, `physicsActive(obj, bool)`, `physicsAwake(obj)` | Gravity (9.81 by default), turning an object's physics on/off. |
| `physicsRay(ox, oy, oz, dx, dy, dz, max) → dist, nx, ny, nz, obj` | Casts a ray and tells you what it hits. |
| `contacts() → list` | All the collisions in this frame. |
| `colGround(mesh, x, y, z)`, `colWall(...)`, `colRay(...)` | Collision against the triangles of a mesh (standing on the floor, not going through walls). |
| `hitboxActive`, `setHitboxActive`, `setHitboxSize`, `hitboxInside`, `hitboxTouching` | Manage hitboxes and ask what's inside them. |
| `showHitboxes(bool)` | Draws the hitboxes for debugging. |
