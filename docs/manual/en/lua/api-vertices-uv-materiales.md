# Vertices, UVs and materials

| Function | What it does |
|---|---|
| `vertexCount(m)`, `vertexPos(m, i)`, `setVertexPos(m, i, x, y, z)` | Read and move vertices one by one. |
| `setVertices(m, {i, x, y, z, ...})` | Move many vertices at once (much faster). |
| `vertexGroup(m, "grupo") → {índices}` | The vertices of a group (call it only once, in `start`). |
| `setVertexColor`, `setVerticesColor` | Paint vertices from code. |
| `setVertexUV`, `scrollUV(m, du, dv)`, `copyUV` | Move the UVs (water, conveyor belts…). |
| `setTextureB`, `setTextureMix`, `getTextureMix` | Blend two textures in a material. |
