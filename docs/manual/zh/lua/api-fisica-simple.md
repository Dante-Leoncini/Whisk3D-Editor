# 简单物理（速度和反弹）

| 函数 | 作用 |
|---|---|
| `velocidad(obj, vx [, vy, vz])` / `velocidad(obj) → vx, vy, vz` | 给它一个速度，引擎会自动移动它。 |
| `acelerar(obj, factor [, tope])` | 通过乘以它的速度来加速。 |
| `caja(obj [, x, y, z])` | 它的碰撞盒的大小：在 3D 中是宽度（X）、深度（Y）和高度（Z）；在 2D 中是以像素为单位的宽度和高度。 |
| `rebotar(obj, otro)`, `rebotarEn(obj, area [, lados])`, `rebotarDentro(obj, area)` | 碰到另一个对象或某个区域的边缘时反弹（发生反弹时返回 `true`）。 |
