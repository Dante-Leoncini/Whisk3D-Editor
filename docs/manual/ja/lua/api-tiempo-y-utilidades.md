# 時間、乱数、ユーティリティ

| 関数 | 説明 |
|---|---|
| `random() → 0..1`、`random(min, max) → integer` | 乱数。 |
| `clockMs() → ms` | ミリ秒。処理にかかる時間を測るのに使います。 |
| `luaProfile(frames [, label])` | スクリプトのどの部分が遅いかを計測します（`luaProfile(0)`で停止します）。 |
| `createLine(name, points)`、`linePoint(id, s)` | エンジンが計算する、なめらかな閉じたカーブ（移動ルート）。 |

「開始からの経過時間」を返す関数はありません。変数に`dt`を足していってください。
