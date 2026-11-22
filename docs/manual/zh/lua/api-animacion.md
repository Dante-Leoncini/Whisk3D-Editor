# 动画

| 函数 | 作用 |
|---|---|
| `animScene(name [, loop])`, `stopScene()`, `animSceneFrame(f)` | 整个场景的动画。 |
| `animObject(root, "clip" [, loop])`, `animObjectStop`, `animObjectFinished`, `animObjectSpeed` | 对象及其子级的动画片段。 |
| `animClip(armature, clip [, loop])`, `animFinished`, `animSpeed`, `animTransition` | 骨架的动画（走、跳……）以及它们之间的平滑过渡。 |
| `animLayer(esq, n, clip [, weight, mode])` 及同系列函数 | 在多个层（1 到 16）中同时混合多个动画，例如跑步 + 射击。 |
| `bonePoint(esq, "hueso" [, x, y, z]) → x, y, z` | 某根骨骼所在的位置（用来把武器放到手上）。 |
| `animate(mesh, n [, frame])` | 切换网格的顶点动画。 |
