# Scripts Lua

O interpretador de **Lua 5.4** (C puro e leve) vive no Core: o editor e o jogo final usam exatamente o mesmo. Cada script ligado a um objeto roda no seu próprio estado de Lua, com `inicio()` e `actualizar(dt)`, e os scripts trocam dados com `setCompartido` e `compartido`.

O Lua também se ajusta a cada máquina. No Dreamcast, é compilado com números de 32 bits (`LUA_32BITS`), porque o SH-4 faz `float` por hardware e `double` por software, lentíssimo. No Symbian, em modo C89. O script nem percebe. Veja [Lua no Whisk3D](../lua/README.md).
