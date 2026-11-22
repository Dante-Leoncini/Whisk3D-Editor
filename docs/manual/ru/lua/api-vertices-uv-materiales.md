# Вершины, UV и материалы

| Функция | Что делает |
|---|---|
| `vertexCount(m)`, `vertexPos(m, i)`, `setVertexPos(m, i, x, y, z)` | Читать и перемещать вершины по одной. |
| `setVertices(m, {i, x, y, z, ...})` | Переместить много вершин за раз (намного быстрее). |
| `vertexGroup(m, "grupo") → {índices}` | Вершины группы (вызывать один раз, в `start`). |
| `setVertexColor`, `setVerticesColor` | Раскрашивать вершины из кода. |
| `setVertexUV`, `scrollUV(m, du, dv)`, `copyUV` | Сдвигать UV (вода, конвейерные ленты…). |
| `setTextureB`, `setTextureMix`, `getTextureMix` | Смешивать две текстуры в материале. |
