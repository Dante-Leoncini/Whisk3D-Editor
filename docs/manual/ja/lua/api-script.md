# スクリプトと通信

| 関数 | 説明 |
|---|---|
| `yo() → obj` | このスクリプトの持ち主のオブジェクト。 |
| `objeto(prop) → obj \| nil` | `propiedades`の`prop`フィールドで選んだオブジェクト。 |
| `opcion(prop) → texto` | `propiedades`のドロップダウンリストで選んだ選択肢。何も選んでいなければ、リストの最初のもの（デフォルト）。 |
| `propiedad(prop) → valor` | このオブジェクトが個別に持つ数値、はい/いいえ、テキスト（または宣言した初期値）。 |
| `parametro(prop [, def]) → número` | `propiedad`と同じですが、常に数値を返します。存在しない場合は`def`（または0）を返します。 |
| `objetivo() → obj \| nil` | 持ち主のオブジェクトのターゲット（"target"）。 |
| `compartido(clave) → valor` | すべてのスクリプトで共有する領域を読み取ります。 |
| `setCompartido(clave, valor)` | 共有領域に数値、はい/いいえ、テキストを書き込みます。`nil`を渡すと削除します。 |
