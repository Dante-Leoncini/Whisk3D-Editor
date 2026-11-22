# シンプルな物理（速度と跳ね返り）

| 関数 | 説明 |
|---|---|
| `getVelocity(obj, vx [, vy, vz])` / `getVelocity(obj) → vx, vy, vz` | 速度を与えると、エンジンが自動で動かします。 |
| `accelerate(obj, factor [, tope])` | 速度に掛け算して加速します。 |
| `box(obj [, x, y, z])` | 衝突ボックスのサイズ。3Dでは幅（X）、奥行き（Y）、高さ（Z）、2Dではピクセル単位の幅と高さです。 |
| `bounce(obj, other)`、`bounceOn(obj, area [, sides])`、`bounceInside(obj, area)` | 別のオブジェクトや、領域の端での跳ね返り（跳ね返ったら`true`を返します）。 |
