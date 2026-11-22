# 顶点、UV 和材质

| 函数 | 作用 |
|---|---|
| `vertexCount(m)`, `vertexPos(m, i)`, `setVertexPos(m, i, x, y, z)` | 逐个读取和移动顶点。 |
| `setVertices(m, {i, x, y, z, ...})` | 一次移动大量顶点（快得多）。 |
| `vertexGroup(m, "grupo") → {índices}` | 某个组中的顶点（只调用一次，放在 `start` 中）。 |
| `setVertexColor`, `setVerticesColor` | 用代码为顶点上色。 |
| `setVertexUV`, `scrollUV(m, du, dv)`, `copyUV` | 移动 UV（水面、传送带……）。 |
| `setTextureB`, `setTextureMix`, `getTextureMix` | 在一个材质中混合两张纹理。 |
