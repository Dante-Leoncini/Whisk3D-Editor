# 灯光和相机

| 函数 | 作用 |
|---|---|
| `color(luz) → r, g, b`, `setColor(luz, r, g, b)` | 灯光的颜色。 |
| `energia(luz) → e`, `setEnergia(luz, e)` | 它的亮度，从 0 到 1（用来让灯光闪烁或逐渐熄灭）。 |
| `setCamaraActiva(cam)` | 切换观看游戏所用的相机。 |
| `lenteDe(cam) → fov, cerca, lejos`, `setLente(cam [, fov, cerca, lejos])` | 视角和裁剪距离。 |
| `camaraXZ() → fx, fy, rx, ry` | 相机在地面上朝向哪里，以 X 和 Y 表示：前方（`fx, fy`）和右方（`rx, ry`），用来根据相机方向移动角色。（这个名字来自地面还是 XZ 的时候。） |
| `pantallaDe(obj) → x, y, delante` | 3D 对象出现在屏幕上的位置（用来在它上方放一个 2D 标签）。 |
| `setRiel(cam, curva [, offset])`, `setRielNodo`, `setMiradaRiel`, `rielDe` | 让相机沿一条曲线（轨道）移动。 |
| `resolucion() → ancho, alto` | 3D 图像的实际尺寸，单位为像素。 |
