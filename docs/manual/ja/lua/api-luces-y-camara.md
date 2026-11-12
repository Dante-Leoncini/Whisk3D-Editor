# ライトとカメラ

| 関数 | 説明 |
|---|---|
| `color(luz) → r, g, b`、`setColor(luz, r, g, b)` | ライトの色。 |
| `energia(luz) → e`、`setEnergia(luz, e)` | 明るさ。0〜1（ちらつかせたり、少しずつ消したりするため）。 |
| `setCamaraActiva(cam)` | ゲームを映すカメラを切り替えます。 |
| `lenteDe(cam) → fov, cerca, lejos`、`setLente(cam [, fov, cerca, lejos])` | 視野角とクリッピング距離。 |
| `camaraXZ() → fx, fy, rx, ry` | カメラが床の上（XとY）でどちらを向いているか：前方（`fx, fy`）と右方向（`rx, ry`）。カメラに合わせてキャラクターを動かすために使います。（この名前は、床がXZだったころのものです。） |
| `pantallaDe(obj) → x, y, delante` | 3Dオブジェクトが画面上のどこに表示されるか（その上に2Dのラベルを置くため）。 |
| `setRiel(cam, curva [, offset])`、`setRielNodo`、`setMiradaRiel`、`rielDe` | カメラをカーブ（レール）に沿って動かします。 |
| `resolucion() → ancho, alto` | 3D画像の実際のピクセルサイズ。 |
