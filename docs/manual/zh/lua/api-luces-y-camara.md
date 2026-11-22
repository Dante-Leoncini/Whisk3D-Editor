# 灯光和相机

| 函数 | 作用 |
|---|---|
| `getColor(light) → r, g, b`, `setColor(light, r, g, b)` | 灯光的颜色。 |
| `getEnergy(light) → e`, `setEnergy(light, e)` | 它的亮度，从 0 到 1（用来让灯光闪烁或逐渐熄灭）。 |
| `setActiveCamera(cam)` | 切换观看游戏所用的相机。 |
| `getLens(cam) → fov, near, far`, `setLens(cam [, fov, near, far])` | 视角和裁剪距离。 |
| `cameraXZ() → fx, fy, rx, ry` | 相机在地面上朝向哪里，以 X 和 Y 表示：前方（`fx, fy`）和右方（`rx, ry`），用来根据相机方向移动角色。（这个名字来自地面还是 XZ 的时候。） |
| `screenOf(obj) → x, y, front` | 3D 对象出现在屏幕上的位置（用来在它上方放一个 2D 标签）。 |
| `setRail(cam, curve [, offset])`, `setRailNode`, `setRailLook`, `getRail` | 让相机沿一条曲线（轨道）移动。 |
| `resolution() → width, height` | 3D 图像的实际尺寸，单位为像素。 |
