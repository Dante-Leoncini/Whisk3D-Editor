# 引擎何时调用你的脚本

你的脚本可以定义一些特殊名称的函数，引擎会在恰当的时机自动调用它们。这些函数都是可选的：只写你需要的就行。

| 函数 | 调用时机 |
|---|---|
| `inicio()` | 调用一次，在按下 Play 时（或者对象刚通过 `instanciar` 创建时）。用来查找对象和准备变量。 |
| `actualizar(dt)` | 每一帧都会调用，前提是对象**可见**并且位于活动场景中。`dt` 是距上一帧经过的秒数（介于 0.001 和 0.1 之间）。 |
| `alEntrar(otro, miHitbox, suHitbox)` | 当一个碰撞盒开始接触另一个碰撞盒时。 |
| `alQuedarse(otro, miHitbox, suHitbox)` | 在它们保持接触期间的每一帧。 |
| `alSalir(otro, miHitbox, suHitbox)` | 当它们不再接触时（其中一个被隐藏或停用时也会触发）。 |
| `alTocar(otro, impulso, x, y, z, nx, ny, nz)` | 当两个**刚体**发生碰撞时。`x, y, z` 是接触点，`nx, ny, nz` 是指向对方的方向。 |

碰撞盒事件会发送给碰撞盒自身的脚本，以及它的父对象的脚本。如果对方是刚体，`suHitbox` 的值为 `nil`。

## 用 dt 移动物体

始终把速度乘以 `dt`。这样，游戏在高性能 PC 和慢速手机上的运行速度都一样：

```lua
function actualizar(dt)
  mover(yo(), 3 * dt, 0, 0)   -- 每秒向 +X 移动 3 个单位
end
```

## 一帧内的执行顺序

1. 物理（速度、刚体）和碰撞事件：`alTocar`、`alSalir`、`alEntrar`、`alQuedarse`。
2. 所有脚本的 `actualizar(dt)`，按 Outliner 中的顺序执行。
3. 动画（场景、片段、骨架）。
4. 帧结束：执行待处理的 `destruir()`，加入实例化的对象，并执行请求的 `cambiarEscena()`。

## 停止游戏时

按下 Stop 后，所有脚本都会关闭，共享空间会被清空，场景会恢复到按 Play 之前的状态。如果你在游戏运行时编辑脚本，该对象会重新加载这个脚本：它的变量会归零，并再次调用 `inicio()`。
