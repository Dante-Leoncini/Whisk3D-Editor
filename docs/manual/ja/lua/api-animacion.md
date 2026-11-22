# アニメーション

| 関数 | 説明 |
|---|---|
| `animScene(name [, loop])`、`stopScene()`、`animSceneFrame(f)` | シーン全体のアニメーション。 |
| `animObject(root, "clip" [, loop])`、`animObjectStop`、`animObjectFinished`、`animObjectSpeed` | オブジェクトとその子のアニメーションクリップ。 |
| `animClip(armature, clip [, loop])`、`animFinished`、`animSpeed`、`animTransition` | アーマチュアのアニメーション（歩く、ジャンプする…）と、その間のなめらかな切り替え。 |
| `animLayer(esq, n, clip [, weight, mode])`とその関連関数 | 複数のアニメーションをレイヤー（1〜16）で同時にブレンドします。たとえば、走る + 撃つ。 |
| `bonePoint(esq, "hueso" [, x, y, z]) → x, y, z` | ボーンの位置（手に武器を持たせるときなど）。 |
| `animate(mesh, n [, frame])` | メッシュの頂点アニメーションを切り替えます。 |
