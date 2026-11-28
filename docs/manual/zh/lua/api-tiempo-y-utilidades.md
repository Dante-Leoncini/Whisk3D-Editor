# 时间、随机数和实用工具

| 函数 | 作用 |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | 随机数。 |
| `clockMs() → ms` | 毫秒数，用来测量某件事花了多长时间。 |
| `unixTime() → seconds, ms` | 以 Unix 时间表示的系统时间：自 1970-01-01（UTC）以来的秒数，另外单独给出毫秒（0–999）。 |
| `unixTimeMs() → ms`, `unixTimeSec() → seconds` | 用一个数表示同样的 Unix 时间：毫秒（整数）或带小数的秒。测量只需相减：`t = unixTimeSec() - start`。在 64 位 Lua 中精确（除 Dreamcast 外的所有平台；在 Dreamcast 上请用 `unixTime()`）。 |
| `date([seconds [, utc]]) → table` | 某个 Unix 时间的日期和时间（不传则为现在），本地时间，或在 `utc = true` 时为 UTC：`year`、`month`、`day`、`hour`、`min`、`sec`、`wday`（1 = 星期日）、`yday`。 |
| `luaProfile(frames [, label])` | 测量脚本的哪些部分比较慢（`luaProfile(0)` 关闭它）。 |
| `createLine(name, points)`, `linePoint(id, s)` | 由引擎计算的一条平滑闭合曲线（用于路线）。 |

从开始到现在经过的时间：在 `start()` 中保存 `unixTimeSec()`，之后相减（真实时间），或者在一个变量中累加 `dt`（游戏时间）。
