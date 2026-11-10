# 2D 界面和屏幕

| 函数 | 作用 |
|---|---|
| `pantalla() → ancho, alto` | 画布的尺寸。 |
| `margenPantalla() → mx, my` | 在固定尺寸的画布周围，每一侧到真正的屏幕边缘还剩多少屏幕，以画布单位表示（取景留下的边带也算屏幕）。竖屏且画布为 4:3 时：把一个计量条向上移动 `my`，它就会贴住上边缘。 |
| `marcoPantalla() → x, y, ancho, alto` | 3D 图像的框（相机的取景范围，不含边带），以画布单位表示，从其中心算起。 |
| `capturarPantalla(nombre [, conHud]) → ruta` | 把这一帧的 3D 图像复制到一张纹理中（在场景绘制完成时；传入 `conHud` 时，在 HUD 之后）。该路径（`"pantalla:<nombre>"`）可用于 `setTextura`、材质，或例程的 "Bind texture" 步骤。 |
| `capturaUV(nombre) → u0, v0, u1, v1` | 所捕获纹理中图像为正向的那一部分，用于 `setUVRect`（如果还没有复制则返回 `nil`：复制在绘制这一帧时进行）。 |
| `areaSegura() → x, y, w, h` | 屏幕上没有刘海的区域（不要在 `inicio` 中使用）。 |
| `escala() → f` | 不带参数时：用于让尺寸适配屏幕的系数（较短边 / 480）。 |
| `posPx(obj) → x, y`, `setPosPx(obj, x, y)` | 2D 元素的位置（传 `nil` 会让该轴保持不变）。 |
| `tamPx(obj) → w, h`, `setTamPx(obj, w, h)` | 2D 元素的尺寸。 |
| `cajaUI(obj) → x, y, w, h` | 它实际占据的矩形（x, y = 中心）。 |
| `setTexto(obj, valor)`, `setTam(texto, px)` | 修改文本或按钮的文字，以及字号。 |
| `setTextura(img, "ruta.png")`, `setUVRect`, `setFlipbook` | 更换图像、裁剪其中一部分（u1 < u0 或 v1 < v0 时为镜像），或播放一张帧序列图。 |
| `setOpacidad(obj, 0..1)`, `fundir(obj, objetivo, paso)` | 透明度和淡入淡出。 |
| `setRot2D(obj, grados)` | 旋转一个 2D 元素。 |
| `chocan(a, b) → bool` | 两个 2D 元素在屏幕上是否重叠。 |
| `limitar(obj, area [, eje])`, `dentro(area, x [, y])` | 让一个元素保持在另一个元素内部，或询问某个点是否在内部。 |
| `salir()` | 关闭游戏（在编辑器中不起作用）。 |
