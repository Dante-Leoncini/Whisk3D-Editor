# Scripts Lua

L'interpréteur **Lua 5.4** (du C pur et léger) vit dans le Core : l'éditeur et le jeu final utilisent exactement le même. Chaque script attaché à un objet tourne dans son propre état Lua, avec `inicio()` et `actualizar(dt)`, et les scripts s'échangent des données avec `setCompartido` et `compartido`.

Lua s'adapte lui aussi à chaque machine. Sur la Dreamcast, il est compilé avec des nombres sur 32 bits (`LUA_32BITS`), car le SH-4 calcule les `float` en matériel et les `double` en logiciel, très lentement. Sous Symbian, en mode C89. Le script ne s'en rend pas compte. Voir [Lua dans Whisk3D](../lua/README.md).
