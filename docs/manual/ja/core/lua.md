# Luaスクリプト

**Lua 5.4**のインタープリター（素のCで書かれた軽量なもの）はCoreの中にあります。エディターと完成したゲームは、まったく同じものを使います。オブジェクトに付けた各スクリプトは、それぞれ専用のLuaステートで`inicio()`と`actualizar(dt)`を持って動き、スクリプト同士は`setCompartido`と`compartido`でデータを受け渡します。

Luaもマシンごとに調整されています。Dreamcastでは32ビットの数値（`LUA_32BITS`）でコンパイルします。SH-4は`float`をハードウェアで、`double`をソフトウェアで処理するので、`double`はとても遅いからです。SymbianではC89モードです。スクリプトはそれを意識しません。[Whisk3DのLua](../lua/README.md)を参照してください。
