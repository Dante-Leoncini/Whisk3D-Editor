# 时间、随机数和实用工具

| 函数 | 作用 |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | 随机数。 |
| `clockMs() → ms` | 毫秒数，用来测量某件事花了多长时间。 |
| `luaProfile(frames [, label])` | 测量脚本的哪些部分比较慢（`luaProfile(0)` 关闭它）。 |
| `createLine(name, points)`, `linePoint(id, s)` | 由引擎计算的一条平滑闭合曲线（用于路线）。 |

没有“从开始到现在经过的时间”这样的函数：请在一个变量中累加 `dt`。
