# ルーチンとメモリ

ルーチン（Propertiesパネルで組み立てる描画ステップのリスト）は、Luaが設定した値を読み取れます。各メモリは名前付きの256個の数値のブロックで、ステップからは`@nombre[i]`として読み取ります。

| 関数 | 説明 |
|---|---|
| `setMemory(name, i, v)`、`getMemory(name, i)`、`setMemories(name, i, v1, v2, ...)` | メモリの書き込みと読み取り（インデックスは**0**〜255）。 |
| `setStep(routine, i, field, ...)`、`getStep(routine, i, field)` | ルーチンのステップを変更、または読み取ります（フィールド：`"on"`、`"n"`、`"color"`、`"ref"`、`"modo"`、`"entero"`）。 |
| `step(name, ...)` | 描画ステップを実行します。ルーチンの「Call Lua」ステップから呼ばれた関数の中でだけ有効です。 |

**Repeat**が1より大きいルーチンでは、`loop`メモリで何周目かがわかります：`@loop[0]`が現在の周回（0から）、`@loop[1]`が合計です。
