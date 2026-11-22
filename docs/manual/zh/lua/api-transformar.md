# 3D 中的移动、旋转和缩放

| 函数 | 作用 |
|---|---|
| `getPosition(obj) → x, y, z` | 它所在的位置。 |
| `setPosition(obj, x, y, z)` | 把它放到一个精确的位置（忽略物理）。 |
| `move(obj, dx, dy, dz)` | 从当前位置移动它（配合 `dt` 使用）。 |
| `getRotation(obj) → x, y, z` | 它的旋转，以度为单位。 |
| `setRotation(obj, x, y, z [, order])` | 精确的旋转，与 Transform 卡片中的相同。用 `order` 选择各轴的应用顺序，例如 `"ZXY"`（先绕竖直轴确定朝向，就像车辆一样）。 |
| `getRotationX(obj)`, `getRotationY(obj)`, `getRotationZ(obj) → 度` | 单个轴的旋转。`getRotationZ` 是绕竖直轴的旋转。 |
| `setRotationX(obj, g)`, `setRotationY(obj, g)`, `setRotationZ(obj, g)` | 只改变一个轴，另外两个保持不变。例如 `setRotationZ(obj, getRotationZ(obj) + 90 * dt)` 让它绕竖直轴旋转。 |
| `rotate(obj, dx, dy, dz)` | 让它绕自身的轴旋转。 |
| `rotateTowards(obj, dx, dy [, factor])` | 让它平滑地转向地面上（X 和 Y）的某个方向。 |
| `lookAt(obj, x, y, z)` | 让它朝向某个点（相机的正面）。 |
| `getScale(obj) → sx, sy, sz` | 它的缩放。 |
| `setScale(obj, s [, sy, sz])` | 精确的缩放；只传一个数字时等比缩放。 |
| `scale(obj, f)` | 把当前缩放乘以一个系数。 |
| `worldPoint(obj, x, y, z) → x, y, z` | 把对象上的一个点转换为世界坐标。 |
| `isVisible(obj) → bool`, `setVisible(obj, bool)` | 显示或隐藏它。隐藏的对象不再收到 `update`。 |
| `setTint(obj, r, g, b [, a])` | 改变它的颜色（取值 0 到 1）。 |
