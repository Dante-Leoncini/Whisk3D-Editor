# Simple physics (velocity and bounces)

| Function | What it does |
|---|---|
| `getVelocity(obj, vx [, vy, vz])` / `getVelocity(obj) → vx, vy, vz` | You give it a velocity and the engine moves it by itself. |
| `accelerate(obj, factor [, tope])` | Speeds it up by multiplying its velocity. |
| `box(obj [, x, y, z])` | The size of its collision box: in 3D, the width (X), the depth (Y) and the height (Z); in 2D, the width and the height in pixels. |
| `bounce(obj, other)`, `bounceOn(obj, area [, sides])`, `bounceInside(obj, area)` | Bounces off another object or off the edges of an area (they return `true` if it bounced). |
