# Physics

All homemade, in C++03, with no external engine and no memory allocation every frame. If nobody uses it, it costs nothing: each step returns on its first line.

- **W3dFisica:** velocity and bouncing of axis-aligned boxes. Nothing more.
- **W3dRigido:** oriented boxes with mass, gravity, impulses with friction and bounce, raycasts and contact events.
- **W3dColMalla:** walking on a level's triangles (ramps, steps) with walls that stop you, using a grid so it doesn't have to check thousands of triangles.
- **W3dHitbox:** boxes that only report (`alEntrar`, `alQuedarse`, `alSalir`, `alTocar`).
- **W3dRagdoll:** ragdolls, plus two-bone inverse kinematics and FABRIK.
- **W3dNav:** the path from one point to another over a grid on the floor (A* and then string pulling).

Why it matters: the editor's Play and the built game call the same step with the same `dt`, so the game behaves the same in both, on PC and on the N95. From Lua: [Simple physics](../lua/api-fisica-simple.md) and [Rigid bodies, collisions and hitboxes](../lua/api-cuerpos-rigidos.md).
