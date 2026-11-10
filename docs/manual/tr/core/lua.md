# Lua script'leri

**Lua 5.4** yorumlayıcısı (sade ve hafif C) Core'un içinde yaşar: editör ve son oyun tam olarak aynısını kullanır. Bir nesneye bağlı her script, `inicio()` ve `actualizar(dt)` ile kendi Lua durumunda çalışır ve script'ler birbirine `setCompartido` ve `compartido` ile veri aktarır.

Lua da her makineye uyarlanır. Dreamcast'te 32 bitlik sayılarla (`LUA_32BITS`) derlenir, çünkü SH-4 `float`'u donanımla, `double`'ı ise yazılımla ve çok yavaş yapar. Symbian'da C89 kipinde. Script bunun farkına varmaz. Bkz. [Whisk3D'de Lua](../lua/README.md).
