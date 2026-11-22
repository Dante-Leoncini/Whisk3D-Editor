# 粒子、雾和可见性

| 函数 | 作用 |
|---|---|
| `emit(obj [, n])`, `setEmitting(obj, bool)` | 发射一波粒子，或开启/关闭一个发射器。 |
| `getFog(obj)`, `setFog(obj, density [, r, g, b, inicio, fin])` | 读取和修改雾。 |
| `setSector`, `visCell`, `setVisAnchor`, `setVisCurveT`, `setBatch` | 高级控制：关卡的哪些部分会被绘制。 |
