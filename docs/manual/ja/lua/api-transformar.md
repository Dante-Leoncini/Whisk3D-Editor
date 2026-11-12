# 3Dでの移動・回転・拡大縮小

| 関数 | 説明 |
|---|---|
| `posicion(obj) → x, y, z` | 位置。 |
| `setPosicion(obj, x, y, z)` | 正確な位置に置きます（物理を無視します）。 |
| `mover(obj, dx, dy, dz)` | 現在の位置から動かします（`dt`と一緒に使ってください）。 |
| `rotacion(obj) → x, y, z` | 回転（度）。 |
| `setRotacion(obj, x, y, z [, orden])` | 正確な回転で、Transformカードの回転と同じです。`orden`で軸を適用する順番を選べます。たとえば`"ZXY"`です（乗り物のように、まず垂直軸まわりの向きから）。 |
| `rotacionX(obj)`, `rotacionY(obj)`, `rotacionZ(obj) → 度` | 1つの軸だけの回転。`rotacionZ` は縦軸まわりの回転です。 |
| `setRotacionX(obj, g)`, `setRotacionY(obj, g)`, `setRotacionZ(obj, g)` | 1つの軸だけを変えます。ほかの2つはそのままです。たとえば `setRotacionZ(obj, rotacionZ(obj) + 90 * dt)` で縦軸まわりに回ります。 |
| `girar(obj, dx, dy, dz)` | オブジェクト自身の軸まわりに回転させます。 |
| `girarHacia(obj, dx, dy [, factor])` | 床の上（XとY）のある方向に向けて、なめらかに回転させていきます。 |
| `mirarA(obj, x, y, z)` | ある点の方へ向けます（カメラの正面）。 |
| `escala(obj) → sx, sy, sz` | スケール。 |
| `setEscala(obj, s [, sy, sz])` | 正確なスケール。数値を1つだけ渡すと均等に拡大縮小します。 |
| `escalar(obj, f)` | 現在のスケールに掛け算します。 |
| `puntoMundo(obj, x, y, z) → x, y, z` | オブジェクト内の点をワールド座標に変換します。 |
| `visible(obj) → bool`、`setVisible(obj, bool)` | 表示・非表示。非表示のオブジェクトには`actualizar`が呼ばれなくなります。 |
| `setTinte(obj, r, g, b [, a])` | 色を変えます（0〜1の値）。 |
