# 键盘、手柄、触屏和鼠标

| 函数 | 作用 |
|---|---|
| `tecla(nombre) → bool` | 该键处于按下状态（只要你一直按着）。 |
| `teclaApretada(nombre) → bool` | 该键恰好在这一帧被按下（每次按下只触发一次）。 |
| `boton(nombre [, mando])`, `botonApretado(nombre [, mando])` | 同上，用于手柄按钮。传入 `mando`（1 到 4）时，检查该手柄的按钮；不传时，检查任意手柄。 |
| `stick(["izq"\|"der"] [, mando]) → x, y` | 手柄摇杆，从 -1 到 1（向上为 -1）。 |
| `gatillo(["izq"\|"der"] [, mando]) → 0..1` | 模拟扳机。 |
| `controles() → n`, `control([n]) → nombre, tipo` | 有几个手柄，以及它们的类型（`"xbox"`、`"playstation"`、`"nintendo"`、`"generico"`）。 |
| `toque() → x, y, tocando` | 屏幕上的手指（或鼠标单击）。 |
| `dedo([n]) → x, y, activo` | 多点触控：第 n 根手指（1 到 4）。 |
| `tactil() → bool` | 屏幕可以触摸：手机或平板（Android、iPhone 上的浏览器……），或任何已经触摸过屏幕的设备。用于只在需要的地方显示屏幕控件。 |
| `inclinacion() → x, y, z` | 手机的加速度计，单位为 g，坐标轴按你看到的屏幕方向（已随屏幕旋转）：`x` 表示向右转了多少，就像方向盘（角度的正弦）；竖直立起时 `y = 1`，屏幕朝上平放时 `z = 1`。设备没有加速度计时返回 `nil`。 |
| `vibrar(ms [, fuerza])` | 以 0 到 1 的 `fuerza` 振动 `ms` 毫秒：手机的振动器（Android、浏览器）和手柄的马达（PC）。在手机上，力度体现为缩短脉冲。在没有振动装置的设备上，或玩家关闭了振动时，不起作用。 |
| `apretado(obj) → bool` | 该 2D 元素在这一帧被点击或触摸（用于按钮）。 |
| `raton() → x, y, dentro` | 鼠标所在的位置。 |
| `capturarRaton(bool)`, `ratonRel() → dx, dy`, `ratonCapturado()` | 捕获鼠标，用鼠标控制视角（第一人称游戏）。适用于 PC 和浏览器；在浏览器中，鼠标会在游戏中第一次单击或按键时被捕获。<kbd>Esc</kbd> 会释放鼠标并暂停游戏。 |
