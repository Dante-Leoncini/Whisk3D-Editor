# 3Dでの移動・回転・拡大縮小

| 関数 | 説明 |
|---|---|
| `getPosition(obj) → x, y, z` | 位置。 |
| `setPosition(obj, x, y, z)` | 正確な位置に置きます（物理を無視します）。 |
| `move(obj, dx, dy, dz)` | 現在の位置から動かします（`dt`と一緒に使ってください）。 |
| `getRotation(obj) → x, y, z` | 回転（度）。 |
| `setRotation(obj, x, y, z [, order])` | 正確な回転で、Transformカードの回転と同じです。`order`で軸を適用する順番を選べます。たとえば`"ZXY"`です（乗り物のように、まず垂直軸まわりの向きから）。 |
| `getRotationX(obj)`, `getRotationY(obj)`, `getRotationZ(obj) → 度` | 1つの軸だけの回転。`getRotationZ` は縦軸まわりの回転です。 |
| `setRotationX(obj, g)`, `setRotationY(obj, g)`, `setRotationZ(obj, g)` | 1つの軸だけを変えます。ほかの2つはそのままです。たとえば `setRotationZ(obj, getRotationZ(obj) + 90 * dt)` で縦軸まわりに回ります。 |
| `rotate(obj, dx, dy, dz)` | オブジェクト自身の軸まわりに回転させます。 |
| `rotateTowards(obj, dx, dy [, factor])` | 床の上（XとY）のある方向に向けて、なめらかに回転させていきます。 |
| `lookAt(obj, x, y, z)` | ある点の方へ向けます（カメラの正面）。 |
| `getScale(obj) → sx, sy, sz` | スケール。 |
| `setScale(obj, s [, sy, sz])` | 正確なスケール。数値を1つだけ渡すと均等に拡大縮小します。 |
| `scale(obj, f)` | 現在のスケールに掛け算します。 |
| `worldPoint(obj, x, y, z) → x, y, z` | オブジェクト内の点をワールド座標に変換します。 |
| `isVisible(obj) → bool`、`setVisible(obj, bool)` | 表示・非表示。非表示のオブジェクトには`update`が呼ばれなくなります。 |
| `setTint(obj, r, g, b [, a])` | 色を変えます（0〜1の値）。 |
