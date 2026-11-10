# Lua 脚本

**Lua 5.4** 解释器（纯 C，轻量）位于 Core 中：编辑器和最终的游戏使用的是完全相同的解释器。挂在对象上的每个脚本都在自己的 Lua 状态中运行，带有 `inicio()` 和 `actualizar(dt)`，脚本之间用 `setCompartido` 和 `compartido` 传递数据。

Lua 也会针对每台机器进行调整。在 Dreamcast 上，它以 32 位数字编译（`LUA_32BITS`），因为 SH-4 用硬件计算 `float`，而 `double` 要用软件计算，慢得要命。在 Symbian 上，以 C89 模式编译。脚本对此毫无察觉。参见 [Whisk3D 中的 Lua](../lua/README.md)。
