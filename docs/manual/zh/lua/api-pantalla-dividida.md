# 分屏（同一台设备上的两名玩家）

| 函数 | 作用 |
|---|---|
| `setVistas(n [, "lado" \| "arriba"])` | `2` 把屏幕一分为二（左右并排或上下排列）；`1` 恢复为整个屏幕。2D 界面（HUD）只绘制一次，覆盖在两个画面之上。 |
| `setVistaCamara(i, camara)` | 视图 `i`（1 或 2）使用的相机。不设置时使用活动相机。 |
| `vista() → i, n` | 正在绘制的视图（1 或 2；不在绘制中时为 0），以及共有几个视图。 |
| `antesDeVista(i)` | （由你的脚本定义。）引擎在绘制每个视图之前调用它，调用的是执行了 `setVistas` 的那个脚本。你可以在这里把朝向相机的东西（告示牌、粒子）转向这个视图自己的相机，或者隐藏这个视图不该看到的东西。不要在这里移动任何随时间推进的东西：那些要放在 `actualizar` 里。 |

每一半都是一幅完整的图像，有自己的相机：游戏画面（相机的宽高比）被一分为二。

## 每人一个手柄

`boton`、`botonApretado`、`stick` 和 `gatillo` 接受**手柄编号**（1 到 4，按第一次操作的先后顺序）：`boton("a", 2)`、`stick("izq", 1)`。不带编号时表示任意手柄（所有手柄合在一起），和以前一样。键盘没有编号：由你来分配（例如，玩家 1 用方向键和数字键，玩家 2 用 WASD 和字母键；在带 QWERTY 键盘的 Nokia 上，字母键也能用）。

```lua
function actualizar(dt)
  local x1, y1 = stick("izq", 1)
  local x2, y2 = stick("izq", 2)
  if tecla("w") then y2 = -1 end
  mover(jugador1, x1 * dt * 5, 0, y1 * dt * 5)
  mover(jugador2, x2 * dt * 5, 0, y2 * dt * 5)
end
```
