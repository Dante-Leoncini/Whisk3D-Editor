# 场景和过场动画

| 函数 | 作用 |
|---|---|
| `changeScene(name [, restart])` | 在帧结束时切换到另一个场景（菜单、第 2 关……）。 |
| `playScene(name [, "funcion"])` | 把一个 3D 场景当作过场动画播放，同时暂停游戏；播放结束时调用该函数。 |
| `stopScene3D()`, `scenePlaying()` | 停止过场动画 / 返回正在播放的是哪一个。 |
| `draw3D([on])` | 关闭或开启全部 3D 绘制（例如在不透明的菜单后面）。 |
