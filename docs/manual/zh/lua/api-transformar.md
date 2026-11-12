# 3D 中的移动、旋转和缩放

| 函数 | 作用 |
|---|---|
| `posicion(obj) → x, y, z` | 它所在的位置。 |
| `setPosicion(obj, x, y, z)` | 把它放到一个精确的位置（忽略物理）。 |
| `mover(obj, dx, dy, dz)` | 从当前位置移动它（配合 `dt` 使用）。 |
| `rotacion(obj) → x, y, z` | 它的旋转，以度为单位。 |
| `setRotacion(obj, x, y, z [, orden])` | 精确的旋转，与 Transform 卡片中的相同。用 `orden` 选择各轴的应用顺序，例如 `"ZXY"`（先绕竖直轴确定朝向，就像车辆一样）。 |
| `rotacionX(obj)`, `rotacionY(obj)`, `rotacionZ(obj) → 度` | 单个轴的旋转。`rotacionZ` 是绕竖直轴的旋转。 |
| `setRotacionX(obj, g)`, `setRotacionY(obj, g)`, `setRotacionZ(obj, g)` | 只改变一个轴，另外两个保持不变。例如 `setRotacionZ(obj, rotacionZ(obj) + 90 * dt)` 让它绕竖直轴旋转。 |
| `girar(obj, dx, dy, dz)` | 让它绕自身的轴旋转。 |
| `girarHacia(obj, dx, dy [, factor])` | 让它平滑地转向地面上（X 和 Y）的某个方向。 |
| `mirarA(obj, x, y, z)` | 让它朝向某个点（相机的正面）。 |
| `escala(obj) → sx, sy, sz` | 它的缩放。 |
| `setEscala(obj, s [, sy, sz])` | 精确的缩放；只传一个数字时等比缩放。 |
| `escalar(obj, f)` | 把当前缩放乘以一个系数。 |
| `puntoMundo(obj, x, y, z) → x, y, z` | 把对象上的一个点转换为世界坐标。 |
| `visible(obj) → bool`, `setVisible(obj, bool)` | 显示或隐藏它。隐藏的对象不再收到 `actualizar`。 |
| `setTinte(obj, r, g, b [, a])` | 改变它的颜色（取值 0 到 1）。 |
