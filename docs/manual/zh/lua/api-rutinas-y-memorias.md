# 例程和内存

例程（在 Properties 面板中搭建的绘制步骤列表）可以读取 Lua 写入的数值。每块内存是一个带名称的、包含 256 个数字的块，步骤以 `@nombre[i]` 的形式读取它。

| 函数 | 作用 |
|---|---|
| `setMemory(name, i, v)`, `getMemory(name, i)`, `setMemories(name, i, v1, v2, ...)` | 写入和读取内存（索引从 **0** 到 255）。 |
| `setStep(routine, i, field, ...)`, `getStep(routine, i, field)` | 修改或读取例程中的某个步骤（字段：`"on"`、`"n"`、`"color"`、`"ref"`、`"modo"`、`"entero"`）。 |
| `step(name, ...)` | 执行一个绘制步骤；只在由例程的“Call Lua”步骤调用的函数中有效。 |

在 **Repeat** 大于 1 的例程中，`loop` 内存会告诉你当前是第几轮：`@loop[0]` 是当前轮次（从 0 开始），`@loop[1]` 是总轮数。
