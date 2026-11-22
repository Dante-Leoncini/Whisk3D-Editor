# 頂点、UV、マテリアル

| 関数 | 説明 |
|---|---|
| `vertexCount(m)`、`vertexPos(m, i)`、`setVertexPos(m, i, x, y, z)` | 頂点を1つずつ読み取り、動かします。 |
| `setVertices(m, {i, x, y, z, ...})` | 多くの頂点を一度に動かします（ずっと高速です）。 |
| `vertexGroup(m, "grupo") → {índices}` | グループに属する頂点（`start`で一度だけ呼んでください）。 |
| `setVertexColor`、`setVerticesColor` | コードで頂点に色を塗ります。 |
| `setVertexUV`、`scrollUV(m, du, dv)`、`copyUV` | UVを動かします（水、ベルトコンベア…）。 |
| `setTextureB`、`setTextureMix`、`getTextureMix` | マテリアルで2つのテクスチャをブレンドします。 |
