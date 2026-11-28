# 時間、乱数、ユーティリティ

| 関数 | 説明 |
|---|---|
| `random() → 0..1`、`random(min, max) → integer` | 乱数。 |
| `clockMs() → ms` | ミリ秒。処理にかかる時間を測るのに使います。 |
| `unixTime() → seconds, ms` | システムの時刻をUnix時間で返します。1970年1月1日（UTC）からの秒数と、別にミリ秒（0〜999）です。 |
| `unixTimeMs() → ms`, `unixTimeSec() → seconds` | 同じUnix時間を1つの数で返します。ミリ秒（整数）か、小数付きの秒です。計測は引き算だけです：`t = unixTimeSec() - start`。64ビットのLuaで正確です（Dreamcast以外のすべてのプラットフォーム。Dreamcastでは`unixTime()`）。 |
| `date([seconds [, utc]]) → table` | Unix時間の日付と時刻（省略すると現在）。ローカル時刻、または`utc = true`でUTC：`year`、`month`、`day`、`hour`、`min`、`sec`、`wday`（1 = 日曜）、`yday`。 |
| `luaProfile(frames [, label])` | スクリプトのどの部分が遅いかを計測します（`luaProfile(0)`で停止します）。 |
| `createLine(name, points)`、`linePoint(id, s)` | エンジンが計算する、なめらかな閉じたカーブ（移動ルート）。 |

開始からの経過時間：`start()`で`unixTimeSec()`を保存し、あとで引きます（実時間）。または変数に`dt`を足していきます（ゲーム内の時間）。
