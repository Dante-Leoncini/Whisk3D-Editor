# シンプルな物理（速度と跳ね返り）

| 関数 | 説明 |
|---|---|
| `velocidad(obj, vx [, vy, vz])` / `velocidad(obj) → vx, vy, vz` | 速度を与えると、エンジンが自動で動かします。 |
| `acelerar(obj, factor [, tope])` | 速度に掛け算して加速します。 |
| `caja(obj [, x, y, z])` | 衝突ボックスのサイズ。3Dでは幅（X）、奥行き（Y）、高さ（Z）、2Dではピクセル単位の幅と高さです。 |
| `rebotar(obj, otro)`、`rebotarEn(obj, area [, lados])`、`rebotarDentro(obj, area)` | 別のオブジェクトや、領域の端での跳ね返り（跳ね返ったら`true`を返します）。 |
