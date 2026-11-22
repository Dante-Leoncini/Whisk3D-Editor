# ライトとカメラ

| 関数 | 説明 |
|---|---|
| `getColor(light) → r, g, b`、`setColor(light, r, g, b)` | ライトの色。 |
| `getEnergy(light) → e`、`setEnergy(light, e)` | 明るさ。0〜1（ちらつかせたり、少しずつ消したりするため）。 |
| `setActiveCamera(cam)` | ゲームを映すカメラを切り替えます。 |
| `getLens(cam) → fov, near, far`、`setLens(cam [, fov, near, far])` | 視野角とクリッピング距離。 |
| `cameraXZ() → fx, fy, rx, ry` | カメラが床の上（XとY）でどちらを向いているか：前方（`fx, fy`）と右方向（`rx, ry`）。カメラに合わせてキャラクターを動かすために使います。（この名前は、床がXZだったころのものです。） |
| `screenOf(obj) → x, y, front` | 3Dオブジェクトが画面上のどこに表示されるか（その上に2Dのラベルを置くため）。 |
| `setRail(cam, curve [, offset])`、`setRailNode`、`setRailLook`、`getRail` | カメラをカーブ（レール）に沿って動かします。 |
| `resolution() → width, height` | 3D画像の実際のピクセルサイズ。 |
