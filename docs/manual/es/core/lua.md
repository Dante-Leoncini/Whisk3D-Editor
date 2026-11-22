# Scripts Lua

El intérprete de **Lua 5.4** (C plano y liviano) vive en el Core: el editor y el juego final usan exactamente el mismo. Cada script colgado de un objeto corre en su propio estado de Lua, con `start()` y `update(dt)`, y los scripts se pasan datos con `setShared` y `shared`.

Lua también se ajusta a cada máquina. En la Dreamcast se compila con números de 32 bits (`LUA_32BITS`), porque el SH-4 hace `float` por hardware y `double` por software, lentísimo. En Symbian, en modo C89. El script no se entera. Ver [Lua en Whisk3D](../lua/README.md).
