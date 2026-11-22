# 剛体、衝突、ヒットボックス

| 関数 | 説明 |
|---|---|
| `physicsVel`、`physicsAngVel`、`physicsImpulse`、`physicsForce` | 剛体の速度、回転、押し出し（インパルス）、力。 |
| `physicsGravity([g])`、`physicsActive(obj, bool)`、`physicsAwake(obj)` | 重力（初期値は9.81）、オブジェクトの物理のオン・オフ。 |
| `physicsRay(ox, oy, oz, dx, dy, dz, max) → dist, nx, ny, nz, obj` | レイを飛ばし、何に当たるかを返します。 |
| `contacts() → list` | このフレームのすべての衝突。 |
| `colGround(mesh, x, y, z)`、`colWall(...)`、`colRay(...)` | メッシュの三角形との衝突（地面の上に立つ、壁を通り抜けない）。 |
| `hitboxActive`、`setHitboxActive`、`setHitboxSize`、`hitboxInside`、`hitboxTouching` | ヒットボックスを操作し、中に何があるかを調べます。 |
| `showHitboxes(bool)` | デバッグ用にヒットボックスを描画します。 |
