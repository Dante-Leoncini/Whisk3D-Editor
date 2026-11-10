# Lua scripts

The **Lua 5.4** interpreter (plain, lightweight C) lives in the Core: the editor and the final game use exactly the same one. Each script attached to an object runs in its own Lua state, with `inicio()` and `actualizar(dt)`, and scripts pass data to each other with `setCompartido` and `compartido`.

Lua also adapts to each machine. On the Dreamcast it's compiled with 32-bit numbers (`LUA_32BITS`), because the SH-4 does `float` in hardware and `double` in software, painfully slowly. On Symbian, in C89 mode. The script never notices. See [Lua in Whisk3D](../lua/README.md).
