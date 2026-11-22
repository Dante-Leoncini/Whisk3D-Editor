# スクリプトと通信

| 関数 | 説明 |
|---|---|
| `self() → obj` | このスクリプトの持ち主のオブジェクト。 |
| `object(prop) → obj \| nil` | `properties`の`prop`フィールドで選んだオブジェクト。 |
| `option(prop) → text` | `properties`のドロップダウンリストで選んだ選択肢。何も選んでいなければ、リストの最初のもの（デフォルト）。 |
| `property(prop) → value` | このオブジェクトが個別に持つ数値、はい/いいえ、テキスト（または宣言した初期値）。 |
| `param(prop [, def]) → número` | `property`と同じですが、常に数値を返します。存在しない場合は`def`（または0）を返します。 |
| `target() → obj \| nil` | 持ち主のオブジェクトのターゲット（"target"）。 |
| `getShared(clave) → value` | すべてのスクリプトで共有する領域を読み取ります。 |
| `setShared(clave, value)` | 共有領域に数値、はい/いいえ、テキストを書き込みます。`nil`を渡すと削除します。 |
