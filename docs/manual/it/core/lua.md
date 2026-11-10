# Script Lua

L'interprete di **Lua 5.4** (C semplice e leggero) vive nel Core: l'editor e il gioco finale usano esattamente lo stesso. Ogni script collegato a un oggetto gira nel proprio stato Lua, con `inicio()` e `actualizar(dt)`, e gli script si scambiano dati con `setCompartido` e `compartido`.

Anche Lua si adatta a ogni macchina. Sul Dreamcast si compila con numeri a 32 bit (`LUA_32BITS`), perché l'SH-4 calcola i `float` in hardware e i `double` in software, lentissimo. Su Symbian, in modalità C89. Lo script non se ne accorge. Vedi [Lua in Whisk3D](../lua/README.md).
