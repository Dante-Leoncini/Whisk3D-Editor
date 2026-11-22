# Vertici, UV e materiali

| Funzione | Cosa fa |
|---|---|
| `vertexCount(m)`, `vertexPos(m, i)`, `setVertexPos(m, i, x, y, z)` | Leggere e spostare i vertici uno per uno. |
| `setVertices(m, {i, x, y, z, ...})` | Spostare molti vertici in una volta (molto più veloce). |
| `vertexGroup(m, "grupo") → {índices}` | I vertici di un gruppo (chiamala una sola volta, in `start`). |
| `setVertexColor`, `setVerticesColor` | Colorare i vertici da codice. |
| `setVertexUV`, `scrollUV(m, du, dv)`, `copyUV` | Spostare le UV (acqua, nastri trasportatori…). |
| `setTextureB`, `setTextureMix`, `getTextureMix` | Mescolare due texture in un materiale. |
